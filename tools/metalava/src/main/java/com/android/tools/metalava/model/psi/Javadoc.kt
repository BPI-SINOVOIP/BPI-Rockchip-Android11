/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.tools.metalava.model.psi

import com.android.tools.metalava.doclava1.Issues
import com.android.tools.metalava.model.ClassItem
import com.android.tools.metalava.model.Item
import com.android.tools.metalava.model.PackageItem
import com.android.tools.metalava.reporter
import com.intellij.psi.JavaDocTokenType
import com.intellij.psi.JavaPsiFacade
import com.intellij.psi.PsiClass
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiJavaCodeReferenceElement
import com.intellij.psi.PsiMember
import com.intellij.psi.PsiMethod
import com.intellij.psi.PsiReference
import com.intellij.psi.PsiTypeParameter
import com.intellij.psi.PsiWhiteSpace
import com.intellij.psi.impl.source.SourceTreeToPsiMap
import com.intellij.psi.impl.source.javadoc.PsiDocMethodOrFieldRef
import com.intellij.psi.impl.source.tree.CompositePsiElement
import com.intellij.psi.impl.source.tree.JavaDocElementType
import com.intellij.psi.javadoc.PsiDocComment
import com.intellij.psi.javadoc.PsiDocTag
import com.intellij.psi.javadoc.PsiDocToken
import com.intellij.psi.javadoc.PsiInlineDocTag
import org.intellij.lang.annotations.Language

/*
 * Various utilities for handling javadoc, such as
 * merging comments into existing javadoc sections,
 * rewriting javadocs into fully qualified references, etc.
 *
 * TODO: Handle KDoc
 */

/**
 * If true, we'll rewrite all the javadoc documentation in doc stubs
 * to include fully qualified names
 */
const val EXPAND_DOCUMENTATION = true

/**
 * If the reference is to a class in the same package, include the package prefix?
 * This should not be necessary, but doclava has problems finding classes without
 * it. Consider turning this off when we switch to Dokka.
 */
const val INCLUDE_SAME_PACKAGE = true

/** If documentation starts with hash, insert the implicit class? */
const val PREPEND_LOCAL_CLASS = false

/**
 * Whether we should report unresolved symbols. This is typically
 * a bug in the documentation. It looks like there are a LOT
 * of mistakes right now, so I'm worried about turning this on
 * since doclava didn't seem to abort on this.
 *
 * Here are some examples I've spot checked:
 * (1) "Unresolved SQLExceptionif": In java.sql.CallableStatement the
 * getBigDecimal method contains this, presumably missing a space
 * before the if suffix: "@exception SQLExceptionif parameterName does not..."
 * (2) In android.nfc.tech.IsoDep there is "@throws TagLostException if ..."
 * but TagLostException is not imported anywhere and is not in the same
 * package (it's in the parent package).
 */
const val REPORT_UNRESOLVED_SYMBOLS = false

/**
 * Merges the given [newText] into the existing documentation block [existingDoc]
 * (which should be a full documentation node, including the surrounding comment
 * start and end tokens.)
 *
 * If the [tagSection] is null, add the comment to the initial text block
 * of the description. Otherwise if it is "@return", add the comment
 * to the return value. Otherwise the [tagSection] is taken to be the
 * parameter name, and the comment added as parameter documentation
 * for the given parameter.
 */
