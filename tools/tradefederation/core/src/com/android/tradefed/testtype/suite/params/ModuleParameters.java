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
package com.android.tradefed.testtype.suite.params;

/** Special values associated with the suite "parameter" keys in the metadata of each module. */
public enum ModuleParameters {
    /** describes a parameterization based on app that should be installed in instant mode. */
    INSTANT_APP("instant_app", "instant_app_family"),
    NOT_INSTANT_APP("not_instant_app", "instant_app_family"),

    MULTI_ABI("multi_abi", "multi_abi_family"),
    NOT_MULTI_ABI("not_multi_abi", "multi_abi_family"),

    SECONDARY_USER("secondary_user", "secondary_user_family"),
    NOT_SECONDARY_USER("not_secondary_user", "secondary_user_family");

    public static final String INSTANT_APP_FAMILY = "instant_app_family";
    public static final String MULTI_ABI_FAMILY = "multi_abi_family";
    public static final String SECONDARY_USER_FAMILY = "secondary_user_family";
    public static final String[] FAMILY_LIST =
            new String[] {INSTANT_APP_FAMILY, MULTI_ABI_FAMILY, SECONDARY_USER_FAMILY};

    private final String mName;
    /** Defines whether several module parameters are associated and mutually exclusive. */
    private final String mFamily;

    private ModuleParameters(String name, String family) {
        mName = name;
        mFamily = family;
    }

    @Override
    public String toString() {
        return mName;
    }

    /** Returns the family of the Module Parameter. */
    public String getFamily() {
        return mFamily;
    }
}
