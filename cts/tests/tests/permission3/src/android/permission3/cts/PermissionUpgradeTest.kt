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

import org.junit.Test

/**
 * Runtime permission behavior tests for upgrading apps.
 */
class PermissionUpgradeTest : BaseUsePermissionTest() {

    @Test
    fun testUpgradeKeepsPermissions() {
        installPackage(APP_APK_PATH_22)

        approvePermissionReview()

        assertAllPermissionsGrantedByDefault()

        installPackage(APP_APK_PATH_23, reinstall = true)

        assertAllPermissionsGrantedOnUpgrade()
    }

    private fun assertAllPermissionsGrantedByDefault() {
        arrayOf(
            android.Manifest.permission.SEND_SMS,
            android.Manifest.permission.RECEIVE_SMS,
            // The APK does not request READ_CONTACTS because of other tests
            android.Manifest.permission.WRITE_CONTACTS,
            android.Manifest.permission.READ_CALENDAR,
            android.Manifest.permission.WRITE_CALENDAR,
            android.Manifest.permission.READ_SMS,
            android.Manifest.permission.RECEIVE_WAP_PUSH,
            android.Manifest.permission.RECEIVE_MMS,
            "android.permission.READ_CELL_BROADCASTS",
            android.Manifest.permission.READ_EXTERNAL_STORAGE,
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION,
            android.Manifest.permission.READ_PHONE_STATE,
            android.Manifest.permission.CALL_PHONE,
            android.Manifest.permission.READ_CALL_LOG,
            android.Manifest.permission.WRITE_CALL_LOG,
            android.Manifest.permission.ADD_VOICEMAIL,
            android.Manifest.permission.USE_SIP,
            android.Manifest.permission.PROCESS_OUTGOING_CALLS,
            android.Manifest.permission.CAMERA,
            android.Manifest.permission.BODY_SENSORS,
            // Split permissions
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION
        ).forEach {
            assertAppHasPermission(it, true)
        }
    }

    private fun assertAllPermissionsGrantedOnUpgrade() {
        assertAppHasAllOrNoPermissions(true)
    }

    @Test
    fun testNoDowngradePermissionModel() {
        installPackage(APP_APK_PATH_23)
        installPackage(APP_APK_PATH_22, reinstall = true, expectSuccess = false)
    }

    @Test
    fun testRevokePropagatedOnUpgradeOldToNewModel() {
        installPackage(APP_APK_PATH_22)

        approvePermissionReview()

        // Revoke a permission
        revokeAppPermissions(android.Manifest.permission.WRITE_CALENDAR, isLegacyApp = true)

        installPackage(APP_APK_PATH_23, reinstall = true)

        assertAppHasPermission(android.Manifest.permission.WRITE_CALENDAR, false)
    }

    @Test
    fun testRevokePropagatedOnUpgradeNewToNewModel() {
        installPackage(APP_APK_PATH_23)

        // Make sure we don't have the permission
        assertAppHasPermission(android.Manifest.permission.READ_CALENDAR, false)
        assertAppHasPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE, false)

        // Request the permission and allow it
        // Make sure the permission is granted
        requestAppPermissionsAndAssertResult(android.Manifest.permission.READ_CALENDAR to true) {
            clickPermissionRequestAllowButton()
        }

        installPackage(APP_APK_PATH_23, reinstall = true)

        // Make sure the permission is still granted after the upgrade
        assertAppHasPermission(android.Manifest.permission.READ_CALENDAR, true)
        // Also make sure one of the not granted permissions is still not granted
        assertAppHasPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE, false)
    }

    private fun assertAppHasAllOrNoPermissions(expectPermissions: Boolean) {
        arrayOf(
            android.Manifest.permission.SEND_SMS,
            android.Manifest.permission.RECEIVE_SMS,
            android.Manifest.permission.RECEIVE_WAP_PUSH,
            android.Manifest.permission.RECEIVE_MMS,
            android.Manifest.permission.READ_CALENDAR,
            android.Manifest.permission.WRITE_CALENDAR,
            android.Manifest.permission.WRITE_CONTACTS,
            android.Manifest.permission.READ_SMS,
            android.Manifest.permission.READ_PHONE_STATE,
            android.Manifest.permission.READ_CALL_LOG,
            android.Manifest.permission.WRITE_CALL_LOG,
            android.Manifest.permission.ADD_VOICEMAIL,
            android.Manifest.permission.CALL_PHONE,
            android.Manifest.permission.USE_SIP,
            android.Manifest.permission.PROCESS_OUTGOING_CALLS,
            android.Manifest.permission.RECORD_AUDIO,
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION,
            android.Manifest.permission.CAMERA,
            android.Manifest.permission.BODY_SENSORS,
            android.Manifest.permission.READ_CELL_BROADCASTS,
            // Split permissions
            android.Manifest.permission.ACCESS_BACKGROUND_LOCATION,
            // Storage permissions
            android.Manifest.permission.READ_EXTERNAL_STORAGE,
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE
        ).forEach {
            assertAppHasPermission(it, expectPermissions)
        }
    }
}