fun mergeDocumentation(
    existingDoc: String,
    psiElement: PsiElement,
    newText: String,
    tagSection: String?,
    append: Boolean
): String {

    if (existingDoc.isBlank()) {
        // There's no existing comment: Create a new one. This is easy.
        val content = when {
            tagSection == "@return" -> "@return $newText"
            tagSection?.startsWith("@") ?: false -> "$tagSection $newText"
            tagSection != null -> "@param $tagSection $newText"
            else -> newText
        }

        val inherit =
            when (psiElement) {
                is PsiMethod -> psiElement.findSuperMethods(true).isNotEmpty()
                else -> false
            }
        val initial = if (inherit) "/**\n* {@inheritDoc}\n */" else "/** */"
        val new = insertInto(initial, content, initial.indexOf("*/"))
        if (new.startsWith("/**\n * \n *")) {
            return "/**\n *" + new.substring(10)
        }
        return new
    }

    val doc = trimDocIndent(existingDoc)

    // We'll use the PSI Javadoc support to parse the documentation
    // to help us scan the tokens in the documentation, such that
    // we don't have to search for raw substrings like "@return" which
    // can incorrectly find matches in escaped code snippets etc.
    val factory = JavaPsiFacade.getElementFactory(psiElement.project)
        ?: error("Invalid tool configuration; did not find JavaPsiFacade factory")
    val docComment = factory.createDocCommentFromText(doc)

    if (tagSection == "@return") {
        // Add in return value
        val returnTag = docComment.findTagByName("return")
        if (returnTag == null) {
            // Find last tag
            val lastTag = findLastTag(docComment)
            val offset = if (lastTag != null) {
                findTagEnd(lastTag)
            } else {
                doc.length - 2
            }
            return insertInto(doc, "@return $newText", offset)
        } else {
            // Add text to the existing @return tag
            val offset = if (append)
                findTagEnd(returnTag)
            else returnTag.textRange.startOffset + returnTag.name.length + 1
            return insertInto(doc, newText, offset)
        }
    } else if (tagSection != null) {
        val parameter = if (tagSection.startsWith("@"))
            docComment.findTagByName(tagSection.substring(1))
        else findParamTag(docComment, tagSection)
        if (parameter == null) {
            // Add new parameter or tag
            // TODO: Decide whether to place it alphabetically or place it by parameter order
            // in the signature. Arguably I should follow the convention already present in the
            // doc, if any
            // For now just appending to the last tag before the return tag (if any).
            // This actually works out well in practice where arguments are generally all documented
            // or all not documented; when none of the arguments are documented these end up appending
            // exactly in the right parameter order!
            val returnTag = docComment.findTagByName("return")
            val anchor = returnTag ?: findLastTag(docComment)
            val offset = when {
                returnTag != null -> returnTag.textRange.startOffset
                anchor != null -> findTagEnd(anchor)
                else -> doc.length - 2 // "*/
            }
            val tagName = if (tagSection.startsWith("@")) tagSection else "@param $tagSection"
            return insertInto(doc, "$tagName $newText", offset)
        } else {
            // Add to existing tag/parameter
            val offset = if (append)
                findTagEnd(parameter)
            else parameter.textRange.startOffset + parameter.name.length + 1
            return insertInto(doc, newText, offset)
        }
    } else {
        // Add to the main text section of the comment.
        val firstTag = findFirstTag(docComment)
        val startOffset =
            if (!append) {
                4 // "/** ".length
            } else firstTag?.textRange?.startOffset ?: doc.length - 2
        // Insert a <br> before the appended docs, unless it's the beginning of a doc section
        return insertInto(doc, if (startOffset > 4) "<br>\n$newText" else newText, startOffset)
    }
}

fun findParamTag(docComment: PsiDocComment, paramName: String): PsiDocTag? {
    return docComment.findTagsByName("param").firstOrNull { it.valueElement?.text == paramName }
}

fun findFirstTag(docComment: PsiDocComment): PsiDocTag? {
    return docComment.tags.asSequence().minBy { it.textRange.startOffset }
}

fun findLastTag(docComment: PsiDocComment): PsiDocTag? {
    return docComment.tags.asSequence().maxBy { it.textRange.startOffset }
}

fun findTagEnd(tag: PsiDocTag): Int {
    var curr: PsiElement? = tag.nextSibling
    while (curr != null) {
        if (curr is PsiDocToken && curr.tokenType == JavaDocTokenType.DOC_COMMENT_END) {
            return curr.textRange.startOffset
        } else if (curr is PsiDocTag) {
            return curr.textRange.startOffset
        }

        curr = curr.nextSibling
    }

    return tag.textRange.endOffset
}

fun trimDocIndent(existingDoc: String): String {
    val index = existingDoc.indexOf('\n')
    if (index == -1) {
        return existingDoc
    }

    return existingDoc.substring(0, index + 1) +
        existingDoc.substring(index + 1).trimIndent().split('\n').joinToString(separator = "\n") {
            if (!it.startsWith(" ")) {
                " ${it.trimEnd()}"
            } else {
                it.trimEnd()
            }
        }
}

