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

package com.android.xsdc.java;

import com.android.xsdc.CodeWriter;
import com.android.xsdc.FileSystem;
import com.android.xsdc.XmlSchema;
import com.android.xsdc.XsdConstants;
import com.android.xsdc.tag.*;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.xml.namespace.QName;

public class JavaCodeGenerator {
    private XmlSchema xmlSchema;
    private String packageName;
    private Map<String, JavaSimpleType> javaSimpleTypeMap;

    public JavaCodeGenerator(XmlSchema xmlSchema, String packageName)
            throws JavaCodeGeneratorException {
        this.xmlSchema = xmlSchema;
        this.packageName = packageName;

        // class naming validation
        {
            Set<String> nameSet = new HashSet<>();
            nameSet.add("XmlParser");
            for (XsdType type : xmlSchema.getTypeMap().values()) {
                if ((type instanceof XsdComplexType) || (type instanceof XsdRestriction &&
                        ((XsdRestriction)type).getEnums() != null)) {
                    String name = Utils.toClassName(type.getName());
                    if (nameSet.contains(name)) {
                        throw new JavaCodeGeneratorException(
                                String.format("duplicate class name : %s", name));
                    }
                    nameSet.add(name);
                }
            }
            for (XsdElement element : xmlSchema.getElementMap().values()) {
                XsdType type = element.getType();
                if (type.getRef() == null && type instanceof XsdComplexType) {
                    String name = Utils.toClassName(element.getName());
                    if (nameSet.contains(name)) {
                        throw new JavaCodeGeneratorException(
                                String.format("duplicate class name : %s", name));
                    }
                    nameSet.add(name);
                }
            }
        }

        javaSimpleTypeMap = new HashMap<>();
        for (XsdType type : xmlSchema.getTypeMap().values()) {
            if (type instanceof XsdSimpleType) {
                XsdType refType = new XsdType(null, new QName(type.getName()));
                parseSimpleType(refType, true);
            }
        }
    }

    public void print(FileSystem fs)
            throws JavaCodeGeneratorException, IOException {
        for (XsdType type : xmlSchema.getTypeMap().values()) {
            if (type instanceof XsdComplexType) {
                String name = Utils.toClassName(type.getName());
                XsdComplexType complexType = (XsdComplexType) type;
                try (CodeWriter out = new CodeWriter(fs.getPrintWriter(name + ".java"))) {
                    out.printf("package %s;\n\n", packageName);
                    printClass(out, name, complexType, "");
                }
            } else if (type instanceof XsdRestriction &&
                    ((XsdRestriction)type).getEnums() != null) {
                String name = Utils.toClassName(type.getName());
                XsdRestriction restrictionType = (XsdRestriction) type;
                try (CodeWriter out = new CodeWriter(fs.getPrintWriter(name + ".java"))) {
                    out.printf("package %s;\n\n", packageName);
                    printEnumClass(out, name, restrictionType);
                }
            }
        }
        for (XsdElement element : xmlSchema.getElementMap().values()) {
            XsdType type = element.getType();
            if (type.getRef() == null && type instanceof XsdComplexType) {
                String name = Utils.toClassName(element.getName());
                XsdComplexType complexType = (XsdComplexType) type;
                try (CodeWriter out = new CodeWriter(fs.getPrintWriter(name + ".java"))) {
                    out.printf("package %s;\n\n", packageName);
                    printClass(out, name, complexType, "");
                }
            }
        }
        try (CodeWriter out = new CodeWriter(fs.getPrintWriter("XmlParser.java"))) {
            printXmlParser(out);
        }
    }

    private void printEnumClass(CodeWriter out, String name, XsdRestriction restrictionType)
            throws JavaCodeGeneratorException {
        if (restrictionType.isDeprecated()) {
            out.printf("@java.lang.Deprecated\n");
        }
        out.printf("public enum %s {", name);
        List<XsdEnumeration> enums = restrictionType.getEnums();

        for (XsdEnumeration tag : enums) {
            if (tag.isDeprecated()) {
                out.printf("@java.lang.Deprecated\n");
            }
            String value = tag.getValue();
            if ("".equals(value)) {
                value = "EMPTY";
            }
            out.printf("\n%s(\"%s\"),", Utils.toEnumName(value), tag.getValue());
        }
        out.printf(";\n\n");
        out.printf("private final String rawName;\n\n");
        out.printf("%s(String rawName) {\n"
                + "this.rawName = rawName;\n"
                + "}\n\n", name);
        out.printf("public String getRawName() {\n"
                + "return rawName;\n"
                + "}\n");
        out.println("}");
    }

