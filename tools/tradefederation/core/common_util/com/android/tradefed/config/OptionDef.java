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
package com.android.tradefed.config;

import com.android.tradefed.build.BuildSerializedVersion;

import java.io.Serializable;

/** Holds the details of an {@link Option}. */
public final class OptionDef implements Serializable {
    private static final long serialVersionUID = BuildSerializedVersion.VERSION;

    public final String name;
    public final String key;
    public final String value;
    public final String source;
    public final String applicableObjectType;

    public OptionDef(String optionName, String optionValue, String source) {
        this(optionName, null, optionValue, source, null);
    }

    public OptionDef(String optionName, String optionKey, String optionValue, String source) {
        this(optionName, optionKey, optionValue, source, null);
    }

    public OptionDef(
            String optionName, String optionKey, String optionValue, String source, String type) {
        this.name = optionName;
        this.key = optionKey;
        this.value = optionValue;
        this.source = source;
        this.applicableObjectType = type;
    }
}
