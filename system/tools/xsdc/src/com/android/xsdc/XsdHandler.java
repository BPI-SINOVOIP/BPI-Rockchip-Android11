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

package com.android.xsdc;

import com.android.xsdc.tag.*;

import org.xml.sax.Attributes;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;
import java.util.stream.Collectors;

import javax.xml.namespace.QName;

public class XsdHandler extends DefaultHandler {
    private static class State {
        final String name;
        final Map<String, String> attributeMap;
        final List<XsdTag> tags;
        boolean deprecated;
        boolean finalValue;
        Nullability nullability;

        State(String name, Map<String, String> attributeMap) {
            this.name = name;
            this.attributeMap = Collections.unmodifiableMap(attributeMap);
            tags = new ArrayList<>();
            deprecated = false;
            finalValue = false;
            nullability = Nullability.UNKNOWN;
        }
    }

    private XmlSchema schema;
    private final Stack<State> stateStack;
    private final Map<String, String> namespaces;
    private Locator locator;
    private boolean documentationFlag;
    private boolean enumerationFlag;
    private List<XsdTag> enumTags;

    public XsdHandler() {
        stateStack = new Stack<>();
        namespaces = new HashMap<>();
        documentationFlag = false;
        enumerationFlag = false;
        enumTags = new ArrayList<>();
    }

    public XmlSchema getSchema() {
        return schema;
    }

    @Override
    public void setDocumentLocator(Locator locator) {
        this.locator = locator;
    }

    @Override
    public void startPrefixMapping(String prefix, String uri) {
        namespaces.put(prefix, uri);
    }

    @Override
    public void endPrefixMapping(String prefix) {
        namespaces.remove(prefix);
    }

    private QName parseQName(String str) throws XsdParserException {
        if (str == null) return null;
        String[] parsed = str.split(":");
        if (parsed.length == 2) {
            return new QName(namespaces.get(parsed[0]), parsed[1]);
        } else if (parsed.length == 1) {
            return new QName(null, str);
        }
        throw new XsdParserException(String.format("QName parse error : %s", str));
    }

    private List<QName> parseQNames(String str) throws XsdParserException {
        List<QName> qNames = new ArrayList<>();
        if (str == null) return qNames;
        String[] parsed = str.split("\\s+");
        for (String s : parsed) {
            qNames.add(parseQName(s));
        }
        return qNames;
    }

    @Override
    public void startElement(
            String uri, String localName, String qName, Attributes attributes) {
        // we need to copy attributes because it is mutable..
        Map<String, String> attributeMap = new HashMap<>();
        for (int i = 0; i < attributes.getLength(); ++i) {
            attributeMap.put(attributes.getLocalName(i), attributes.getValue(i));
        }
        if (!documentationFlag) {
            stateStack.push(new State(localName, attributeMap));
        }
        if (localName == "documentation") {
            documentationFlag = true;
        }
    }

