/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.incrementalinstall.common;

public class Consts {

    // Tag for the package to launch and verify, sent from Host to app validator.
    public static final String PACKAGE_TO_LAUNCH_TAG = "PACKAGE_TO_LAUNCH";

    // Tag for the components that should be loaded, sent from Host to app validator.
    public static final String LOADED_COMPONENTS_TAG = "LOADED_COMPONENTS";

    // Tag for the components that should not be loaded, sent from Host to app validator.
    public static final String NOT_LOADED_COMPONENTS_TAG = "NOT_LOADED_COMPONENTS";

    // Tag for installation type (incremental or not), sent from Host to app validator.
    public static final String IS_INCFS_INSTALLATION_TAG = "IS_INCFS";

    // Tag for the version of the installed app, sent from Host to app validator.
    public static final String INSTALLED_VERSION_CODE_TAG = "VERSION_CODE";

    // Action broadcast from test app after attempting to load a component.
    public static final String INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION =
            "android.incrementalinstall.incrementaltestapp.INCREMENTAL_TEST_APP_RECEIVER_ACTION";

    // Name of component attempted to load in the test app.
    public static final String TARGET_COMPONENT_KEY = "IncrementalTestAppTargetComponent";

    // Status of the attempt to load the component.
    public static final String COMPONENT_STATUS_KEY = "ComponentStatus";

    public static class SupportedComponents {

        public static final String ON_CREATE_COMPONENT = "onCreate";
        public static final String ON_CREATE_COMPONENT_2 = "onCreate2";
        public static final String DYNAMIC_ASSET_COMPONENT = "dynamicAsset";
        public static final String DYNAMIC_CODE_COMPONENT = "dynamicCode";
        public static final String COMPRESSED_NATIVE_COMPONENT = "compressedNative";
        public static final String UNCOMPRESSED_NATIVE_COMPONENT = "unCompressedNative";

        public static String[] getAllComponents() {
            return new String[]{ON_CREATE_COMPONENT, ON_CREATE_COMPONENT_2, DYNAMIC_ASSET_COMPONENT,
                    DYNAMIC_CODE_COMPONENT, COMPRESSED_NATIVE_COMPONENT,
                    UNCOMPRESSED_NATIVE_COMPONENT};
        }
    }

}
