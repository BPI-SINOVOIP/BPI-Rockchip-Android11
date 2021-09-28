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

package com.android.xsdc.tag;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public abstract class XsdComplexType extends XsdType {
    final private XsdType base;
    final private List<XsdAttribute> attributes;
    final private List<XsdAttributeGroup> attributeGroups;
    final private List<XsdElement> elements;
    final private XsdGroup group;

    XsdComplexType(String name, XsdType base, List<XsdAttribute> attributes,
            List<XsdAttributeGroup> attributeGroups,
            List<XsdElement> elements, XsdGroup group) {
        super(name, null);
        this.base = base;
        this.attributes = Collections.unmodifiableList(
                attributes != null ? attributes : new ArrayList<>());
        this.attributeGroups = Collections.unmodifiableList(
                attributeGroups != null ? attributeGroups : new ArrayList<>());
        this.elements = Collections.unmodifiableList(
                elements != null ? elements : new ArrayList<>());
        this.group = group;
    }

    public XsdType getBase() {
        return base;
    }

    public List<XsdAttribute> getAttributes() {
        return attributes;
    }

    public List<XsdAttributeGroup> getAttributeGroups() {
        return attributeGroups;
    }

    public List<XsdElement> getElements() {
        return elements;
    }

    public XsdGroup getGroup() {
        return group;
    }
}
