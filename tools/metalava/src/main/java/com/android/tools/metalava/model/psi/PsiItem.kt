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

import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty
import com.android.tools.metalava.model.DefaultItem
import com.android.tools.metalava.model.MutableModifierList
import com.android.tools.metalava.model.ParameterItem
import com.intellij.psi.PsiCompiledElement
import com.intellij.psi.PsiDocCommentOwner
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiModifierListOwner
import org.jetbrains.kotlin.idea.KotlinLanguage
import org.jetbrains.kotlin.kdoc.psi.api.KDoc
import org.jetbrains.uast.UElement
import org.jetbrains.uast.sourcePsiElement

abstract class PsiItem(
    override val codebase: PsiBasedCodebase,
    val element: PsiElement,
    override val modifiers: PsiModifierItem,
    override var documentation: String
) : DefaultItem() {

    @Suppress("LeakingThis")
    override var deprecated: Boolean = modifiers.isDeprecated()

    @Suppress("LeakingThis") // Documentation can change, but we don't want to pick up subsequent @docOnly mutations
    override var docOnly = documentation.contains("@doconly")
    @Suppress("LeakingThis")
    override var removed = documentation.contains("@removed")

    // a property with a lazily calculated default value
    inner class lazyDelegate<T>(
        val defaultValueProvider: () -> T
    ) : ReadWriteProperty<PsiItem, T> {
        private var currentValue: T? = null

        override operator fun setValue(thisRef: PsiItem, property: KProperty<*>, value: T) {
            currentValue = value
        }
        override operator fun getValue(thisRef: PsiItem, property: KProperty<*>): T {
            if (currentValue == null) {
                currentValue = defaultValueProvider()
            }

            return currentValue!!
        }
    }

    override var originallyHidden: Boolean by lazyDelegate({
        documentation.contains('@') &&

            (documentation.contains("@hide") ||
                documentation.contains("@pending") ||
                // KDoc:
                documentation.contains("@suppress")) ||
            modifiers.hasHideAnnotations()
    })

    override var hidden: Boolean by lazyDelegate({ originallyHidden && !modifiers.hasShowAnnotation() })

    override fun psi(): PsiElement? = element

    // TODO: Consider only doing this in tests!
    override fun isFromClassPath(): Boolean {
        return if (element is UElement) {
            (element.sourcePsi ?: element.javaPsi) is PsiCompiledElement
        } else {
            element is PsiCompiledElement
        }
    }

    override fun isCloned(): Boolean = false

    /** Get a mutable version of modifiers for this item */
    override fun mutableModifiers(): MutableModifierList = modifiers

    override fun findTagDocumentation(tag: String): String? {
        if (element is PsiCompiledElement) {
            return null
        }
        if (documentation.isBlank()) {
            return null
        }

        // We can't just use element.docComment here because we may have modified
        // the comment and then the comment snapshot in PSI isn't up to date with our
        // latest changes
        val docComment = codebase.getComment(documentation)
        val docTag = docComment.findTagByName(tag) ?: return null
        val text = docTag.text

        // Trim trailing next line (javadoc *)
        var index = text.length - 1
        while (index > 0) {
            val c = text[index]
            if (!(c == '*' || c.isWhitespace())) {
                break
            }
            index--
        }
        index++
        return if (index < text.length) {
            text.substring(0, index)
        } else {
            text
        }
    }

    override fun appendDocumentation(comment: String, tagSection: String?, append: Boolean) {
        if (comment.isBlank()) {
            return
        }

        // TODO: Figure out if an annotation should go on the return value, or on the method.
        // For example; threading: on the method, range: on the return value.
        // TODO: Find a good way to add or append to a given tag (@param <something>, @return, etc)

        if (this is ParameterItem) {
            // For parameters, the documentation goes into the surrounding method's documentation!
            // Find the right parameter location!
            val parameterName = name()
            val target = containingMethod()
            target.appendDocumentation(comment, parameterName)
            return
        }

        // Micro-optimization: we're very often going to be merging @apiSince and to a lesser
        // extend @deprecatedSince into existing comments, since we're flagging every single
        // public API. Normally merging into documentation has to be done carefully, since
        // there could be existing versions of the tag we have to append to, and some parts
        // of the comment needs to be present in certain places. For example, you can't
        // just append to the description of a method by inserting something right before "*/"
        // since you could be appending to a javadoc tag like @return.
        //
        // However, for @apiSince and @deprecatedSince specifically, in addition to being frequent,
        // they will (a) never appear in existing docs, and (b) they're separate tags, which means
        // it's safe to append them at the end. So we'll special case these two tags here, to
        // help speed up the builds since these tags are inserted 30,000+ times for each framework
        // API target (there are many), and each time would have involved constructing a full javadoc
        // AST with lexical tokens using IntelliJ's javadoc parsing APIs. Instead, we'll just
        // do some simple string heuristics.
        if (tagSection == "@apiSince" || tagSection == "@deprecatedSince") {
            documentation = addUniqueTag(documentation, tagSection, comment)
            return
        }

        documentation = mergeDocumentation(documentation, element, comment.trim(), tagSection, append)
    }

    private fun addUniqueTag(documentation: String, tagSection: String, commentLine: String): String {
        assert(commentLine.indexOf('\n') == -1) // Not meant for multi-line comments

        if (documentation.isBlank()) {
            return "/** $tagSection $commentLine */"
        }

        // Already single line?
        if (documentation.indexOf('\n') == -1) {
            var end = documentation.lastIndexOf("*/")
            val s = "/**\n *" + documentation.substring(3, end) + "\n * $tagSection $commentLine\n */"
            return s
        }

        var end = documentation.lastIndexOf("*/")
        while (end > 0 && documentation[end - 1].isWhitespace() &&
            documentation[end - 1] != '\n') {
            end--
        }
        // The comment ends with:
        // * some comment here */
        var insertNewLine: Boolean = documentation[end - 1] != '\n'

        var indent: String
        var linePrefix = ""
        val secondLine = documentation.indexOf('\n')
        if (secondLine == -1) {
            // Single line comment
            indent = "\n * "
        } else {
            val indentStart = secondLine + 1
            var indentEnd = indentStart
            while (indentEnd < documentation.length) {
                if (!documentation[indentEnd].isWhitespace()) {
                    break
                }
                indentEnd++
            }
            indent = documentation.substring(indentStart, indentEnd)
            // TODO: If it starts with "* " follow that
            if (documentation.startsWith("* ", indentEnd)) {
                linePrefix = "* "
            }
        }
        val s = documentation.substring(0, end) + (if (insertNewLine) "\n" else "") + indent + linePrefix + tagSection + " " + commentLine + "\n" + indent + " */"
        return s
    }

    override fun fullyQualifiedDocumentation(): String {
        return fullyQualifiedDocumentation(documentation)
    }

    override fun fullyQualifiedDocumentation(documentation: String): String {
        return toFullyQualifiedDocumentation(this, documentation)
    }

    /** Finish initialization of the item */
    open fun finishInitialization() {
        modifiers.setOwner(this)
    }

    override fun isKotlin(): Boolean {
        return isKotlin(element)
    }

    companion object {
        fun javadoc(element: PsiElement): String {
            if (element is PsiCompiledElement) {
                return ""
            }

            if (element is UElement) {
                val comments = element.comments
                if (comments.isNotEmpty()) {
                    val sb = StringBuilder()
                    comments.asSequence().joinTo(buffer = sb, separator = "\n")
                    return sb.toString()
                } else {
                    // Temporary workaround: UAST seems to not return document nodes
                    // https://youtrack.jetbrains.com/issue/KT-22135
                    val first = element.sourcePsiElement?.firstChild
                    if (first is KDoc) {
                        return first.text
                    }
                }
            }

            if (element is PsiDocCommentOwner && element.docComment !is PsiCompiledElement) {
                return element.docComment?.text ?: ""
            }

            return ""
        }

        fun modifiers(
            codebase: PsiBasedCodebase,
            element: PsiModifierListOwner,
            documentation: String
        ): PsiModifierItem {
            return PsiModifierItem.create(codebase, element, documentation)
        }

        fun isKotlin(element: PsiElement): Boolean {
            return element.language === KotlinLanguage.INSTANCE
        }
    }
}
