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
package android.signature.cts;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Represents one class member parsed from the reader of dex signatures.
 */
public abstract class DexMember {
    private final String mName;
    private final String mClassDescriptor;
    private final String mType;
    private final Set<String> mFlags;

    protected DexMember(String className, String name, String type, String[] flags) {
        mName = name;
        mClassDescriptor = className;
        mType = type;
        mFlags = flags == null ? Collections.emptySet() : new HashSet<>(Arrays.asList(flags));
    }

    public String getName() {
        return mName;
    }

    public String getDexClassName() {
        return mClassDescriptor;
    }

    public String getJavaClassName() {
        return dexToJavaType(mClassDescriptor);
    }

    public String getDexType() {
        return mType;
    }

    public String getJavaType() {
        return dexToJavaType(mType);
    }

    public Set<String> getHiddenapiFlags() {
        return mFlags;
    }

    /**
     * Converts `type` to a Java type.
     */
    protected static String dexToJavaType(String type) {
        String javaDimension = "";
        while (type.startsWith("[")) {
            javaDimension += "[]";
            type = type.substring(1);
        }

        String javaType = null;
        if ("V".equals(type)) {
            javaType = "void";
        } else if ("Z".equals(type)) {
            javaType = "boolean";
        } else if ("B".equals(type)) {
            javaType = "byte";
        } else if ("C".equals(type)) {
            javaType = "char";
        } else if ("S".equals(type)) {
            javaType = "short";
        } else if ("I".equals(type)) {
            javaType = "int";
        } else if ("J".equals(type)) {
            javaType = "long";
        } else if ("F".equals(type)) {
            javaType = "float";
        } else if ("D".equals(type)) {
            javaType = "double";
        } else if (type.startsWith("L") && type.endsWith(";")) {
            javaType = type.substring(1, type.length() - 1).replace('/', '.');
        } else {
            throw new IllegalStateException("Unexpected type " + type);
        }

        return javaType + javaDimension;
    }
}