fun insertInto(existingDoc: String, newText: String, initialOffset: Int): String {
    // TODO: Insert "." between existing documentation and new documentation, if necessary.

    val offset = if (initialOffset > 4 && existingDoc.regionMatches(initialOffset - 4, "\n * ", 0, 4, false)) {
        initialOffset - 4
    } else {
        initialOffset
    }
    val index = existingDoc.indexOf('\n')
    val prefixWithStar = index == -1 || existingDoc[index + 1] == '*' ||
        existingDoc[index + 1] == ' ' && existingDoc[index + 2] == '*'

    val prefix = existingDoc.substring(0, offset)
    val suffix = existingDoc.substring(offset)
    val startSeparator = "\n"
    val endSeparator =
        if (suffix.startsWith("\n") || suffix.startsWith(" \n")) "" else if (suffix == "*/") "\n" else if (prefixWithStar) "\n * " else "\n"

    val middle = if (prefixWithStar) {
        startSeparator + newText.split('\n').joinToString(separator = "\n") { " * $it" } +
            endSeparator
    } else {
        "$startSeparator$newText$endSeparator"
    }

    // Going from single-line to multi-line?
    return if (existingDoc.indexOf('\n') == -1 && existingDoc.startsWith("/** ")) {
        prefix.substring(0, 3) + "\n *" + prefix.substring(3) + middle +
            if (suffix == "*/") " */" else suffix
    } else {
        prefix + middle + suffix
    }
}

/** Converts from package.html content to a package-info.java javadoc string. */
@Language("JAVA")
fun packageHtmlToJavadoc(@Language("HTML") packageHtml: String?): String {
    packageHtml ?: return ""
    if (packageHtml.isBlank()) {
        return ""
    }

    val body = getBodyContents(packageHtml).trim()
    if (body.isBlank()) {
        return ""
    }
    // Combine into comment lines prefixed by asterisk, ,and make sure we don't
    // have end-comment markers in the HTML that will escape out of the javadoc comment
    val comment = body.lines().joinToString(separator = "\n") { " * $it" }.replace("*/", "&#42;/")
    @Suppress("DanglingJavadoc")
    return "/**\n$comment\n */\n"
}

/**
 * Returns the body content from the given HTML document.
 * Attempts to tokenize the HTML properly such that it doesn't
 * get confused by comments or text that looks like tags.
 */
