/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.function.Function;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

/**
 * Parser to read apk-info.xml.
 */
public class GameCoreConfigurationXmlParser {

    public enum Field {
        NAME("name", null),

        // Certification requirements
        TARGET_FRAME_TIME("frameTime", "16.666"),
        MAX_JANK_RATE("jankRate", "0.0"),
        MAX_LOAD_TIME("loadTime", "-1"),

        // Apk info
        FILE_NAME("fileName", null),
        PACKAGE_NAME("packageName", null),
        ACTIVITY_NAME("activityName", null),
        LAYER_NAME("layerName", null),
        SCRIPT("script", null),
        ARGS("args", null),
        LOAD_TIME("loadTime", "10000"),
        RUN_TIME("runTime", "10000"),
        EXPECT_INTENTS("expectIntents", "false");

        private String mTag;
        private String mDefaultValue;

        Field(String tag, String defaultValue) {
            mTag = tag;
            mDefaultValue = defaultValue;
        }

        public String getTag() {
            return mTag;
        }

        public String getDefaultValue() {
            return mDefaultValue;
        }
    }

    public GameCoreConfigurationXmlParser() {
    }

    public GameCoreConfiguration parse(File file)
            throws IOException, ParserConfigurationException, SAXException {
        try (InputStream is = new FileInputStream(file)) {
            return parse(is);
        }
    }

    public GameCoreConfiguration parse(InputStream inputStream)
            throws ParserConfigurationException, IOException, SAXException {
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        DocumentBuilder db = dbf.newDocumentBuilder();
        Document doc = db.parse(inputStream);
        doc.getDocumentElement().normalize();

        List<CertificationRequirements> requirements = parseList(
                doc.getDocumentElement(),
                "certification",
                this::createCertificationRequirements);
        if (requirements == null) {
            requirements = Collections.emptyList();
        }
        List<ApkInfo> apkInfo =
                parseList(doc.getDocumentElement(), "apk-info", this::createApkInfo);
        return new GameCoreConfiguration(requirements, apkInfo);
    }

    private <T> List<T> parseList(Element element, String tagName, Function<Element, T> parser) {
        NodeList nodeList = element.getElementsByTagName(tagName);
        if (nodeList.getLength() != 1) {
            return null;
        }
        List<T> result = new ArrayList<>();
        NodeList children = nodeList.item(0).getChildNodes();
        for (int i = 0; i < children.getLength(); i++) {
            Node child = children.item(i);
            if (child.getNodeType() == Node.ELEMENT_NODE) {
                result.add(parser.apply((Element) child));
            }
        }
        return result;
    }

    private CertificationRequirements createCertificationRequirements(Element element) {
        if (!element.getTagName().equals("apk")) {
            throw new RuntimeException(
                    "Unexpected tag <"
                            + element.getNodeName()
                            + "> in <certification>.  Only <apk> is allowed." );
        }
        return new CertificationRequirements(
                getElement(element, Field.NAME),
                Float.parseFloat(getElement(element, Field.TARGET_FRAME_TIME)),
                Float.parseFloat(getElement(element, Field.MAX_JANK_RATE)),
                Integer.parseInt(getElement(element, Field.MAX_LOAD_TIME)));
    }

    private ApkInfo createApkInfo(Element element) {
        if (!element.getTagName().equals("apk")) {
            throw new RuntimeException(
                    "Unexpected tag <"
                            + element.getNodeName()
                            + "> in <apk-info>.  Only <apk> is allowed." );
        }

        List<ApkInfo.Argument> args = parseList(element, Field.ARGS.getTag(), this::createArgument);
        if (args == null) {
            args = Collections.emptyList();
        }

        return new ApkInfo(
                getElement(element, Field.NAME),
                getElement(element, Field.FILE_NAME),
                getElement(element, Field.PACKAGE_NAME),
                getElement(element, Field.ACTIVITY_NAME),
                getElement(element, Field.LAYER_NAME),
                getElement(element, Field.SCRIPT),
                args,
                Integer.parseInt(getElement(element, Field.LOAD_TIME)),
                Integer.parseInt(getElement(element, Field.RUN_TIME)),
                Boolean.parseBoolean(getElement(element, Field.EXPECT_INTENTS))
                );
    }

    private ApkInfo.Argument createArgument(Element element) {
        String type = element.getAttribute("type");
        if (type == null || type.isEmpty()) {
            type = "STRING";
        }
        return new ApkInfo.Argument(
                element.getTagName(),
                element.getTextContent(),
                ApkInfo.Argument.Type.valueOf(type.toUpperCase(Locale.US)));
    }

    private String getElement(Element element, Field field) {
        NodeList elements = element.getElementsByTagName(field.getTag());
        if (elements.getLength() > 0) {
            return elements.item(0).getTextContent();
        } else {
            return field.getDefaultValue();
        }
    }
}
