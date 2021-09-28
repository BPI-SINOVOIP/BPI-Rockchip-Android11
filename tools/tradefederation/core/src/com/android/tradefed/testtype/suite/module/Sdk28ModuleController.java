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
package com.android.tradefed.testtype.suite.module;

/**
 * A module controller to check if a device is on SDK 28 (Android 9) or above. This is used to
 * workaround b/78780430, where on Android 8.1 armeabi-v7a devices, instrumentation will crash
 * because the target package was pre-opted for only arm64.
 *
 * <p>Use by adding this line to your AndroidTest.xml: <object type="module_controller"
 * class="com.android.tradefed.testtype.suite.module.Sdk28ModuleController" />
 */
public class Sdk28ModuleController extends MinSdkModuleController {
    public Sdk28ModuleController() {
        super(28);
    }
}