@Suppress("LocalVariableName")
private fun getBodyContents(html: String): String {
    val length = html.length
    val STATE_TEXT = 1
    val STATE_SLASH = 2
    val STATE_ATTRIBUTE_NAME = 3
    val STATE_IN_TAG = 4
    val STATE_BEFORE_ATTRIBUTE = 5
    val STATE_ATTRIBUTE_BEFORE_EQUALS = 6
    val STATE_ATTRIBUTE_AFTER_EQUALS = 7
    val STATE_ATTRIBUTE_VALUE_NONE = 8
    val STATE_ATTRIBUTE_VALUE_SINGLE = 9
    val STATE_ATTRIBUTE_VALUE_DOUBLE = 10
    val STATE_CLOSE_TAG = 11
    val STATE_ENDING_TAG = 12

    var bodyStart = -1
    var htmlStart = -1

    var state = STATE_TEXT
    var offset = 0
    var tagStart = -1
    var tagEndStart = -1
    var prev = -1
    loop@ while (offset < length) {
        if (offset == prev) {
            // Purely here to prevent potential bugs in the state machine from looping
            // infinitely
            offset++
            if (offset == length) {
                break
            }
        }
        prev = offset

        val c = html[offset]
        when (state) {
            STATE_TEXT -> {
                if (c == '<') {
                    state = STATE_SLASH
                    offset++
                    continue@loop
                }

                // Other text is just ignored
                offset++
            }

            STATE_SLASH -> {
                if (c == '!') {
                    if (html.startsWith("!--", offset)) {
                        // Comment
                        val end = html.indexOf("-->", offset + 3)
                        if (end == -1) {
                            offset = length
                        } else {
                            offset = end + 3
                            state = STATE_TEXT
                        }
                        continue@loop
                    } else if (html.startsWith("![CDATA[", offset)) {
                        val end = html.indexOf("]]>", offset + 8)
                        if (end == -1) {
                            offset = length
                        } else {
                            state = STATE_TEXT
                            offset = end + 3
                        }
                        continue@loop
                    } else {
                        val end = html.indexOf('>', offset + 2)
                        if (end == -1) {
                            offset = length
                            state = STATE_TEXT
                        } else {
                            offset = end + 1
                            state = STATE_TEXT
                        }
                        continue@loop
                    }
                } else if (c == '/') {
                    state = STATE_CLOSE_TAG
                    offset++
                    tagEndStart = offset
                    continue@loop
                } else if (c == '?') {
                    // XML Prologue
                    val end = html.indexOf('>', offset + 2)
                    if (end == -1) {
                        offset = length
                        state = STATE_TEXT
                    } else {
                        offset = end + 1
                        state = STATE_TEXT
                    }
                    continue@loop
                }
                state = STATE_IN_TAG
                tagStart = offset
            }

            STATE_CLOSE_TAG -> {
                if (c == '>') {
                    state = STATE_TEXT
                    if (html.startsWith("body", tagEndStart, true)) {
                        val bodyEnd = tagEndStart - 2 // </
                        if (bodyStart != -1) {
                            return html.substring(bodyStart, bodyEnd)
                        }
                    }
                    if (html.startsWith("html", tagEndStart, true)) {
                        val htmlEnd = tagEndStart - 2
                        if (htmlEnd != -1) {
                            return html.substring(htmlStart, htmlEnd)
                        }
                    }
                }
                offset++
            }

            STATE_IN_TAG -> {
                val whitespace = Character.isWhitespace(c)
                if (whitespace || c == '>') {
                    if (html.startsWith("body", tagStart, true)) {
                        bodyStart = html.indexOf('>', offset) + 1
                    }
                    if (html.startsWith("html", tagStart, true)) {
                        htmlStart = html.indexOf('>', offset) + 1
                    }
                }

                when {
                    whitespace -> state = STATE_BEFORE_ATTRIBUTE
                    c == '>' -> {
                        state = STATE_TEXT
                    }
                    c == '/' -> state = STATE_ENDING_TAG
                }
                offset++
            }

            STATE_ENDING_TAG -> {
                if (c == '>') {
                    if (html.startsWith("body", tagEndStart, true)) {
                        val bodyEnd = tagEndStart - 1
                        if (bodyStart != -1) {
                            return html.substring(bodyStart, bodyEnd)
                        }
                    }
                    if (html.startsWith("html", tagEndStart, true)) {
                        val htmlEnd = tagEndStart - 1
                        if (htmlEnd != -1) {
                            return html.substring(htmlStart, htmlEnd)
                        }
                    }
                    offset++
                    state = STATE_TEXT
                }
            }

            STATE_BEFORE_ATTRIBUTE -> {
                if (c == '>') {
                    state = STATE_TEXT
                } else if (c == '/') {
                    // we expect an '>' next to close the tag
                } else if (!Character.isWhitespace(c)) {
                    state = STATE_ATTRIBUTE_NAME
                }
                offset++
            }
            STATE_ATTRIBUTE_NAME -> {
                when {
                    c == '>' -> state = STATE_TEXT
                    c == '=' -> state = STATE_ATTRIBUTE_AFTER_EQUALS
                    Character.isWhitespace(c) -> state = STATE_ATTRIBUTE_BEFORE_EQUALS
                    c == ':' -> {
                    }
                }
                offset++
            }
            STATE_ATTRIBUTE_BEFORE_EQUALS -> {
                if (c == '=') {
                    state = STATE_ATTRIBUTE_AFTER_EQUALS
                } else if (c == '>') {
                    state = STATE_TEXT
                } else if (!Character.isWhitespace(c)) {
                    // Attribute value not specified (used for some boolean attributes)
                    state = STATE_ATTRIBUTE_NAME
                }
                offset++
            }

            STATE_ATTRIBUTE_AFTER_EQUALS -> {
                if (c == '\'') {
                    // a='b'
                    state = STATE_ATTRIBUTE_VALUE_SINGLE
                } else if (c == '"') {
                    // a="b"
                    state = STATE_ATTRIBUTE_VALUE_DOUBLE
                } else if (!Character.isWhitespace(c)) {
                    // a=b
                    state = STATE_ATTRIBUTE_VALUE_NONE
                }
                offset++
            }

            STATE_ATTRIBUTE_VALUE_SINGLE -> {
                if (c == '\'') {
                    state = STATE_BEFORE_ATTRIBUTE
                }
                offset++
            }
            STATE_ATTRIBUTE_VALUE_DOUBLE -> {
                if (c == '"') {
                    state = STATE_BEFORE_ATTRIBUTE
                }
                offset++
            }
            STATE_ATTRIBUTE_VALUE_NONE -> {
                if (c == '>') {
                    state = STATE_TEXT
                } else if (Character.isWhitespace(c)) {
                    state = STATE_BEFORE_ATTRIBUTE
                }
                offset++
            }
            else -> assert(false) { state }
        }
    }

    return html
}

