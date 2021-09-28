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
package com.android.tv.common.buildtype;

/**
 * The provides a {@link BuildType} for selecting features in code.
 *
 * <p>This is considered an anti-pattern and new usages should be discouraged.
 */
public interface HasBuildType {

    /** Compile time constant for build type. */
    enum BuildType {
        AOSP,
        ENG,
        NO_JNI_TEST,
        PROD
    }

    /** @deprecated use injection instead. */
    @Deprecated
    BuildType getBuildType();
}