    private void printClass(CodeWriter out, String name, XsdComplexType complexType,
            String nameScope) throws JavaCodeGeneratorException {
        assert name != null;
        // need element, attribute name duplicate validation?

        String baseName = getBaseName(complexType);
        JavaSimpleType valueType = (complexType instanceof XsdSimpleContent) ?
                getValueType((XsdSimpleContent) complexType, false) : null;

        String finalString = getFinalString(complexType.isFinalValue());
        if (complexType.isDeprecated()) {
            out.printf("@java.lang.Deprecated\n");
        }
        if (nameScope.isEmpty()) {
            out.printf("public%s class %s ", finalString, name);
        } else {
            out.printf("public%s static class %s ", finalString, name);
        }
        if (baseName != null) {
            out.printf("extends %s {\n", baseName);
        } else {
            out.println("{");
        }

        // parse types for elements and attributes
        List<JavaType> elementTypes = new ArrayList<>();
        List<XsdElement> elements = new ArrayList<>();
        elements.addAll(getAllElements(complexType.getGroup()));
        elements.addAll(complexType.getElements());

        for (XsdElement element : elements) {
            JavaType javaType;
            XsdElement elementValue = resolveElement(element);
            if (element.getRef() == null && element.getType().getRef() == null
                    && element.getType() instanceof XsdComplexType) {
                // print inner class for anonymous types
                String innerName = Utils.toClassName(getElementName(element));
                XsdComplexType innerType = (XsdComplexType) element.getType();
                String innerNameScope = nameScope + name + ".";
                printClass(out, innerName, innerType, innerNameScope);
                out.println();
                javaType = new JavaComplexType(innerNameScope + innerName);
            } else {
                javaType = parseType(elementValue.getType(), getElementName(elementValue));
            }
            elementTypes.add(javaType);
        }
        List<JavaSimpleType> attributeTypes = new ArrayList<>();
        List<XsdAttribute> attributes =  new ArrayList<>();
        for (XsdAttributeGroup attributeGroup : complexType.getAttributeGroups()) {
            attributes.addAll(getAllAttributes(resolveAttributeGroup(attributeGroup)));
        }
        attributes.addAll(complexType.getAttributes());

        for (XsdAttribute attribute : attributes) {
            XsdType type = resolveAttribute(attribute).getType();
            attributeTypes.add(parseSimpleType(type, false));
        }

        // print member variables
        for (int i = 0; i < elementTypes.size(); ++i) {
            JavaType type = elementTypes.get(i);
            XsdElement element = elements.get(i);
            XsdElement elementValue = resolveElement(element);
            String typeName = element.isMultiple() ? String.format("java.util.List<%s>",
                    type.getNullableName()) : type.getName();
            out.printf("%sprivate %s %s;\n", getNullabilityString(element.getNullability()),
                    typeName, Utils.toVariableName(getElementName(elementValue)));
        }
        for (int i = 0; i < attributeTypes.size(); ++i) {
            JavaType type = attributeTypes.get(i);
            XsdAttribute attribute = resolveAttribute(attributes.get(i));
            out.printf("%sprivate %s %s;\n", getNullabilityString(attribute.getNullability()),
                    type.getName(), Utils.toVariableName(attribute.getName()));
        }
        if (valueType != null) {
            out.printf("private %s value;\n", valueType.getName());
        }

        // print getters and setters
        for (int i = 0; i < elementTypes.size(); ++i) {
            JavaType type = elementTypes.get(i);
            XsdElement element = elements.get(i);
            XsdElement elementValue = resolveElement(element);
            printGetterAndSetter(out, type, Utils.toVariableName(getElementName(elementValue)),
                    element.isMultiple(), element);
        }
        for (int i = 0; i < attributeTypes.size(); ++i) {
            JavaType type = attributeTypes.get(i);
            XsdAttribute attribute = resolveAttribute(attributes.get(i));
            printGetterAndSetter(out, type, Utils.toVariableName(attribute.getName()), false,
                    attribute);
        }
        if (valueType != null) {
            printGetterAndSetter(out, valueType, "value", false, null);
        }

        out.println();
        printParser(out, nameScope + name, complexType);

        out.println("}");
    }

