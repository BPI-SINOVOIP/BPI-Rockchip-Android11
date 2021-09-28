package org.jetbrains.dokka

import com.intellij.psi.*
import com.intellij.psi.impl.source.javadoc.CorePsiDocTagValueImpl
import com.intellij.psi.impl.source.tree.JavaDocElementType
import com.intellij.psi.javadoc.*
import com.intellij.psi.util.PsiTreeUtil
import com.intellij.util.IncorrectOperationException
import com.intellij.util.containers.isNullOrEmpty
import org.jetbrains.dokka.Model.CodeNode
import org.jetbrains.kotlin.utils.join
import org.jetbrains.kotlin.utils.keysToMap
import org.jsoup.Jsoup
import org.jsoup.nodes.Element
import org.jsoup.nodes.Node
import org.jsoup.nodes.TextNode
import java.io.File
import java.net.URI
import java.util.regex.Pattern

private val NAME_TEXT = Pattern.compile("(\\S+)(.*)", Pattern.DOTALL)
private val TEXT = Pattern.compile("(\\S+)\\s*(.*)", Pattern.DOTALL)

data class JavadocParseResult(
    val content: Content,
    val deprecatedContent: Content?,
    val attributeRefs: List<String>,
    val apiLevel: DocumentationNode? = null,
    val deprecatedLevel: DocumentationNode? = null,
    val artifactId: DocumentationNode? = null,
    val attribute: DocumentationNode? = null
) {
    companion object {
        val Empty = JavadocParseResult(Content.Empty,
            null,
            emptyList(),
            null,
            null,
            null
        )
    }
}

interface JavaDocumentationParser {
    fun parseDocumentation(element: PsiNamedElement): JavadocParseResult
}

