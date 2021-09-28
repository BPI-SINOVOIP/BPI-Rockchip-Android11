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

package android.permission3.cts

import android.support.test.uiautomator.By
import androidx.test.filters.FlakyTest
import com.android.compatibility.common.util.SystemUtil.eventually
import org.junit.Assume.assumeFalse
import org.junit.Before
import org.junit.Test

/**
 * Runtime permission behavior tests for apps targeting API 29.
 */
class PermissionTest29 : BaseUsePermissionTest() {
    @Before
    fun assumeNotTv() {
        assumeFalse(isTv)
    }

    @Before
    fun installApp29() {
        installPackage(APP_APK_PATH_29)
    }

    @Before
    fun assertAppHasNoPermissions() {
        assertAppHasPermission(android.Manifest.permission.ACCESS_FINE_LOCATION, false)
        assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)
    }

    @Test
    fun testRequestOnlyBackgroundNotPossible() {
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {}

        assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)
    }

    @Test
    fun testRequestBoth() {
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to true
        ) {
            clickPermissionRequestSettingsLinkAndAllowAlways()
        }
    }

    @Test
    fun testRequestBothInSequence() {
        // Step 1: request foreground only
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true
        ) {
            clickPermissionRequestAllowForegroundButton()
        }

        assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)

        // Step 2: request background only
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to true
        ) {
            clickPermissionRequestSettingsLinkAndAllowAlways()
        }

        assertAppHasPermission(android.Manifest.permission.ACCESS_FINE_LOCATION, true)
    }

    @Test
    fun testRequestBothButGrantInSequence() {
        // Step 1: grant foreground only
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            clickPermissionRequestAllowForegroundButton()
        }

        // Step 2: grant background
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to true
        ) {
            clickPermissionRequestSettingsLinkAndAllowAlways()
        }
    }

    @Test
    fun testDenyBackgroundWithPrejudice() {
        // Step 1: deny the first time
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to false,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            clickPermissionRequestDenyButton()
        }

        // Step 2: deny with prejudice
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to false,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            clickPermissionRequestDenyAndDontAskAgainButton()
        }

        // Step 3: All further requests should be denied automatically
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to false,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {}
    }

    @FlakyTest
    @Test
    fun testGrantDialogToSettingsNoOp() {
        // Step 1: Request both, go to settings, do nothing
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            openSettingsThenDoNothingThenLeave()

            assertAppHasPermission(android.Manifest.permission.ACCESS_FINE_LOCATION, false)
            assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)

            clickPermissionRequestAllowForegroundButton()
        }

        // Step 2: Upgrade foreground to background, go to settings, do nothing
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            openSettingsThenDoNothingThenLeave()

            assertAppHasPermission(android.Manifest.permission.ACCESS_FINE_LOCATION, true)
            assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)

            clickPermissionRequestNoUpgradeAndDontAskAgainButton()
        }
    }

    private fun openSettingsThenDoNothingThenLeave() {
        clickPermissionRequestSettingsLink()
        eventually {
            pressBack()
            if (isAutomotive) {
                waitFindObject(By.textContains("Allow in settings."), 100)
            } else {
                waitFindObject(By.res("com.android.permissioncontroller:id/grant_dialog"), 100)
            }
        }
    }

    @FlakyTest
    @Test
    fun testGrantDialogToSettingsDowngrade() {
        // Request upgrade, downgrade permission to denied in settings
        requestAppPermissionsAndAssertResult(
            android.Manifest.permission.ACCESS_FINE_LOCATION to true,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION to false
        ) {
            clickPermissionRequestAllowForegroundButton()
        }

        requestAppPermissions(
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION
        ) {
            clickPermissionRequestSettingsLinkAndDeny()
            waitForIdle()
            pressBack()
        }

        assertAppHasPermission(android.Manifest.permission.ACCESS_FINE_LOCATION, false)
        assertAppHasPermission(android.Manifest.permission.ACCESS_BACKGROUND_LOCATION, false)
    }
}