    private void printParser(CodeWriter out, String name, XsdComplexType complexType)
            throws JavaCodeGeneratorException {
        JavaSimpleType baseValueType = (complexType instanceof XsdSimpleContent) ?
                getValueType((XsdSimpleContent) complexType, true) : null;
        List<XsdElement> allElements = new ArrayList<>();
        List<XsdAttribute> allAttributes = new ArrayList<>();
        stackComponents(complexType, allElements, allAttributes);

        // parse types for elements and attributes
        List<JavaType> allElementTypes = new ArrayList<>();
        for (XsdElement element : allElements) {
            XsdElement elementValue = resolveElement(element);
            JavaType javaType = parseType(elementValue.getType(), elementValue.getName());
            allElementTypes.add(javaType);
        }
        List<JavaSimpleType> allAttributeTypes = new ArrayList<>();
        for (XsdAttribute attribute : allAttributes) {
            XsdType type = resolveAttribute(attribute).getType();
            allAttributeTypes.add(parseSimpleType(type, false));
        }

        out.printf("static %s read(org.xmlpull.v1.XmlPullParser parser) " +
                "throws org.xmlpull.v1.XmlPullParserException, java.io.IOException, " +
                "javax.xml.datatype.DatatypeConfigurationException {\n", name);

        out.printf("%s instance = new %s();\n"
                + "String raw = null;\n", name, name);
        for (int i = 0; i < allAttributes.size(); ++i) {
            JavaType type = allAttributeTypes.get(i);
            XsdAttribute attribute = resolveAttribute(allAttributes.get(i));
            String variableName = Utils.toVariableName(attribute.getName());
            out.printf("raw = parser.getAttributeValue(null, \"%s\");\n"
                    + "if (raw != null) {\n", attribute.getName());
            out.print(type.getParsingExpression());
            out.printf("instance.set%s(value);\n"
                    + "}\n", Utils.capitalize(variableName));
        }

        if (baseValueType != null) {
            out.print("raw = XmlParser.readText(parser);\n"
                    + "if (raw != null) {\n");
            out.print(baseValueType.getParsingExpression());
            out.print("instance.setValue(value);\n"
                    + "}\n");
        } else if (!allElements.isEmpty()) {
            out.print("int outerDepth = parser.getDepth();\n"
                    + "int type;\n"
                    + "while ((type=parser.next()) != org.xmlpull.v1.XmlPullParser.END_DOCUMENT\n"
                    + "        && type != org.xmlpull.v1.XmlPullParser.END_TAG) {\n"
                    + "if (parser.getEventType() != org.xmlpull.v1.XmlPullParser.START_TAG) "
                    + "continue;\n"
                    + "String tagName = parser.getName();\n");
            for (int i = 0; i < allElements.size(); ++i) {
                JavaType type = allElementTypes.get(i);
                XsdElement element = allElements.get(i);
                XsdElement elementValue = resolveElement(element);
                String variableName = Utils.toVariableName(getElementName(elementValue));
                out.printf("if (tagName.equals(\"%s\")) {\n", elementValue.getName());
                if (type instanceof JavaSimpleType) {
                    out.print("raw = XmlParser.readText(parser);\n");
                }
                out.print(type.getParsingExpression());
                if (element.isMultiple()) {
                    out.printf("instance.get%s().add(value);\n",
                            Utils.capitalize(variableName));
                } else {
                    out.printf("instance.set%s(value);\n",
                            Utils.capitalize(variableName));
                }
                out.printf("} else ");
            }
            out.print("{\n"
                    + "XmlParser.skip(parser);\n"
                    + "}\n"
                    + "}\n");
            out.printf("if (type != org.xmlpull.v1.XmlPullParser.END_TAG) {\n"
                    + "throw new javax.xml.datatype.DatatypeConfigurationException(\"%s is not closed\");\n"
                    + "}\n", name);
        } else {
            out.print("XmlParser.skip(parser);\n");
        }
        out.print("return instance;\n"
                + "}\n");
    }

