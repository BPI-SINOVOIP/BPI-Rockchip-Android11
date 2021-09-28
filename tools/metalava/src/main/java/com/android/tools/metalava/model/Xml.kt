/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tools.metalava.model

import com.android.utils.XmlUtils.stripBom
import org.w3c.dom.Document
import org.xml.sax.ErrorHandler
import org.xml.sax.InputSource
import org.xml.sax.SAXParseException
import java.io.PrintWriter
import java.io.Reader
import java.io.StringReader
import javax.xml.parsers.DocumentBuilder
import javax.xml.parsers.DocumentBuilderFactory
import javax.xml.parsers.ParserConfigurationException

// XML parser features
private const val LOAD_EXTERNAL_DTD = "http://apache.org/xml/features/nonvalidating/load-external-dtd"
private const val EXTERNAL_PARAMETER_ENTITIES = "http://xml.org/sax/features/external-parameter-entities"
private const val EXTERNAL_GENERAL_ENTITIES = "http://xml.org/sax/features/external-general-entities"

/**
 * Parses the given XML string as a DOM document, using the JDK parser. The parser does not
 * validate, and is optionally namespace aware.
 *
 * @param xml the XML content to be parsed (must be well formed)
 * @param namespaceAware whether the parser is namespace aware
 * @param errorWriter the writer to write errors to
 * @return the DOM document
 */
fun parseDocument(xml: String, namespaceAware: Boolean, errorWriter: PrintWriter = PrintWriter(System.out)): Document {
    return parseDocument(StringReader(stripBom(xml)), namespaceAware, errorWriter)
}

/**
 * Parses the given [Reader] as a DOM document, using the JDK parser. The parser does not
 * validate, and is optionally namespace aware.
 *
 * @param xml a reader for the XML content to be parsed (must be well formed)
 * @param namespaceAware whether the parser is namespace aware
 * @param errorWriter the writer to write errors to
 * @return the DOM document
 */
fun parseDocument(xml: Reader, namespaceAware: Boolean, errorWriter: PrintWriter = PrintWriter(System.out)): Document {
    val inputStream = InputSource(xml)
    val builder = createDocumentBuilder(namespaceAware)
    builder.setErrorHandler(object : ErrorHandler {
        override fun warning(exception: SAXParseException) {
            errorWriter.println("${exception.lineNumber}: Warning: ${exception.message}")
        }

        override fun error(exception: SAXParseException) {
            errorWriter.println("${exception.lineNumber}: Error: ${exception.message}")
        }

        override fun fatalError(exception: SAXParseException) {
            error(exception)
        }
    })
    return builder.parse(inputStream)
}

/** Creates a preconfigured document builder.  */
private fun createDocumentBuilder(namespaceAware: Boolean): DocumentBuilder {
    try {
        val factory = DocumentBuilderFactory.newInstance()
        factory.isNamespaceAware = namespaceAware
        factory.isValidating = false
        factory.setFeature(EXTERNAL_GENERAL_ENTITIES, false)
        factory.setFeature(EXTERNAL_PARAMETER_ENTITIES, false)
        factory.setFeature(LOAD_EXTERNAL_DTD, false)
        return factory.newDocumentBuilder()
    } catch (e: ParserConfigurationException) {
        throw Error(e) // Impossible in the current context.
    }
}