    @Override
    public void endElement(String uri, String localName, String qName) throws SAXException {
        if (documentationFlag && localName != "documentation") {
            return;
        }
        try {
            State state = stateStack.pop();
            switch (state.name) {
                case "schema":
                    schema = makeSchema(state);
                    break;
                case "element":
                    stateStack.peek().tags.add(makeElement(state));
                    break;
                case "attribute":
                    stateStack.peek().tags.add(makeAttribute(state));
                    break;
                case "attributeGroup":
                    stateStack.peek().tags.add(makeAttributeGroup(state));
                    break;
                case "complexType":
                    stateStack.peek().tags.add(makeComplexType(state));
                    break;
                case "complexContent":
                    stateStack.peek().tags.add(makeComplexContent(state));
                    break;
                case "simpleContent":
                    stateStack.peek().tags.add(makeSimpleContent(state));
                    break;
                case "restriction":
                    if (enumerationFlag) {
                        stateStack.peek().tags.add(makeEnumRestriction(state));
                        enumerationFlag = false;
                    } else {
                        stateStack.peek().tags.add(makeGeneralRestriction(state));
                    }
                    break;
                case "extension":
                    stateStack.peek().tags.add(makeGeneralExtension(state));
                    break;
                case "simpleType":
                    stateStack.peek().tags.add(makeSimpleType(state));
                    break;
                case "list":
                    stateStack.peek().tags.add(makeSimpleTypeList(state));
                    break;
                case "union":
                    stateStack.peek().tags.add(makeSimpleTypeUnion(state));
                    break;
                case "sequence":
                    stateStack.peek().tags.addAll(makeSequence(state));
                    break;
                case "choice":
                    stateStack.peek().tags.addAll(makeChoice(state));
                    break;
                case "all":
                    stateStack.peek().tags.addAll(makeAll(state));
                    break;
                case "enumeration":
                    stateStack.peek().tags.add(makeEnumeration(state));
                    enumerationFlag = true;
                    break;
                case "group":
                    stateStack.peek().tags.add(makeGroup(state));
                    break;
                case "fractionDigits":
                case "length":
                case "maxExclusive":
                case "maxInclusive":
                case "maxLength":
                case "minExclusive":
                case "minInclusive":
                case "minLength":
                case "pattern":
                case "totalDigits":
                case "whiteSpace":
                    // Tags under simpleType <restriction>. They are ignored.
                    break;
                case "annotation":
                    stateStack.peek().deprecated = isDeprecated(state.attributeMap, state.tags,
                            stateStack.peek().deprecated);
                    stateStack.peek().finalValue = isFinalValue(state.attributeMap, state.tags,
                            stateStack.peek().finalValue);
                    stateStack.peek().nullability = getNullability(state.attributeMap, state.tags,
                            stateStack.peek().nullability);
                    break;
                case "appinfo":
                    // They function like comments, so are ignored.
                    break;
                case "documentation":
                    documentationFlag = false;
                    break;
                case "key":
                case "keyref":
                case "selector":
                case "field":
                case "unique":
                    // These tags are not related to xml parsing.
                    // They are using when validating xml files via xsd file.
                    // So they are ignored.
                    break;
                default:
                    throw new XsdParserException(String.format("unsupported tag : %s", state.name));
            }
        } catch (XsdParserException e) {
            throw new SAXException(
                    String.format("Line %d, Column %d - %s",
                            locator.getLineNumber(), locator.getColumnNumber(), e.getMessage()));
        }
    }