fun containsLinkTags(documentation: String): Boolean {
    var index = 0
    while (true) {
        index = documentation.indexOf('@', index)
        if (index == -1) {
            return false
        }
        if (!documentation.startsWith("@code", index) &&
            !documentation.startsWith("@literal", index) &&
            !documentation.startsWith("@param", index) &&
            !documentation.startsWith("@deprecated", index) &&
            !documentation.startsWith("@inheritDoc", index) &&
            !documentation.startsWith("@return", index)
        ) {
            return true
        }

        index++
    }
}

// ------------------------------------------------------------------------------------
// Expanding javadocs into fully qualified documentation
// ------------------------------------------------------------------------------------

fun toFullyQualifiedDocumentation(owner: PsiItem, documentation: String): String {
    if (documentation.isBlank() || !containsLinkTags(documentation)) {
        return documentation
    }

    val codebase = owner.codebase
    val comment =
        try {
            codebase.getComment(documentation, owner.psi())
        } catch (throwable: Throwable) {
            // TODO: Get rid of line comments as documentation
            // Invalid comment
            if (documentation.startsWith("//") && documentation.contains("/**")) {
                return toFullyQualifiedDocumentation(owner, documentation.substring(documentation.indexOf("/**")))
            }
            codebase.getComment(documentation, owner.psi())
        }
    val sb = StringBuilder(documentation.length)
    expand(owner, comment, sb)

    return sb.toString()
}

private fun reportUnresolvedDocReference(owner: Item, unresolved: String) {
    @Suppress("ConstantConditionIf")
    if (!REPORT_UNRESOLVED_SYMBOLS) {
        return
    }

    if (unresolved.startsWith("{@") && !unresolved.startsWith("{@link")) {
        return
    }

    // References are sometimes split across lines and therefore have newlines, leading asterisks
    // etc in the middle: clean this up before emitting reference into error message
    val cleaned = unresolved.replace("\n", "").replace("*", "")
        .replace("  ", " ")

    reporter.report(Issues.UNRESOLVED_LINK, owner, "Unresolved documentation reference: $cleaned")
}

