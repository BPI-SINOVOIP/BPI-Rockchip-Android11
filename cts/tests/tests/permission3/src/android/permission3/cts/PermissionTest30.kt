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

package android.permission3.cts

import android.Manifest.permission.ACCESS_BACKGROUND_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import org.junit.Test

/**
 * Runtime permission behavior apps targeting API 30
 */
class PermissionTest30 : BaseUsePermissionTest() {

    @Test
    fun testCantRequestFgAndBgAtOnce() {
        installPackage(APP_APK_PATH_LATEST_WITH_BACKGROUND)
        assertAppHasPermission(ACCESS_FINE_LOCATION, false)
        assertAppHasPermission(ACCESS_BACKGROUND_LOCATION, false)

        requestAppPermissionsAndAssertResult(ACCESS_FINE_LOCATION to false,
                ACCESS_BACKGROUND_LOCATION to false) {
            // Do nothing, should be automatically denied
        }
    }

    @Test
    fun testRequestBothInSequence() {
        installPackage(APP_APK_PATH_LATEST_WITH_BACKGROUND)
        assertAppHasPermission(ACCESS_FINE_LOCATION, false)
        assertAppHasPermission(ACCESS_BACKGROUND_LOCATION, false)

        requestAppPermissionsAndAssertResult(ACCESS_FINE_LOCATION to true) {
            clickPermissionRequestAllowForegroundButton()
        }

        requestAppPermissionsAndAssertResult(ACCESS_BACKGROUND_LOCATION to true) {
            clickAllowAlwaysInSettings()
            waitForIdle()
            pressBack()
        }
    }
}
