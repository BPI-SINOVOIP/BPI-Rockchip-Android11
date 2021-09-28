/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.inputmethodservice.cts.common;

/**
 * Constant table for test IME 2.
 */
public final class Ime2Constants {

    // This is constants holding class, can't instantiate.
    private Ime2Constants() {}

    /**
     * Package name of test IME 2.
     */
    public static final String PACKAGE = "android.inputmethodservice.cts.ime2";
    /**
     * Class name of test IME 2.
     */
    public static final String CLASS =   "android.inputmethodservice.cts.ime2.CtsInputMethod2";
    /**
     * APK name that contains test IME 2.
     */
    public static final String APK = "CtsInputMethod2.apk";

    /**
     * IME ID of test IME 2.
     */
    public static final String IME_ID = ComponentNameUtils.buildComponentName(PACKAGE, CLASS);
}
