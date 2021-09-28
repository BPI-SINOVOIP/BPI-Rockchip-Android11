/*
 * Copyright (C) 2020 The Android Open Source Project
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

import com.android.tools.layoutlib.annotations.Nullable;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.File;
import java.io.FileWriter;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;


public class ResourceConverter {

    /**
     * Convert the Android strings.xml into generic Java strings.properties.
     */
    public static void main(String[] args) throws Exception {
        System.out.println("Parsing input...");
        Map<String, String> map = loadStrings("./validator/resources/values.xml");
        System.out.println("Writing to output...");
        writeStrings(map, "./validator/resources/strings.properties");
        System.out.println("Finished converting.");
    }

    /**
     * Writes <name, value> pair to outputPath.
     */
    private static void writeStrings(Map<String, String> map, String outputPath) throws Exception {
        File output = new File(outputPath);
        output.createNewFile();
        FileWriter writer = new FileWriter(output);
        try {
            writer.write(getCopyRight());
            writer.write("\n");
            for (Entry<String, String> entry : map.entrySet()) {
                String name = entry.getKey();
                String value = entry.getValue();
                writer.write(name + " = " + value + "\n");
            }
        } finally {
            writer.close();
        }
    }

    /**
     * Very hacky parser for Android-understood-values.xml. It parses <string> </string>
     * tags, and retrieve the name and the value associated.
     *
     * @param path to .xml containing android strings
     * @return Map containing name and values.
     */
    @Nullable
    private static Map<String, String> loadStrings(String path)
            throws Exception {
        // Use ordered map to minimize changes to strings.properties in git.
        Map<String, String> toReturn = new LinkedHashMap<>();

        File file = new File(path);
        if (!file.exists()) {
            System.err.println("The input file "+ path + " does not exist. Terminating.");
            return toReturn;
        }

        DocumentBuilder documentBuilder = DocumentBuilderFactory
                .newInstance().newDocumentBuilder();
        Document document = documentBuilder.parse(file);
        NodeList nodeList = document.getElementsByTagName("string");
        for (int i = 0; i < nodeList.getLength(); i++) {
            Node node = nodeList.item(i);
            String name = node.getAttributes().getNamedItem("name").getNodeValue();

            StringBuilder valueBuilder = new StringBuilder();
            /**
             * This is a very hacky way to bypass "ns1:g" tag in android's .xml.
             * Ideally we'll read the tag from the parent and apply it here, but it being the
             * deep node list I'm not currently sure how to parse it safely. Might need to look
             * into IntelliJ PSI tree we have in Studio. But I didn't want to add unnecessary deps
             * to LayoutLib.
             *
             * It also means resource namespaces are rendered useless after conversion.
             */
            for (int j = 0; j < node.getChildNodes().getLength(); j++) {
                Node child = node.getChildNodes().item(j);
                if ("ns1:g".equals(child.getNodeName())) {
                    valueBuilder.append(child.getFirstChild().getNodeValue());
                } else {
                    valueBuilder.append(child.getNodeValue());
                }
            }
            toReturn.put(name, valueBuilder.toString());
        }
        return toReturn;
    }

    private static String getCopyRight() {
        return "\n" + "#\n" + "# Copyright (C) 2020 The Android Open Source Project\n" + "#\n" +
                "# Licensed under the Apache License, Version 2.0 (the \"License\");\n" +
                "# you may not use this file except in compliance with the License.\n" +
                "# You may obtain a copy of the License at\n" + "#\n" +
                "#      http://www.apache.org/licenses/LICENSE-2.0\n" + "#\n" +
                "# Unless required by applicable law or agreed to in writing, software\n" +
                "# distributed under the License is distributed on an \"AS IS\" BASIS,\n" +
                "# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n" +
                "# See the License for the specific language governing permissions and\n" +
                "# limitations under the License.\n" + "#";
    }
}