class JavadocParser(
    private val refGraph: NodeReferenceGraph,
    private val logger: DokkaLogger,
    private val signatureProvider: ElementSignatureProvider,
    private val externalDocumentationLinkResolver: ExternalDocumentationLinkResolver
) : JavaDocumentationParser {

    private fun ContentSection.appendTypeElement(
        signature: String,
        selector: (DocumentationNode) -> DocumentationNode?
    ) {
        append(LazyContentBlock {
            val node = refGraph.lookupOrWarn(signature, logger)?.let(selector)
            if (node != null) {
                it.append(NodeRenderContent(node, LanguageService.RenderMode.SUMMARY))
                it.symbol(":")
                it.text(" ")
            }
        })
    }

    override fun parseDocumentation(element: PsiNamedElement): JavadocParseResult {
        val docComment = (element as? PsiDocCommentOwner)?.docComment
        if (docComment == null) return JavadocParseResult.Empty
        val result = MutableContent()
        var deprecatedContent: Content? = null
        val firstParagraph = ContentParagraph()
        firstParagraph.convertJavadocElements(
            docComment.descriptionElements.dropWhile { it.text.trim().isEmpty() },
            element
        )
        val paragraphs = firstParagraph.children.dropWhile { it !is ContentParagraph }
        firstParagraph.children.removeAll(paragraphs)
        if (!firstParagraph.isEmpty()) {
            result.append(firstParagraph)
        }
        paragraphs.forEach {
            result.append(it)
        }

        if (element is PsiMethod) {
            val tagsByName = element.searchInheritedTags()
            for ((tagName, tags) in tagsByName) {
                for ((tag, context) in tags) {
                    val section = result.addSection(javadocSectionDisplayName(tagName), tag.getSubjectName())
                    val signature = signatureProvider.signature(element)
                    when (tagName) {
                        "param" -> {
                            section.appendTypeElement(signature) {
                                it.details
                                    .find { node -> node.kind == NodeKind.Parameter && node.name == tag.getSubjectName() }
                                    ?.detailOrNull(NodeKind.Type)
                            }
                        }
                        "return" -> {
                            section.appendTypeElement(signature) { it.detailOrNull(NodeKind.Type) }
                        }
                    }
                    section.convertJavadocElements(tag.contentElements(), context)
                }
            }
        }

        val attrRefSignatures = mutableListOf<String>()
        var since: DocumentationNode? = null
        var deprecated: DocumentationNode? = null
        var artifactId: DocumentationNode? = null
        var attrName: String? = null
        var attrDesc: Content? = null
        var attr: DocumentationNode? = null
        docComment.tags.forEach { tag ->
            when (tag.name.toLowerCase()) {
                "see" -> result.convertSeeTag(tag)
                "deprecated" -> {
                    deprecatedContent = Content().apply {
                        convertJavadocElements(tag.contentElements(), element)
                    }
                }
                "attr" -> {
                    when (tag.valueElement?.text) {
                        "ref" ->
                            tag.getAttrRef(element)?.let {
                                attrRefSignatures.add(it)
                            }
                        "name" -> attrName = tag.getAttrName()
                        "description" -> attrDesc = tag.getAttrDesc(element)
                    }
                }
                "since", "apisince" -> {
                    since = DocumentationNode(tag.getApiLevel() ?: "", Content.Empty, NodeKind.ApiLevel)
                }
                "deprecatedsince" -> {
                    deprecated = DocumentationNode(tag.getApiLevel() ?: "", Content.Empty, NodeKind.DeprecatedLevel)
                }
                "artifactid" -> {
                    artifactId = DocumentationNode(tag.artifactId() ?: "", Content.Empty, NodeKind.ArtifactId)
                }
                in tagsToInherit -> {
                }
                else -> {
                    val subjectName = tag.getSubjectName()
                    val section = result.addSection(javadocSectionDisplayName(tag.name), subjectName)
                    section.convertJavadocElements(tag.contentElements(), element)
                }
            }
        }
        attrName?.let { name ->
            attr = DocumentationNode(name, attrDesc ?: Content.Empty, NodeKind.AttributeRef)
        }
        return JavadocParseResult(result, deprecatedContent, attrRefSignatures, since, deprecated, artifactId, attr)
    }

    private val tagsToInherit = setOf("param", "return", "throws")

    private data class TagWithContext(val tag: PsiDocTag, val context: PsiNamedElement)

    fun PsiDocTag.artifactId(): String? {
        var artifactName: String? = null
        if (dataElements.isNotEmpty()) {
            artifactName = join(dataElements.map { it.text }, "")
        }
        return artifactName
    }

    fun PsiDocTag.getApiLevel(): String? {
        if (dataElements.isNotEmpty()) {
            val data = dataElements
            if (data[0] is CorePsiDocTagValueImpl) {
                val docTagValue = data[0]
                if (docTagValue.firstChild != null) {
                    val apiLevel = docTagValue.firstChild
                    return apiLevel.text
                }
            }
        }
        return null
    }

    private fun PsiDocTag.getAttrRef(element: PsiNamedElement): String? {
        if (dataElements.size > 1) {
            val elementText = dataElements[1].text
            try {
                val linkComment = JavaPsiFacade.getInstance(project).elementFactory
                    .createDocCommentFromText("/** {@link $elementText} */", element)
                val linkElement = PsiTreeUtil.getChildOfType(linkComment, PsiInlineDocTag::class.java)?.linkElement()
                val signature = resolveInternalLink(linkElement)
                val attrSignature = "AttrMain:$signature"
                return attrSignature
            } catch (e: IncorrectOperationException) {
                return null
            }
        } else return null
    }

    private fun PsiDocTag.getAttrName(): String? {
        if (dataElements.size > 1) {
            val nameMatcher = NAME_TEXT.matcher(dataElements[1].text)
            if (nameMatcher.matches()) {
                return nameMatcher.group(1)
            } else {
                return null
            }
        } else return null
    }

    private fun PsiDocTag.getAttrDesc(element: PsiNamedElement): Content? {
        return Content().apply {
            convertJavadocElementsToAttrDesc(contentElements(), element)
        }
    }

    private fun PsiMethod.searchInheritedTags(): Map<String, Collection<TagWithContext>> {

        val output = tagsToInherit.keysToMap { mutableMapOf<String?, TagWithContext>() }

        fun recursiveSearch(methods: Array<PsiMethod>) {
            for (method in methods) {
                recursiveSearch(method.findSuperMethods())
            }
            for (method in methods) {
                for (tag in method.docComment?.tags.orEmpty()) {
                    if (tag.name in tagsToInherit) {
                        output[tag.name]!![tag.getSubjectName()] = TagWithContext(tag, method)
                    }
                }
            }
        }

        recursiveSearch(arrayOf(this))
        return output.mapValues { it.value.values }
    }


    private fun PsiDocTag.contentElements(): Iterable<PsiElement> {
        val tagValueElements = children
            .dropWhile { it.node?.elementType == JavaDocTokenType.DOC_TAG_NAME }
            .dropWhile { it is PsiWhiteSpace }
            .filterNot { it.node?.elementType == JavaDocTokenType.DOC_COMMENT_LEADING_ASTERISKS }
        return if (getSubjectName() != null) tagValueElements.dropWhile { it is PsiDocTagValue } else tagValueElements
    }

    private fun ContentBlock.convertJavadocElements(elements: Iterable<PsiElement>, element: PsiNamedElement) {
        val doc = Jsoup.parse(expandAllForElements(elements, element))
        doc.body().childNodes().forEach {
            convertHtmlNode(it)?.let { append(it) }
        }
        doc.head().childNodes().forEach {
            convertHtmlNode(it)?.let { append(it) }
        }
    }

    private fun ContentBlock.convertJavadocElementsToAttrDesc(elements: Iterable<PsiElement>, element: PsiNamedElement) {
        val doc = Jsoup.parse(expandAllForElements(elements, element))
        doc.body().childNodes().forEach {
            convertHtmlNode(it)?.let {
                var content = it
                if (content is ContentText) {
                    var description = content.text
                    val matcher = TEXT.matcher(content.text)
                    if (matcher.matches()) {
                        val command = matcher.group(1)
                        if (command == "description") {
                            description = matcher.group(2)
                            content = ContentText(description)
                        }
                    }
                }
                append(content)
            }
        }
    }

    private fun expandAllForElements(elements: Iterable<PsiElement>, element: PsiNamedElement): String {
        val htmlBuilder = StringBuilder()
        elements.forEach {
            if (it is PsiInlineDocTag) {
                htmlBuilder.append(convertInlineDocTag(it, element))
            } else {
                htmlBuilder.append(it.text)
            }
        }
        return htmlBuilder.toString().trim()
    }

    private fun convertHtmlNode(node: Node, isBlockCode: Boolean = false): ContentNode? {
        if (isBlockCode) {
            return if (node is TextNode) { // Fixes b/129762453
                val codeNode = CodeNode(node.wholeText, "")
                ContentText(codeNode.text().removePrefix("#"))
            } else { // Fixes b/129857975
                ContentText(node.toString())
            }
        }
        if (node is TextNode) {
            return ContentText(node.text().removePrefix("#"))
        } else if (node is Element) {
            val childBlock = createBlock(node)
            node.childNodes().forEach {
                val child = convertHtmlNode(it, isBlockCode = childBlock is ContentBlockCode)
                if (child != null) {
                    childBlock.append(child)
                }
            }
            return (childBlock)
        }
        return null
    }

    private fun createBlock(element: Element): ContentBlock = when (element.tagName()) {
        "p" -> ContentParagraph()
        "b", "strong" -> ContentStrong()
        "i", "em" -> ContentEmphasis()
        "s", "del" -> ContentStrikethrough()
        "code" -> ContentCode()
        "pre" -> ContentBlockCode()
        "ul" -> ContentUnorderedList()
        "ol" -> ContentOrderedList()
        "li" -> ContentListItem()
        "a" -> createLink(element)
        "br" -> ContentBlock().apply { hardLineBreak() }

        "dl" -> ContentDescriptionList()
        "dt" -> ContentDescriptionTerm()
        "dd" -> ContentDescriptionDefinition()

        "table" -> ContentTable()
        "tbody" -> ContentTableBody()
        "tr" -> ContentTableRow()
        "th" -> {
            val colspan = element.attr("colspan")
            val rowspan = element.attr("rowspan")
            ContentTableHeader(colspan, rowspan)
        }
        "td" -> {
            val colspan = element.attr("colspan")
            val rowspan = element.attr("rowspan")
            ContentTableCell(colspan, rowspan)
        }

        "h1" -> ContentHeading(1)
        "h2" -> ContentHeading(2)
        "h3" -> ContentHeading(3)
        "h4" -> ContentHeading(4)
        "h5" -> ContentHeading(5)
        "h6" -> ContentHeading(6)

        "div" -> {
            val divClass = element.attr("class")
            if (divClass == "special reference" || divClass == "note") ContentSpecialReference()
            else ContentParagraph()
        }

        "script" -> {

            // If the `type` attr is an empty string, we want to use null instead so that the resulting generated
            // Javascript does not contain a `type` attr.
            //
            // Example:
            // type == ""   => <script type="" src="...">
            // type == null => <script src="...">
            val type = if (element.attr("type").isNotEmpty()) {
                element.attr("type")
            } else {
                null
            }
            ScriptBlock(type, element.attr("src"))
        }

        else -> ContentBlock()
    }

    private fun createLink(element: Element): ContentBlock {
        return when {
            element.hasAttr("docref") -> {
                val docref = element.attr("docref")
                ContentNodeLazyLink(docref, { -> refGraph.lookupOrWarn(docref, logger) })
            }
            element.hasAttr("href") -> {
                val href = element.attr("href")

                val uri = try {
                    URI(href)
                } catch (_: Exception) {
                    null
                }

                if (uri?.isAbsolute == false) {
                    ContentLocalLink(href)
                } else {
                    ContentExternalLink(href)
                }
            }
            element.hasAttr("name") -> {
                ContentBookmark(element.attr("name"))
            }
            else -> ContentBlock()
        }
    }

    private fun MutableContent.convertSeeTag(tag: PsiDocTag) {
        val linkElement = tag.linkElement() ?: return
        val seeSection = findSectionByTag(ContentTags.SeeAlso) ?: addSection(ContentTags.SeeAlso, null)

        val valueElement = tag.referenceElement()
        val externalLink = resolveExternalLink(valueElement)
        val text = ContentText(linkElement.text)

        val linkSignature by lazy { resolveInternalLink(valueElement) }
        val node = when {
            externalLink != null -> {
                val linkNode = ContentExternalLink(externalLink)
                linkNode.append(text)
                linkNode
            }
            linkSignature != null -> {
                val linkNode =
                    ContentNodeLazyLink(
                        (tag.valueElement ?: linkElement).text,
                        { -> refGraph.lookupOrWarn(linkSignature, logger) }
                    )
                linkNode.append(text)
                linkNode
            }
            else -> text
        }
        seeSection.append(node)
    }

    private fun convertInlineDocTag(tag: PsiInlineDocTag, element: PsiNamedElement) = when (tag.name) {
        "link", "linkplain" -> {
            val valueElement = tag.referenceElement()
            val externalLink = resolveExternalLink(valueElement)
            val linkSignature by lazy { resolveInternalLink(valueElement) }
            if (externalLink != null || linkSignature != null) {
                val labelText = tag.dataElements.firstOrNull { it is PsiDocToken }?.text ?: valueElement!!.text
                val linkTarget = if (externalLink != null) "href=\"$externalLink\"" else "docref=\"$linkSignature\""
                val link = "<a $linkTarget>$labelText</a>"
                if (tag.name == "link") "<code>$link</code>" else link
            } else if (valueElement != null) {
                valueElement.text
            } else {
                ""
            }
        }
        "code", "literal" -> {
            val text = StringBuilder()
            tag.dataElements.forEach { text.append(it.text) }
            val escaped = text.toString().trimStart().htmlEscape()
            if (tag.name == "code") "<code>$escaped</code>" else escaped
        }
        "inheritDoc" -> {
            val result = (element as? PsiMethod)?.let {
                // @{inheritDoc} is only allowed on functions
                val parent = tag.parent
                when (parent) {
                    is PsiDocComment -> element.findSuperDocCommentOrWarn()
                    is PsiDocTag -> element.findSuperDocTagOrWarn(parent)
                    else -> null
                }
            }
            result ?: tag.text
        }
        "docRoot" -> {
            // TODO: fix that
            "https://developer.android.com/"
        }
        "sample" -> {
            tag.text?.let { tagText ->
                val (absolutePath, delimiter) = getSampleAnnotationInformation(tagText)
                val code = retrieveCodeInFile(absolutePath, delimiter)
                return if (code != null && code.isNotEmpty()) {
                    "<pre is-upgraded>$code</pre>"
                } else {
                    ""
                }
            }
        }

        // Loads MathJax script from local source, which then updates MathJax HTML code
        "usesMathJax" -> {
            "<script src=\"/_static/js/managed/mathjax/MathJax.js?config=TeX-AMS_SVG\"></script>"
        }

        else -> tag.text
    }

    private fun PsiDocTag.referenceElement(): PsiElement? =
        linkElement()?.let {
            if (it.node.elementType == JavaDocElementType.DOC_REFERENCE_HOLDER) {
                PsiTreeUtil.findChildOfType(it, PsiJavaCodeReferenceElement::class.java)
            } else {
                it
            }
        }

    private fun PsiDocTag.linkElement(): PsiElement? =
        valueElement ?: dataElements.firstOrNull { it !is PsiWhiteSpace }

    private fun resolveExternalLink(valueElement: PsiElement?): String? {
        val target = valueElement?.reference?.resolve()
        if (target != null) {
            return externalDocumentationLinkResolver.buildExternalDocumentationLink(target)
        }
        return null
    }

    private fun resolveInternalLink(valueElement: PsiElement?): String? {
        val target = valueElement?.reference?.resolve()
        if (target != null) {
            return signatureProvider.signature(target)
        }
        return null
    }

    fun PsiDocTag.getSubjectName(): String? {
        if (name == "param" || name == "throws" || name == "exception") {
            return valueElement?.text
        }
        return null
    }

    private fun PsiMethod.findSuperDocCommentOrWarn(): String {
        val method = findFirstSuperMethodWithDocumentation(this)
        if (method != null) {
            val descriptionElements = method.docComment?.descriptionElements?.dropWhile {
                it.text.trim().isEmpty()
            } ?: return ""

            return expandAllForElements(descriptionElements, method)
        }
        logger.warn("No docs found on supertype with {@inheritDoc} method ${this.name} in ${this.containingFile.name}:${this.lineNumber()}")
        return ""
    }


    private fun PsiMethod.findSuperDocTagOrWarn(elementToExpand: PsiDocTag): String {
        val result = findFirstSuperMethodWithDocumentationforTag(elementToExpand, this)

        if (result != null) {
            val (method, tag) = result

            val contentElements = tag.contentElements().dropWhile { it.text.trim().isEmpty() }

            val expandedString = expandAllForElements(contentElements, method)

            return expandedString
        }
        logger.warn("No docs found on supertype for @${elementToExpand.name} ${elementToExpand.getSubjectName()} with {@inheritDoc} method ${this.name} in ${this.containingFile.name}:${this.lineNumber()}")
        return ""
    }

    private fun findFirstSuperMethodWithDocumentation(current: PsiMethod): PsiMethod? {
        val superMethods = current.findSuperMethods()
        for (method in superMethods) {
            val docs = method.docComment?.descriptionElements?.dropWhile { it.text.trim().isEmpty() }
            if (!docs.isNullOrEmpty()) {
                return method
            }
        }
        for (method in superMethods) {
            val result = findFirstSuperMethodWithDocumentation(method)
            if (result != null) {
                return result
            }
        }

        return null
    }

    private fun findFirstSuperMethodWithDocumentationforTag(
        elementToExpand: PsiDocTag,
        current: PsiMethod
    ): Pair<PsiMethod, PsiDocTag>? {
        val superMethods = current.findSuperMethods()
        val mappedFilteredTags = superMethods.map {
            it to it.docComment?.tags?.filter { it.name == elementToExpand.name }
        }

        for ((method, tags) in mappedFilteredTags) {
            tags ?: continue
            for (tag in tags) {
                val (tagSubject, elementSubject) = when (tag.name) {
                    "throws" -> {
                        // match class names only for throws, ignore possibly fully qualified path
                        // TODO: Always match exactly here
                        tag.getSubjectName()?.split(".")?.last() to elementToExpand.getSubjectName()?.split(".")?.last()
                    }
                    else -> {
                        tag.getSubjectName() to elementToExpand.getSubjectName()
                    }
                }

                if (tagSubject == elementSubject) {
                    return method to tag
                }
            }
        }

        for (method in superMethods) {
            val result = findFirstSuperMethodWithDocumentationforTag(elementToExpand, method)
            if (result != null) {
                return result
            }
        }
        return null
    }

    /**
     * Returns information inside @sample
     *
     * Component1 is the absolute path to the file
     * Component2 is the delimiter if exists in the file
     */
    private fun getSampleAnnotationInformation(tagText: String): Pair<String, String> {
        val pathContent = tagText
            .trim { it == '{' || it == '}' }
            .removePrefix("@sample ")

        val formattedPath = pathContent.substringBefore(" ").trim()
        val potentialDelimiter = pathContent.substringAfterLast(" ").trim()

        val delimiter = if (potentialDelimiter == formattedPath) "" else potentialDelimiter
        val path = "samples/$formattedPath"

        return Pair(path, delimiter)
    }

    /**
     * Retrieves the code inside a file.
     *
     * If betweenTag is not empty, it retrieves the code between
     * BEGIN_INCLUDE($betweenTag) and END_INCLUDE($betweenTag) comments.
     *
     * Also, the method will trim every line with the number of spaces in the first line
     */
    private fun retrieveCodeInFile(path: String, betweenTag: String = "") = StringBuilder().apply {
            try {
                if (betweenTag.isEmpty()) {
                    appendContent(path)
                } else {
                    appendContentBetweenIncludes(path, betweenTag)
                }
            } catch (e: java.lang.Exception) {
                logger.error("No file found when processing Java @sample. Path to sample: $path")
            }
        }

    private fun StringBuilder.appendContent(path: String) {
        val spaces = InitialSpaceIndent()
        File(path).forEachLine {
            appendWithoutInitialIndent(it, spaces)
        }
    }

    private fun StringBuilder.appendContentBetweenIncludes(path: String, includeTag: String) {
        var shouldAppend = false
        val beginning = "BEGIN_INCLUDE($includeTag)"
        val end = "END_INCLUDE($includeTag)"
        val spaces = InitialSpaceIndent()
        File(path).forEachLine {
            if (shouldAppend) {
                if (it.contains(end)) {
                    shouldAppend = false
                } else {
                    appendWithoutInitialIndent(it, spaces)
                }
            } else {
                if (it.contains(beginning)) shouldAppend = true
            }
        }
    }

    private fun StringBuilder.appendWithoutInitialIndent(it: String, spaces: InitialSpaceIndent) {
        if (spaces.value == -1) {
            spaces.value = (it.length - it.trimStart().length).coerceAtLeast(0)
            appendln(it)
        } else {
            appendln(if (it.isBlank()) it else it.substring(spaces.value, it.length))
        }
    }

    private data class InitialSpaceIndent(var value: Int = -1)
}
