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

import javax.xml.namespace.QName;

public abstract class XsdTag {
    final private String name;
    final private QName ref;
    private boolean deprecated;
    private boolean finalValue;
    private Nullability nullability;

    XsdTag(String name, QName ref) {
        this.name = name;
        this.ref = ref;
        this.deprecated = false;
        this.finalValue = false;
        this.nullability = Nullability.UNKNOWN;
    }

    public String getName() {
        return name;
    }

    public QName getRef() {
        return ref;
    }

    public boolean isFinalValue() {
        return finalValue;
    }

    public void setFinalValue(boolean finalValue) {
        this.finalValue = finalValue;
    }

    public boolean isDeprecated() {
        return deprecated;
    }

    public void setDeprecated(boolean deprecated) {
        this.deprecated = deprecated;
    }

    public Nullability getNullability() {
        return nullability;
    }

    public void setNullability(Nullability nullability) {
        this.nullability = nullability;
    }
}
