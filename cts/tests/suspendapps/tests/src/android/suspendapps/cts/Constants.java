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

package android.suspendapps.cts;

import com.android.suspendapps.testdeviceadmin.TestCommsReceiver;
import com.android.suspendapps.testdeviceadmin.TestDeviceAdmin;

public interface Constants {
    String PACKAGE_NAME = "android.suspendapps.cts";
    String TEST_APP_PACKAGE_NAME = com.android.suspendapps.suspendtestapp.Constants.PACKAGE_NAME;
    String[] TEST_PACKAGE_ARRAY = new String[] {TEST_APP_PACKAGE_NAME};
    String[] ALL_TEST_PACKAGES = new String[] {
            TEST_APP_PACKAGE_NAME,
            com.android.suspendapps.suspendtestapp.Constants.ANDROID_PACKAGE_NAME_2
    };
    String DEVICE_ADMIN_PACKAGE = TestCommsReceiver.PACKAGE_NAME;
    String DEVICE_ADMIN_COMPONENT =
            DEVICE_ADMIN_PACKAGE + "/" + TestDeviceAdmin.class.getName();
}
