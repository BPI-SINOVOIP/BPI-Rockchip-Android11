/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.processor.compat.changeid;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

import java.io.IOException;
import java.io.OutputStream;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

/**
 * <p>Writes an XML config file containing provided changes.</p>
 * <p>Output example:</p>
 * <pre>
 * {@code
 * <config>
 *     <compat-change id="111" name="change-name1">
 *         <meta-data definedIn="java.package.ClassName" sourcePosition="java/package/ClassName
 *         .java:10" />
 *     </compat-change>
 *     <compat-change disabled="true" id="222" loggingOnly= "true" name="change-name2"
 *     description="my change">
 *         <meta-data .../>
 *     </compat-change>
 *     <compat-change enableAfterTargetSdk="28" id="333" name="change-name3">
 *         <meta-data .../>
 *     </compat-change>
 * </config>
 *  }
 *
 * </pre>
 *
 * The inner {@code meta-data} tags are intended to be stripped before embedding the config on a
 * device. They are intended for use by intermediate build tools only.
 */
final class XmlWriter {
    //XML tags
    private static final String XML_ROOT = "config";
    private static final String XML_CHANGE_ELEMENT = "compat-change";
    private static final String XML_NAME_ATTR = "name";
    private static final String XML_ID_ATTR = "id";
    private static final String XML_DISABLED_ATTR = "disabled";
    private static final String XML_LOGGING_ATTR = "loggingOnly";
    private static final String XML_ENABLED_AFTER_ATTR = "enableAfterTargetSdk";
    private static final String XML_DESCRIPTION_ATTR = "description";
    private static final String XML_METADATA_ELEMENT = "meta-data";
    private static final String XML_DEFINED_IN = "definedIn";
    private static final String XML_SOURCE_POSITION = "sourcePosition";

    private Document mDocument;
    private Element mRoot;

    XmlWriter() {
        mDocument = createDocument();
        mRoot = mDocument.createElement(XML_ROOT);
        mDocument.appendChild(mRoot);
    }

    void addChange(Change change) {
        Element newElement = mDocument.createElement(XML_CHANGE_ELEMENT);
        newElement.setAttribute(XML_NAME_ATTR, change.name);
        newElement.setAttribute(XML_ID_ATTR, change.id.toString());
        if (change.disabled) {
            newElement.setAttribute(XML_DISABLED_ATTR, "true");
        }
        if (change.loggingOnly) {
            newElement.setAttribute(XML_LOGGING_ATTR, "true");
        }
        if (change.enabledAfter != null) {
            newElement.setAttribute(XML_ENABLED_AFTER_ATTR, change.enabledAfter.toString());
        }
        if (change.description != null) {
            newElement.setAttribute(XML_DESCRIPTION_ATTR, change.description);
        }
        Element metaData = mDocument.createElement(XML_METADATA_ELEMENT);
        if (change.qualifiedClass != null) {
            metaData.setAttribute(XML_DEFINED_IN, change.qualifiedClass);
        }
        if (change.sourcePosition != null) {
            metaData.setAttribute(XML_SOURCE_POSITION, change.sourcePosition);
        }
        if (metaData.hasAttributes()) {
            newElement.appendChild(metaData);
        }
        mRoot.appendChild(newElement);
    }

    void write(OutputStream output) throws IOException {
        try {
            TransformerFactory transformerFactory = TransformerFactory.newInstance();
            Transformer transformer = transformerFactory.newTransformer();
            DOMSource domSource = new DOMSource(mDocument);

            StreamResult result = new StreamResult(output);

            transformer.transform(domSource, result);
        } catch (TransformerException e) {
            throw new IOException("Failed to write output", e);
        }
    }

    private Document createDocument() {
        try {
            DocumentBuilderFactory documentFactory = DocumentBuilderFactory.newInstance();
            DocumentBuilder documentBuilder = documentFactory.newDocumentBuilder();
            return documentBuilder.newDocument();
        } catch (ParserConfigurationException e) {
            throw new RuntimeException("Failed to create a new document", e);
        }
    }
}