    private XmlSchema makeSchema(State state) {
        Map<String, XsdElement> elementMap = new LinkedHashMap<>();
        Map<String, XsdType> typeMap = new LinkedHashMap<>();
        Map<String, XsdAttribute> attrMap = new LinkedHashMap<>();
        Map<String, XsdAttributeGroup> attrGroupMap = new LinkedHashMap<>();
        Map<String, XsdGroup> groupMap = new LinkedHashMap<>();

        state.tags.addAll(enumTags);
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdElement) {
                elementMap.put(tag.getName(), (XsdElement) tag);
            } else if (tag instanceof XsdAttribute) {
                attrMap.put(tag.getName(), (XsdAttribute) tag);
            } else if (tag instanceof XsdAttributeGroup) {
                attrGroupMap.put(tag.getName(), (XsdAttributeGroup) tag);
            } else if (tag instanceof XsdType) {
                typeMap.put(tag.getName(), (XsdType) tag);
            } else if (tag instanceof XsdGroup) {
                groupMap.put(tag.getName(), (XsdGroup) tag);
            }
        }

        return new XmlSchema(elementMap, typeMap, attrMap, attrGroupMap, groupMap);
    }

    private XsdElement makeElement(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        QName typename = parseQName(state.attributeMap.get("type"));
        QName ref = parseQName(state.attributeMap.get("ref"));
        String isAbstract = state.attributeMap.get("abstract");
        String defVal = state.attributeMap.get("default");
        String substitutionGroup = state.attributeMap.get("substitutionGroup");
        String maxOccurs = state.attributeMap.get("maxOccurs");

        if ("true".equals(isAbstract)) {
            throw new XsdParserException("abstract element is not supported.");
        }
        if (defVal != null) {
            throw new XsdParserException("default value of an element is not supported.");
        }
        if (substitutionGroup != null) {
            throw new XsdParserException("substitution group of an element is not supported.");
        }

        boolean multiple = false;
        if (maxOccurs != null) {
            if (maxOccurs.equals("0")) return null;
            if (maxOccurs.equals("unbounded") || Integer.parseInt(maxOccurs) > 1) multiple = true;
        }

        XsdType type = null;
        if (typename != null) {
            type = new XsdType(null, typename);
        }
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdType) {
                type = (XsdType) tag;
            }
        }

        return setDeprecatedAndFinal(new XsdElement(name, ref, type, multiple), state.deprecated,
                state.finalValue, state.nullability);
    }

    private XsdAttribute makeAttribute(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        QName typename = parseQName(state.attributeMap.get("type"));
        QName ref = parseQName(state.attributeMap.get("ref"));
        String defVal = state.attributeMap.get("default");
        String use = state.attributeMap.get("use");

        if (use != null && use.equals("prohibited")) return null;

        XsdType type = null;
        if (typename != null) {
            type = new XsdType(null, typename);
        }
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdType) {
                type = (XsdType) tag;
            }
        }

        return setDeprecatedAndFinal(new XsdAttribute(name, ref, type), state.deprecated,
                state.finalValue, state.nullability);
    }

    private XsdAttributeGroup makeAttributeGroup(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        QName ref = parseQName(state.attributeMap.get("ref"));

        List<XsdAttribute> attributes = new ArrayList<>();
        List<XsdAttributeGroup> attributeGroups = new ArrayList<>();

        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdAttribute) {
                attributes.add((XsdAttribute) tag);
            } else if (tag instanceof XsdAttributeGroup) {
                attributeGroups.add((XsdAttributeGroup) tag);
            }
        }

        return setDeprecatedAndFinal(new XsdAttributeGroup(name, ref, attributes, attributeGroups),
                state.deprecated, state.finalValue, state.nullability);
    }

    private XsdGroup makeGroup(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        QName ref = parseQName(state.attributeMap.get("ref"));

        List<XsdElement> elements = new ArrayList<>();

        for (XsdTag tag: state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdElement) {
                elements.add((XsdElement) tag);
            }
        }

        return setDeprecatedAndFinal(new XsdGroup(name, ref, elements), state.deprecated,
                state.finalValue, state.nullability);
    }

    private XsdComplexType makeComplexType(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        String isAbstract = state.attributeMap.get("abstract");
        String mixed = state.attributeMap.get("mixed");

        if ("true".equals(isAbstract)) {
            throw new XsdParserException("abstract complex type is not supported.");
        }
        if ("true".equals(mixed)) {
            throw new XsdParserException("mixed option of a complex type is not supported.");
        }

        List<XsdAttribute> attributes = new ArrayList<>();
        List<XsdAttributeGroup> attributeGroups = new ArrayList<>();
        List<XsdElement> elements = new ArrayList<>();
        XsdComplexType type = null;
        XsdGroup group = null;

        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdAttribute) {
                attributes.add((XsdAttribute) tag);
            } else if (tag instanceof XsdAttributeGroup) {
                attributeGroups.add((XsdAttributeGroup) tag);
            } else if (tag instanceof XsdGroup) {
                group = (XsdGroup) tag;
            } else if (tag instanceof XsdElement) {
                elements.add((XsdElement) tag);
            } else if (tag instanceof XsdComplexContent) {
                XsdComplexContent child = (XsdComplexContent) tag;
                type = setDeprecatedAndFinal(new XsdComplexContent(name, child.getBase(),
                        child.getAttributes(), child.getAttributeGroups(),
                        child.getElements(), child.getGroup()), state.deprecated, state.finalValue,
                        state.nullability);
            } else if (tag instanceof XsdSimpleContent) {
                XsdSimpleContent child = (XsdSimpleContent) tag;
                type = setDeprecatedAndFinal(new XsdSimpleContent(name, child.getBase(),
                        child.getAttributes()), state.deprecated, state.finalValue,
                        state.nullability);
            }
        }

        return (type != null) ? type : setDeprecatedAndFinal(new XsdComplexContent(name, null,
                attributes, attributeGroups, elements, group), state.deprecated, state.finalValue,
                state.nullability);
    }

    private XsdComplexContent makeComplexContent(State state) throws XsdParserException {
        String mixed = state.attributeMap.get("mixed");
        if ("true".equals(mixed)) {
            throw new XsdParserException("mixed option of a complex content is not supported.");
        }

        XsdComplexContent content = null;
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdGeneralExtension) {
                XsdGeneralExtension extension = (XsdGeneralExtension) tag;
                content = new XsdComplexContent(null, extension.getBase(),
                        extension.getAttributes(), extension.getAttributeGroups(),
                        extension.getElements(), extension.getGroup());
            } else if (tag instanceof XsdGeneralRestriction) {
                XsdGeneralRestriction restriction = (XsdGeneralRestriction) tag;
                XsdType base = restriction.getBase();
                if (base.getRef() != null && base.getRef().getNamespaceURI().equals(
                        XsdConstants.XSD_NAMESPACE)) {
                    // restriction of base 'xsd:anyType' is equal to complex content definition
                    content = new XsdComplexContent(null, null, restriction.getAttributes(),
                            restriction.getAttributeGroups(), restriction.getElements(),
                            restriction.getGroup());
                } else {
                    // otherwise ignore restrictions
                    content = new XsdComplexContent(null, base, null, null, null, null);
                }
            }
        }

        return setDeprecatedAndFinal(content, state.deprecated, state.finalValue,
                state.nullability);
    }

    private XsdSimpleContent makeSimpleContent(State state) {
        XsdSimpleContent content = null;

        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdGeneralExtension) {
                XsdGeneralExtension extension = (XsdGeneralExtension) tag;
                content = new XsdSimpleContent(null, extension.getBase(),
                        extension.getAttributes());
            } else if (tag instanceof XsdGeneralRestriction) {
                XsdGeneralRestriction restriction = (XsdGeneralRestriction) tag;
                content = new XsdSimpleContent(null, restriction.getBase(), null);
            }
        }

        return setDeprecatedAndFinal(content, state.deprecated, state.finalValue,
                state.nullability);
    }

    private XsdGeneralRestriction makeGeneralRestriction(State state) throws XsdParserException {
        QName base = parseQName(state.attributeMap.get("base"));

        XsdType type = null;
        if (base != null) {
            type = new XsdType(null, base);
        }
        List<XsdAttribute> attributes = new ArrayList<>();
        List<XsdAttributeGroup> attributeGroups = new ArrayList<>();
        List<XsdElement> elements = new ArrayList<>();
        XsdGroup group = null;
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdAttribute) {
                attributes.add((XsdAttribute) tag);
            } else if (tag instanceof XsdAttributeGroup) {
                attributeGroups.add((XsdAttributeGroup) tag);
            } else if (tag instanceof XsdElement) {
                elements.add((XsdElement) tag);
            } else if (tag instanceof XsdGroup) {
                group = (XsdGroup) tag;
            }
        }

        return setDeprecatedAndFinal(new XsdGeneralRestriction(type, attributes, attributeGroups,
                elements, group), state.deprecated, state.finalValue, state.nullability);
    }

    private XsdGeneralExtension makeGeneralExtension(State state) throws XsdParserException {
        QName base = parseQName(state.attributeMap.get("base"));

        List<XsdAttribute> attributes = new ArrayList<>();
        List<XsdAttributeGroup> attributeGroups = new ArrayList<>();
        List<XsdElement> elements = new ArrayList<>();
        XsdGroup group = null;
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdAttribute) {
                attributes.add((XsdAttribute) tag);
            } else if (tag instanceof XsdAttributeGroup) {
                attributeGroups.add((XsdAttributeGroup) tag);
            } else if (tag instanceof XsdElement) {
                elements.add((XsdElement) tag);
            } else if (tag instanceof XsdGroup) {
                group = (XsdGroup) tag;
            }
        }
        return setDeprecatedAndFinal(new XsdGeneralExtension(new XsdType(null, base), attributes,
                attributeGroups, elements, group), state.deprecated, state.finalValue,
                state.nullability);
    }

    private XsdSimpleType makeSimpleType(State state) throws XsdParserException {
        String name = state.attributeMap.get("name");
        XsdSimpleType type = null;
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdList) {
                type = new XsdList(name, ((XsdList) tag).getItemType());
            } else if (tag instanceof XsdGeneralRestriction) {
                type = new XsdRestriction(name, ((XsdGeneralRestriction) tag).getBase(), null);
            } else if (tag instanceof XsdEnumRestriction) {
                if (name == null) {
                    throw new XsdParserException(
                            "The name of simpleType for enumeration must be set.");
                }
                type = new XsdRestriction(name, ((XsdEnumRestriction) tag).getBase(),
                        ((XsdEnumRestriction) tag).getEnums());
                enumTags.add(type);
            } else if (tag instanceof XsdUnion) {
                type = new XsdUnion(name, ((XsdUnion) tag).getMemberTypes());
            }
        }
        return setDeprecatedAndFinal(type, state.deprecated, state.finalValue, state.nullability);
    }

    private XsdList makeSimpleTypeList(State state) throws XsdParserException {
        QName itemTypeName = parseQName(state.attributeMap.get("itemType"));

        XsdType itemType = null;
        if (itemTypeName != null) {
            itemType = new XsdType(null, itemTypeName);
        }
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdType) {
                itemType = (XsdType) tag;
            }
        }
        return setDeprecatedAndFinal(new XsdList(null, itemType), state.deprecated,
                state.finalValue, state.nullability);
    }

    private XsdUnion makeSimpleTypeUnion(State state) throws XsdParserException {
        List<QName> memberTypeNames = parseQNames(state.attributeMap.get("memberTypes"));
        List<XsdType> memberTypes = memberTypeNames.stream().map(
                ref -> new XsdType(null, ref)).collect(Collectors.toList());

        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdType) {
                memberTypes.add((XsdType) tag);
            }
        }

        return setDeprecatedAndFinal(new XsdUnion(null, memberTypes), state.deprecated,
                state.finalValue, state.nullability);
    }

    private static List<XsdTag> makeSequence(State state) throws XsdParserException {
        String minOccurs = state.attributeMap.get("minOccurs");
        String maxOccurs = state.attributeMap.get("maxOccurs");

        if (minOccurs != null || maxOccurs != null) {
            throw new XsdParserException(
                    "minOccurs, maxOccurs options of a sequence is not supported");
        }

        List<XsdTag> elementsAndGroup = new ArrayList<>();
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdElement) {
                if (maxOccurs != null && (maxOccurs.equals("unbounded")
                        || Integer.parseInt(maxOccurs) > 1)) {
                    ((XsdElement)tag).setMultiple(true);
                }
                elementsAndGroup.add(tag);
            } else if (tag instanceof XsdGroup) {
                elementsAndGroup.add(tag);
            }
        }
        return elementsAndGroup;
    }

    private static List<XsdTag> makeChoice(State state) throws XsdParserException {
        String maxOccurs = state.attributeMap.get("maxOccurs");
        List<XsdTag> elementsAndGroup = new ArrayList<>();

        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdElement) {
                if (maxOccurs != null && (maxOccurs.equals("unbounded")
                        || Integer.parseInt(maxOccurs) > 1)) {
                    ((XsdElement)tag).setMultiple(true);
                }
                XsdElement element = (XsdElement)tag;
                elementsAndGroup.add((XsdTag) setDeprecatedAndFinal(new XsdChoice(element.getName(),
                        element.getRef(), element.getType(), element.isMultiple()),
                        element.isDeprecated(), element.isFinalValue(), element.getNullability()));
            } else if (tag instanceof XsdGroup) {
                elementsAndGroup.add(tag);
            }
        }
        return elementsAndGroup;
    }

    private static List<XsdElement> makeAll(State state) throws XsdParserException {
        List<XsdElement> elements = new ArrayList<>();
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdElement) {
                XsdElement element = (XsdElement)tag;
                elements.add(setDeprecatedAndFinal(new XsdAll(element.getName(), element.getRef(),
                        element.getType(), element.isMultiple()), element.isDeprecated(),
                        element.isFinalValue(), element.getNullability()));
            }
        }
        return elements;
    }

    private XsdEnumeration makeEnumeration(State state) throws XsdParserException {
        String value = state.attributeMap.get("value");
        return setDeprecatedAndFinal(new XsdEnumeration(value), state.deprecated,
                state.finalValue, state.nullability);
    }

    private XsdEnumRestriction makeEnumRestriction(State state) throws XsdParserException {
        QName base = parseQName(state.attributeMap.get("base"));

        XsdType type = null;
        if (base != null) {
            type = new XsdType(null, base);
        }
        List<XsdEnumeration> enums = new ArrayList<>();
        for (XsdTag tag : state.tags) {
            if (tag == null) continue;
            if (tag instanceof XsdEnumeration) {
                enums.add((XsdEnumeration) tag);
            }
        }

        return setDeprecatedAndFinal(new XsdEnumRestriction(type, enums), state.deprecated,
                state.finalValue, state.nullability);
    }

    private boolean isDeprecated(Map<String, String> attributeMap,List<XsdTag> tags,
            boolean deprecated) throws XsdParserException {
        String name = attributeMap.get("name");
        if ("Deprecated".equals(name)) {
            return true;
        }
        return deprecated;
    }

    private boolean isFinalValue(Map<String, String> attributeMap,List<XsdTag> tags,
            boolean finalValue) throws XsdParserException {
        String name = attributeMap.get("name");
        if ("final".equals(name)) {
            return true;
        }
        return finalValue;
    }

    private Nullability getNullability(Map<String, String> attributeMap,List<XsdTag> tags,
            Nullability nullability) throws XsdParserException {
        String name = attributeMap.get("name");
        if ("nullable".equals(name)) {
            return Nullability.NULLABLE;
        } else if ("nonnull".equals(name)) {
            return Nullability.NON_NULL;
        }
        return nullability;
    }

    private static <T extends XsdTag> T setDeprecatedAndFinal(T tag, boolean deprecated,
            boolean finalValue, Nullability nullability) {
        if (tag != null) {
            tag.setDeprecated(deprecated);
            tag.setFinalValue(finalValue);
            tag.setNullability(nullability);
        }
        return tag;
    }
}