    private void printGetterAndSetter(CodeWriter out, JavaType type, String variableName,
            boolean isMultiple, XsdTag tag) {
        String typeName = isMultiple ? String.format("java.util.List<%s>", type.getNullableName())
                : type.getName();
        boolean deprecated = tag == null ? false : tag.isDeprecated();
        boolean finalValue = tag == null ? false : tag.isFinalValue();
        Nullability nullability = tag == null ? Nullability.UNKNOWN : tag.getNullability();
        out.println();
        if (deprecated) {
            out.printf("@java.lang.Deprecated\n");
        }
        out.printf("public%s %s%s get%s() {\n", getFinalString(finalValue),
                getNullabilityString(nullability), typeName, Utils.capitalize(variableName));
        if (isMultiple) {
            out.printf("if (%s == null) {\n"
                    + "%s = new java.util.ArrayList<>();\n"
                    + "}\n", variableName, variableName);
        }
        out.printf("return %s;\n"
                + "}\n", variableName);

        if (isMultiple) return;
        out.println();
        if (deprecated) {
            out.printf("@java.lang.Deprecated\n");
        }
        out.printf("public%s void set%s(%s%s %s) {\n"
                        + "this.%s = %s;\n"
                        + "}\n",
                getFinalString(finalValue), Utils.capitalize(variableName),
                getNullabilityString(nullability), typeName, variableName,
                variableName, variableName);
    }

    private void printXmlParser(CodeWriter out) throws JavaCodeGeneratorException {
        out.printf("package %s;\n", packageName);
        out.println();
        out.println("public class XmlParser {");

        boolean isMultiRootElement = xmlSchema.getElementMap().values().size() > 1;
        for (XsdElement element : xmlSchema.getElementMap().values()) {
            JavaType javaType = parseType(element.getType(), element.getName());
            out.printf("public static %s read%s(java.io.InputStream in)"
                + " throws org.xmlpull.v1.XmlPullParserException, java.io.IOException, "
                + "javax.xml.datatype.DatatypeConfigurationException {\n"
                + "org.xmlpull.v1.XmlPullParser parser = org.xmlpull.v1.XmlPullParserFactory"
                + ".newInstance().newPullParser();\n"
                + "parser.setFeature(org.xmlpull.v1.XmlPullParser.FEATURE_PROCESS_NAMESPACES, "
                + "true);\n"
                + "parser.setInput(in, null);\n"
                + "parser.nextTag();\n"
                + "String tagName = parser.getName();\n"
                + "String raw = null;\n", javaType.getName(),
                isMultiRootElement ? Utils.capitalize(javaType.getName()) : "");
            out.printf("if (tagName.equals(\"%s\")) {\n", element.getName());
            if (javaType instanceof JavaSimpleType) {
                out.print("raw = XmlParser.readText(parser);\n");
            }
            out.print(javaType.getParsingExpression());
            out.print("return value;\n"
                    + "}\n"
                    + "return null;\n"
                    + "}\n");
            out.println();
        }

        out.print(
                "public static java.lang.String readText(org.xmlpull.v1.XmlPullParser parser)"
                        + " throws org.xmlpull.v1.XmlPullParserException, java.io.IOException {\n"
                        + "String result = \"\";\n"
                        + "if (parser.next() == org.xmlpull.v1.XmlPullParser.TEXT) {\n"
                        + "    result = parser.getText();\n"
                        + "    parser.nextTag();\n"
                        + "}\n"
                        + "return result;\n"
                        + "}\n");
        out.println();

        out.print(
                "public static void skip(org.xmlpull.v1.XmlPullParser parser)"
                        + " throws org.xmlpull.v1.XmlPullParserException, java.io.IOException {\n"
                        + "if (parser.getEventType() != org.xmlpull.v1.XmlPullParser.START_TAG) {\n"
                        + "    throw new IllegalStateException();\n"
                        + "}\n"
                        + "int depth = 1;\n"
                        + "while (depth != 0) {\n"
                        + "    switch (parser.next()) {\n"
                        + "        case org.xmlpull.v1.XmlPullParser.END_TAG:\n"
                        + "            depth--;\n"
                        + "            break;\n"
                        + "        case org.xmlpull.v1.XmlPullParser.START_TAG:\n"
                        + "            depth++;\n"
                        + "            break;\n"
                        + "    }\n"
                        + "}\n"
                        + "}\n");

        out.println("}");
    }