private fun expand(owner: PsiItem, element: PsiElement, sb: StringBuilder) {
    when {
        element is PsiWhiteSpace -> {
            sb.append(element.text)
        }
        element is PsiDocToken -> {
            assert(element.firstChild == null)
            val text = element.text
            // Auto-fix some docs in the framework which starts with R.styleable in @attr
            if (text.startsWith("R.styleable#") && owner.documentation.contains("@attr")) {
                sb.append("android.")
            }

            sb.append(text)
        }
        element is PsiDocMethodOrFieldRef -> {
            val text = element.text
            var resolved = element.reference?.resolve()

            // Workaround: relative references doesn't work from a class item to its members
            if (resolved == null && owner is ClassItem) {
                // For some reason, resolving relative methods and field references at the root
                // level isn't working right.
                if (PREPEND_LOCAL_CLASS && text.startsWith("#")) {
                    var end = text.indexOf('(')
                    if (end == -1) {
                        // definitely a field
                        end = text.length
                        val fieldName = text.substring(1, end)
                        val field = owner.findField(fieldName)
                        if (field != null) {
                            resolved = field.psi()
                        }
                    }
                    if (resolved == null) {
                        val methodName = text.substring(1, end)
                        resolved = (owner.psi() as PsiClass).findMethodsByName(methodName, true).firstOrNull()
                    }
                }
            }

            if (resolved is PsiMember) {
                val containingClass = resolved.containingClass
                if (containingClass != null && !samePackage(owner, containingClass)) {
                    val referenceText = element.reference?.element?.text ?: text
                    if (!PREPEND_LOCAL_CLASS && referenceText.startsWith("#")) {
                        sb.append(text)
                        return
                    }

                    var className = containingClass.qualifiedName

                    if (element.firstChildNode.elementType === JavaDocElementType.DOC_REFERENCE_HOLDER) {
                        val firstChildPsi =
                            SourceTreeToPsiMap.treeElementToPsi(element.firstChildNode.firstChildNode)
                        if (firstChildPsi is PsiJavaCodeReferenceElement) {
                            val referenceElement = firstChildPsi as PsiJavaCodeReferenceElement?
                            val referencedElement = referenceElement!!.resolve()
                            if (referencedElement is PsiClass) {
                                className = referencedElement.qualifiedName
                            }
                        }
                    }

                    sb.append(className)
                    sb.append('#')
                    sb.append(resolved.name)
                    val index = text.indexOf('(')
                    if (index != -1) {
                        sb.append(text.substring(index))
                    }
                } else {
                    sb.append(text)
                }
            } else {
                if (resolved == null) {
                    val referenceText = element.reference?.element?.text ?: text
                    if (text.startsWith("#") && owner is ClassItem) {
                        // Unfortunately resolving references is broken from class javadocs
                        // to members using just a relative reference, #.
                    } else {
                        reportUnresolvedDocReference(owner, referenceText)
                    }
                }
                sb.append(text)
            }
        }
        element is PsiJavaCodeReferenceElement -> {
            val resolved = element.resolve()
            if (resolved is PsiClass) {
                if (samePackage(owner, resolved) || resolved is PsiTypeParameter) {
                    sb.append(element.text)
                } else {
                    sb.append(resolved.qualifiedName)
                }
            } else if (resolved is PsiMember) {
                val text = element.text
                sb.append(resolved.containingClass?.qualifiedName)
                sb.append('#')
                sb.append(resolved.name)
                val index = text.indexOf('(')
                if (index != -1) {
                    sb.append(text.substring(index))
                }
            } else {
                val text = element.text
                if (resolved == null) {
                    reportUnresolvedDocReference(owner, text)
                }
                sb.append(text)
            }
        }
        element is PsiInlineDocTag -> {
            val handled = handleTag(element, owner, sb)
            if (!handled) {
                sb.append(element.text)
            }
        }
        element.firstChild != null -> {
            var curr = element.firstChild
            while (curr != null) {
                expand(owner, curr, sb)
                curr = curr.nextSibling
            }
        }
        else -> {
            val text = element.text
            sb.append(text)
        }
    }
}

