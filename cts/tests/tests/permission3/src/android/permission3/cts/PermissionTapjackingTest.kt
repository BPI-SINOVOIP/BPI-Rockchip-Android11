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

import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.content.pm.PackageManager
import android.support.test.uiautomator.By
import com.android.compatibility.common.util.SystemUtil
import org.junit.Assume.assumeFalse
import org.junit.Before
import org.junit.Test

/**
 * Tests permissions can't be tapjacked
 */
class PermissionTapjackingTest : BaseUsePermissionTest() {

    @Before
    fun installAppLatest() {
        installPackage(APP_APK_PATH_WITH_OVERLAY)
    }

    @Test
    fun testTapjackGrantDialog() {
        // PermissionController for television uses a floating window.
        assumeFalse(isTv)

        assertAppHasPermission(ACCESS_FINE_LOCATION, false)
        requestAppPermissionsForNoResult(ACCESS_FINE_LOCATION) {}

        // Wait for overlay to hide the dialog
        waitFindObject(By.res("android.permission3.cts.usepermission:id/overlay_description"))
        try {
            // Try to grant the permission, this should fail
            SystemUtil.eventually({
                if (packageManager.checkPermission(ACCESS_FINE_LOCATION, APP_PACKAGE_NAME) ==
                        PackageManager.PERMISSION_DENIED) {
                    clickPermissionRequestAllowForegroundButton(100)
                }
                assertAppHasPermission(ACCESS_FINE_LOCATION, true)
            }, 10000)
        } catch (e: RuntimeException) {
            // expected
        }
        // Permission should not be granted and dialog should still be showing
        assertAppHasPermission(ACCESS_FINE_LOCATION, false)

        // On Automotive the dialog gets closed by the tapjacking activity popping up
        if (!isAutomotive) {
            clickPermissionRequestAllowForegroundButton()
        }
    }
}