    private String getElementName(XsdElement element) {
        if (element instanceof XsdChoice) {
            return element.getName() + "_optional";
        } else if (element instanceof XsdAll) {
            return element.getName() + "_all";
        }
        return element.getName();
    }

    private String getFinalString(boolean finalValue) {
        if (finalValue) {
          return " final";
        }
        return "";
    }

    private String getNullabilityString(Nullability nullability) {
        if (nullability == Nullability.NON_NULL) {
            return "@android.annotation.NonNull ";
        } else if (nullability == Nullability.NULLABLE) {
            return "@android.annotation.Nullable ";
        }
        return "";
    }

    private void stackComponents(XsdComplexType complexType, List<XsdElement> elements,
            List<XsdAttribute> attributes) throws JavaCodeGeneratorException {
        if (complexType.getBase() != null) {
            QName baseRef = complexType.getBase().getRef();
            if (baseRef != null && !baseRef.getNamespaceURI().equals(XsdConstants.XSD_NAMESPACE)) {
                XsdType parent = getType(baseRef.getLocalPart());
                if (parent instanceof XsdComplexType) {
                    stackComponents((XsdComplexType) parent, elements, attributes);
                }
            }
        }
        elements.addAll(getAllElements(complexType.getGroup()));
        elements.addAll(complexType.getElements());
        for (XsdAttributeGroup attributeGroup : complexType.getAttributeGroups()) {
            attributes.addAll(getAllAttributes(resolveAttributeGroup(attributeGroup)));
        }
        attributes.addAll(complexType.getAttributes());
    }

    private List<XsdAttribute> getAllAttributes(XsdAttributeGroup attributeGroup)
            throws JavaCodeGeneratorException {
        List<XsdAttribute> attributes = new ArrayList<>();
        for (XsdAttributeGroup attrGroup : attributeGroup.getAttributeGroups()) {
            attributes.addAll(getAllAttributes(resolveAttributeGroup(attrGroup)));
        }
        attributes.addAll(attributeGroup.getAttributes());
        return attributes;
    }

    private List<XsdElement> getAllElements(XsdGroup group) throws JavaCodeGeneratorException {
        List<XsdElement> elements = new ArrayList<>();
        if (group == null) {
            return elements;
        }
        elements.addAll(getAllElements(resolveGroup(group)));
        elements.addAll(group.getElements());
        return elements;
    }

    private String getBaseName(XsdComplexType complexType) throws JavaCodeGeneratorException {
        if (complexType.getBase() == null) return null;
        if (complexType.getBase().getRef().getNamespaceURI().equals(XsdConstants.XSD_NAMESPACE)) {
            return null;
        }
        XsdType base = getType(complexType.getBase().getRef().getLocalPart());
        if (base instanceof XsdComplexType) {
            return Utils.toClassName(base.getName());
        }
        return null;
    }

    private JavaSimpleType getValueType(XsdSimpleContent simpleContent, boolean traverse)
            throws JavaCodeGeneratorException {
        assert simpleContent.getBase() != null;
        QName baseRef = simpleContent.getBase().getRef();
        assert baseRef != null;
        if (baseRef.getNamespaceURI().equals(XsdConstants.XSD_NAMESPACE)) {
            return predefinedType(baseRef.getLocalPart());
        } else {
            XsdType parent = getType(baseRef.getLocalPart());
            if (parent instanceof XsdSimpleType) {
                return parseSimpleTypeReference(baseRef, false);
            }
            if (!traverse) return null;
            if (parent instanceof XsdSimpleContent) {
                return getValueType((XsdSimpleContent) parent, true);
            } else {
                throw new JavaCodeGeneratorException(
                        String.format("base not simple : %s", baseRef.getLocalPart()));
            }
        }
    }