fun handleTag(
    element: PsiInlineDocTag,
    owner: PsiItem,
    sb: StringBuilder
): Boolean {
    val name = element.name
    if (name == "code" || name == "literal") {
        // @code: don't attempt to rewrite this
        sb.append(element.text)
        return true
    }

    val reference = extractReference(element)
    val referenceText = reference?.element?.text ?: element.text
    if (!PREPEND_LOCAL_CLASS && referenceText.startsWith("#")) {
        val suffix = element.text
        if (suffix.contains("(") && suffix.contains(")")) {
            expandArgumentList(element, suffix, sb)
        } else {
            sb.append(suffix)
        }
        return true
    }

    // TODO: If referenceText is already absolute, e.g. android.Manifest.permission#BIND_CARRIER_SERVICES,
    // try to short circuit this?

    val valueElement = element.valueElement
    if (valueElement is CompositePsiElement) {
        if (valueElement.firstChildNode.elementType === JavaDocElementType.DOC_REFERENCE_HOLDER) {
            val firstChildPsi =
                SourceTreeToPsiMap.treeElementToPsi(valueElement.firstChildNode.firstChildNode)
            if (firstChildPsi is PsiJavaCodeReferenceElement) {
                val referenceElement = firstChildPsi as PsiJavaCodeReferenceElement?
                val referencedElement = referenceElement!!.resolve()
                if (referencedElement is PsiClass) {
                    var className = PsiClassItem.computeFullClassName(referencedElement)
                    if (className.indexOf('.') != -1 && !referenceText.startsWith(className)) {
                        val simpleName = referencedElement.name
                        if (simpleName != null && referenceText.startsWith(simpleName)) {
                            className = simpleName
                        }
                    }
                    if (referenceText.startsWith(className)) {
                        sb.append("{@")
                        sb.append(element.name)
                        sb.append(' ')
                        sb.append(referencedElement.qualifiedName)
                        val suffix = referenceText.substring(className.length)
                        if (suffix.contains("(") && suffix.contains(")")) {
                            expandArgumentList(element, suffix, sb)
                        } else {
                            sb.append(suffix)
                        }
                        sb.append(' ')
                        sb.append(referenceText)
                        sb.append("}")
                        return true
                    }
                }
            }
        }
    }

    var resolved = reference?.resolve()
    if (resolved == null && owner is ClassItem) {
        // For some reason, resolving relative methods and field references at the root
        // level isn't working right.
        if (PREPEND_LOCAL_CLASS && referenceText.startsWith("#")) {
            var end = referenceText.indexOf('(')
            if (end == -1) {
                // definitely a field
                end = referenceText.length
                val fieldName = referenceText.substring(1, end)
                val field = owner.findField(fieldName)
                if (field != null) {
                    resolved = field.psi()
                }
            }
            if (resolved == null) {
                val methodName = referenceText.substring(1, end)
                resolved = (owner.psi() as PsiClass).findMethodsByName(methodName, true).firstOrNull()
            }
        }
    }

    if (resolved != null) {
        when (resolved) {
            is PsiClass -> {
                val text = element.text
                if (samePackage(owner, resolved)) {
                    sb.append(text)
                    return true
                }
                val qualifiedName = resolved.qualifiedName ?: run {
                    sb.append(text)
                    return true
                }
                if (referenceText == qualifiedName) {
                    // Already absolute
                    sb.append(text)
                    return true
                }
                val append = when {
                    valueElement != null -> {
                        val start = valueElement.startOffsetInParent
                        val end = start + valueElement.textLength
                        text.substring(0, start) + qualifiedName + text.substring(end)
                    }
                    name == "see" -> {
                        val suffix = text.substring(text.indexOf(referenceText) + referenceText.length)
                        "@see $qualifiedName$suffix"
                    }
                    text.startsWith("{") -> "{@$name $qualifiedName $referenceText}"
                    else -> "@$name $qualifiedName $referenceText"
                }
                sb.append(append)
                return true
            }
            is PsiMember -> {
                val text = element.text
                val containing = resolved.containingClass ?: run {
                    sb.append(text)
                    return true
                }
                if (samePackage(owner, containing)) {
                    sb.append(text)
                    return true
                }
                val qualifiedName = containing.qualifiedName ?: run {
                    sb.append(text)
                    return true
                }
                if (referenceText.startsWith(qualifiedName)) {
                    // Already absolute
                    sb.append(text)
                    return true
                }

                // It may also be the case that the reference is already fully qualified
                // but to some different class. For example, the link may be to
                // android.os.Bundle#getInt, but the resolved method actually points to
                // an inherited method into android.os.Bundle from android.os.BaseBundle.
                // In that case we don't want to rewrite the link.
                for (index in 0 until referenceText.length) {
                    val c = referenceText[index]
                    if (c == '.') {
                        // Already qualified
                        sb.append(text)
                        return true
                    } else if (!Character.isJavaIdentifierPart(c)) {
                        break
                    }
                }

                if (valueElement != null) {
                    val start = valueElement.startOffsetInParent

                    var nameEnd = -1
                    var close = start
                    var balance = 0
                    while (close < text.length) {
                        val c = text[close]
                        if (c == '(') {
                            if (nameEnd == -1) {
                                nameEnd = close
                            }
                            balance++
                        } else if (c == ')') {
                            balance--
                            if (balance == 0) {
                                close++
                                break
                            }
                        } else if (c == '}') {
                            if (nameEnd == -1) {
                                nameEnd = close
                            }
                            break
                        } else if (balance == 0 && c == '#') {
                            if (nameEnd == -1) {
                                nameEnd = close
                            }
                        } else if (balance == 0 && !Character.isJavaIdentifierPart(c)) {
                            break
                        }
                        close++
                    }
                    val memberPart = text.substring(nameEnd, close)
                    val append = "${text.substring(0, start)}$qualifiedName$memberPart $referenceText}"
                    sb.append(append)
                    return true
                }
            }
        }
    } else {
        reportUnresolvedDocReference(owner, referenceText)
    }

    return false
}

