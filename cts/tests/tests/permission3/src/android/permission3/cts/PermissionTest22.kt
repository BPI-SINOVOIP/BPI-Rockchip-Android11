/*
 * Copyright (C) 2015 The Android Open Source Project
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

import org.junit.Before
import org.junit.Test

/**
 * Runtime permission behavior tests for apps targeting API 22.
 */
class PermissionTest22 : BaseUsePermissionTest() {
    @Before
    fun installApp22AndApprovePermissionReview() {
        installPackage(APP_APK_PATH_22)
        approvePermissionReview()
    }

    @Test
    fun testCompatDefault() {
        // Legacy permission model appears granted
        assertAppHasPermission(android.Manifest.permission.READ_CALENDAR, true)
        assertAppHasPermission(android.Manifest.permission.WRITE_CALENDAR, true)
        assertAppHasCalendarAccess(true)
    }

    @Test
    fun testCompatRevoked() {
        // Revoke the permission
        revokeAppPermissions(android.Manifest.permission.WRITE_CALENDAR, isLegacyApp = true)

        // Legacy permission model appears granted
        assertAppHasPermission(android.Manifest.permission.READ_CALENDAR, true)
        assertAppHasPermission(android.Manifest.permission.WRITE_CALENDAR, true)
        // Read/write access should be ignored
        assertAppHasCalendarAccess(false)
    }

    @Test
    fun testNoRuntimePrompt() {
        // Request the permission and do nothing
        // Expect the permission is not granted
        requestAppPermissionsAndAssertResult(
            arrayOf(android.Manifest.permission.SEND_SMS), emptyArray()
        ) {}
    }
}