    private JavaType parseType(XsdType type, String defaultName) throws JavaCodeGeneratorException {
        if (type.getRef() != null) {
            String name = type.getRef().getLocalPart();
            if (type.getRef().getNamespaceURI().equals(XsdConstants.XSD_NAMESPACE)) {
                return predefinedType(name);
            } else {
                XsdType typeValue = getType(name);
                if (typeValue instanceof XsdSimpleType) {
                    return parseSimpleTypeReference(type.getRef(), false);
                }
                return parseType(typeValue, name);
            }
        }
        if (type instanceof XsdComplexType) {
            return new JavaComplexType(Utils.toClassName(defaultName));
        } else if (type instanceof XsdSimpleType) {
            return parseSimpleTypeValue((XsdSimpleType) type, false);
        } else {
            throw new JavaCodeGeneratorException(
                    String.format("unknown type name : %s", defaultName));
        }
    }

    private JavaSimpleType parseSimpleType(XsdType type, boolean traverse)
            throws JavaCodeGeneratorException {
        if (type.getRef() != null) {
            return parseSimpleTypeReference(type.getRef(), traverse);
        } else {
            return parseSimpleTypeValue((XsdSimpleType) type, traverse);
        }
    }

    private JavaSimpleType parseSimpleTypeReference(QName typeRef, boolean traverse)
            throws JavaCodeGeneratorException {
        assert typeRef != null;
        String typeName = typeRef.getLocalPart();
        if (typeRef.getNamespaceURI().equals(XsdConstants.XSD_NAMESPACE)) {
            return predefinedType(typeName);
        }
        if (javaSimpleTypeMap.containsKey(typeName)) {
            return javaSimpleTypeMap.get(typeName);
        } else if (traverse) {
            XsdSimpleType simpleType = getSimpleType(typeName);
            JavaSimpleType ret = parseSimpleTypeValue(simpleType, true);
            javaSimpleTypeMap.put(typeName, ret);
            return ret;
        } else {
            throw new JavaCodeGeneratorException(String.format("unknown type name : %s", typeName));
        }
    }

    private JavaSimpleType parseSimpleTypeValue(XsdSimpleType simpleType, boolean traverse)
            throws JavaCodeGeneratorException {
        if (simpleType instanceof XsdList) {
            XsdList list = (XsdList) simpleType;
            return parseSimpleType(list.getItemType(), traverse).newListType();
        } else if (simpleType instanceof XsdRestriction) {
            // we don't consider any restrictions.
            XsdRestriction restriction = (XsdRestriction) simpleType;
            if (restriction.getEnums() != null) {
                String name = Utils.toClassName(restriction.getName());
                return new JavaSimpleType(name, name + ".valueOf(%s.replace(\".\", \"_\")."
                        + "replaceAll(\"[^A-Za-z0-9_]\", \"\"))", false);
            }
            return parseSimpleType(restriction.getBase(), traverse);
        } else if (simpleType instanceof XsdUnion) {
            // unions are almost always interpreted as java.lang.String
            // Exceptionally, if any of member types of union are 'list', then we interpret it as
            // List<String>
            XsdUnion union = (XsdUnion) simpleType;
            for (XsdType memberType : union.getMemberTypes()) {
                if (parseSimpleType(memberType, traverse).isList()) {
                    return new JavaSimpleType("java.lang.String", "%s", true);
                }
            }
            return new JavaSimpleType("java.lang.String", "%s", false);
        } else {
            // unreachable
            throw new IllegalStateException("unknown simple type");
        }
    }

    private XsdElement resolveElement(XsdElement element) throws JavaCodeGeneratorException {
        if (element.getRef() == null) return element;
        String name = element.getRef().getLocalPart();
        XsdElement ret = xmlSchema.getElementMap().get(name);
        if (ret != null) return ret;
        throw new JavaCodeGeneratorException(String.format("no element named : %s", name));
    }

    private XsdGroup resolveGroup(XsdGroup group) throws JavaCodeGeneratorException {
        if (group.getRef() == null) return null;
        String name = group.getRef().getLocalPart();
        XsdGroup ret = xmlSchema.getGroupMap().get(name);
        if (ret != null) return ret;
        throw new JavaCodeGeneratorException(String.format("no group named : %s", name));
    }