private fun expandArgumentList(
    element: PsiInlineDocTag,
    suffix: String,
    sb: StringBuilder
) {
    val elementFactory = JavaPsiFacade.getElementFactory(element.project)
    // Try to rewrite the types to fully qualified names as well
    val begin = suffix.indexOf('(')
    sb.append(suffix.substring(0, begin + 1))
    var index = begin + 1
    var balance = 0
    var argBegin = index
    while (index < suffix.length) {
        val c = suffix[index++]
        if (c == '<' || c == '(') {
            balance++
        } else if (c == '>') {
            balance--
        } else if (c == ')' && balance == 0 || c == ',') {
            // Strip off javadoc header
            while (argBegin < index) {
                val p = suffix[argBegin]
                if (p != '*' && !p.isWhitespace()) {
                    break
                }
                argBegin++
            }
            if (index > argBegin + 1) {
                val arg = suffix.substring(argBegin, index - 1).trim()
                val space = arg.indexOf(' ')
                // Strip off parameter name (shouldn't be there but happens
                // in some Android sources sine tools didn't use to complain
                val typeString = if (space == -1) {
                    arg
                } else {
                    if (space < arg.length - 1 && !arg[space + 1].isJavaIdentifierStart()) {
                        // Example: "String []"
                        arg
                    } else {
                        // Example "String name"
                        arg.substring(0, space)
                    }
                }
                var insert = arg
                if (typeString[0].isUpperCase()) {
                    try {
                        val type = elementFactory.createTypeFromText(typeString, element)
                        insert = type.canonicalText
                    } catch (ignore: com.intellij.util.IncorrectOperationException) {
                        // Not a valid type - just leave what was in the parameter text
                    }
                }
                sb.append(insert)
                sb.append(c)
                if (c == ')') {
                    break
                }
            } else if (c == ')') {
                sb.append(')')
                break
            }
            argBegin = index
        } else if (c == ')') {
            balance--
        }
    }
    while (index < suffix.length) {
        sb.append(suffix[index++])
    }
}

private fun samePackage(owner: PsiItem, cls: PsiClass): Boolean {
    @Suppress("ConstantConditionIf")
    if (INCLUDE_SAME_PACKAGE) {
        // doclava seems to have REAL problems with this
        return false
    }
    val pkg = packageName(owner) ?: return false
    return cls.qualifiedName == "$pkg.${cls.name}"
}

private fun packageName(owner: PsiItem): String? {
    var curr: Item? = owner
    while (curr != null) {
        if (curr is PackageItem) {
            return curr.qualifiedName()
        }
        curr = curr.parent()
    }

    return null
}

// Copied from UnnecessaryJavaDocLinkInspection and tweaked a bit
private fun extractReference(tag: PsiDocTag): PsiReference? {
    val valueElement = tag.valueElement
    if (valueElement != null) {
        return valueElement.reference
    }
    // hack around the fact that a reference to a class is apparently
    // not a PsiDocTagValue
    val dataElements = tag.dataElements
    if (dataElements.isEmpty()) {
        return null
    }
    val salientElement: PsiElement =
        dataElements.firstOrNull { it !is PsiWhiteSpace && it !is PsiDocToken } ?: return null
    val child = salientElement.firstChild
    return if (child !is PsiReference) null else child
}