    private XsdAttribute resolveAttribute(XsdAttribute attribute)
            throws JavaCodeGeneratorException {
        if (attribute.getRef() == null) return attribute;
        String name = attribute.getRef().getLocalPart();
        XsdAttribute ret = xmlSchema.getAttributeMap().get(name);
        if (ret != null) return ret;
        throw new JavaCodeGeneratorException(String.format("no attribute named : %s", name));
    }

    private XsdAttributeGroup resolveAttributeGroup(XsdAttributeGroup attributeGroup)
            throws JavaCodeGeneratorException {
        if (attributeGroup.getRef() == null) return attributeGroup;
        String name = attributeGroup.getRef().getLocalPart();
        XsdAttributeGroup ret = xmlSchema.getAttributeGroupMap().get(name);
        if (ret != null) return ret;
        throw new JavaCodeGeneratorException(String.format("no attribute group named : %s", name));
    }

    private XsdType getType(String name) throws JavaCodeGeneratorException {
        XsdType type = xmlSchema.getTypeMap().get(name);
        if (type != null) return type;
        throw new JavaCodeGeneratorException(String.format("no type named : %s", name));
    }

    private XsdSimpleType getSimpleType(String name) throws JavaCodeGeneratorException {
        XsdType type = getType(name);
        if (type instanceof XsdSimpleType) return (XsdSimpleType) type;
        throw new JavaCodeGeneratorException(String.format("not a simple type : %s", name));
    }

    private static JavaSimpleType predefinedType(String name) throws JavaCodeGeneratorException {
        switch (name) {
            case "string":
            case "token":
            case "normalizedString":
            case "language":
            case "ENTITY":
            case "ID":
            case "Name":
            case "NCName":
            case "NMTOKEN":
            case "anyURI":
            case "anyType":
            case "QName":
            case "NOTATION":
            case "IDREF":
                return new JavaSimpleType("java.lang.String", "%s", false);
            case "ENTITIES":
            case "NMTOKENS":
            case "IDREFS":
                return new JavaSimpleType("java.lang.String", "%s", true);
            case "date":
            case "dateTime":
            case "time":
            case "gDay":
            case "gMonth":
            case "gYear":
            case "gMonthDay":
            case "gYearMonth":
                return new JavaSimpleType("javax.xml.datatype.XMLGregorianCalendar",
                        "javax.xml.datatype.DatatypeFactory.newInstance()"
                                + ".newXMLGregorianCalendar(%s)",
                        false);
            case "duration":
                return new JavaSimpleType("javax.xml.datatype.Duration",
                        "javax.xml.datatype.DatatypeFactory.newInstance().newDuration(%s)", false);
            case "decimal":
                return new JavaSimpleType("java.math.BigDecimal", "new java.math.BigDecimal(%s)",
                        false);
            case "integer":
            case "negativeInteger":
            case "nonNegativeInteger":
            case "positiveInteger":
            case "nonPositiveInteger":
            case "unsignedLong":
                return new JavaSimpleType("java.math.BigInteger", "new java.math.BigInteger(%s)",
                        false);
            case "long":
            case "unsignedInt":
                return new JavaSimpleType("long", "java.lang.Long", "Long.parseLong(%s)", false);
            case "int":
            case "unsignedShort":
                return new JavaSimpleType("int", "java.lang.Integer", "Integer.parseInt(%s)",
                        false);
            case "short":
            case "unsignedByte":
                return new JavaSimpleType("short", "java.lang.Short", "Short.parseShort(%s)",
                        false);
            case "byte":
                return new JavaSimpleType("byte", "java.lang.Byte", "Byte.parseByte(%s)", false);
            case "boolean":
                return new JavaSimpleType("boolean", "java.lang.Boolean",
                        "Boolean.parseBoolean(%s)", false);
            case "double":
                return new JavaSimpleType("double", "java.lang.Double", "Double.parseDouble(%s)",
                        false);
            case "float":
                return new JavaSimpleType("float", "java.lang.Float", "Float.parseFloat(%s)",
                        false);
            case "base64Binary":
                return new JavaSimpleType("byte[]", "java.util.Base64.getDecoder().decode(%s)",
                        false);
            case "hexBinary":
                return new JavaSimpleType("java.math.BigInteger",
                        "new java.math.BigInteger(%s, 16)", false);
        }
        throw new JavaCodeGeneratorException("unknown xsd predefined type : " + name);
    }
}
