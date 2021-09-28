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

package android.permission2.cts

import android.Manifest.permission.ACCEPT_HANDOVER
import android.Manifest.permission.ACCESS_COARSE_LOCATION
import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.ACTIVITY_RECOGNITION
import android.Manifest.permission.ADD_VOICEMAIL
import android.Manifest.permission.ANSWER_PHONE_CALLS
import android.Manifest.permission.BODY_SENSORS
import android.Manifest.permission.CALL_PHONE
import android.Manifest.permission.CAMERA
import android.Manifest.permission.GET_ACCOUNTS
import android.Manifest.permission.PACKAGE_USAGE_STATS
import android.Manifest.permission.PROCESS_OUTGOING_CALLS
import android.Manifest.permission.READ_CALENDAR
import android.Manifest.permission.READ_CALL_LOG
import android.Manifest.permission.READ_CELL_BROADCASTS
import android.Manifest.permission.READ_CONTACTS
import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.Manifest.permission.READ_PHONE_NUMBERS
import android.Manifest.permission.READ_PHONE_STATE
import android.Manifest.permission.READ_SMS
import android.Manifest.permission.RECEIVE_MMS
import android.Manifest.permission.RECEIVE_SMS
import android.Manifest.permission.RECEIVE_WAP_PUSH
import android.Manifest.permission.RECORD_AUDIO
import android.Manifest.permission.SEND_SMS
import android.Manifest.permission.USE_SIP
import android.Manifest.permission.WRITE_CALENDAR
import android.Manifest.permission.WRITE_CALL_LOG
import android.Manifest.permission.WRITE_CONTACTS
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.Manifest.permission_group.UNDEFINED
import android.app.AppOpsManager.permissionToOp
import android.content.pm.PackageManager.GET_PERMISSIONS
import android.content.pm.PermissionInfo.PROTECTION_DANGEROUS
import android.content.pm.PermissionInfo.PROTECTION_FLAG_APPOP
import android.os.Build
import android.permission.PermissionManager
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.google.common.truth.Truth.assertThat
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RuntimePermissionProperties {
    private val context = InstrumentationRegistry.getInstrumentation().getTargetContext()
    private val pm = context.packageManager

    private val platformPkg = pm.getPackageInfo("android", GET_PERMISSIONS)
    private val platformRuntimePerms = platformPkg.permissions
            .filter { it.protection == PROTECTION_DANGEROUS }
    private val platformBgPermNames = platformRuntimePerms.mapNotNull { it.backgroundPermission }

    @Test
    fun allRuntimeForegroundPermissionNeedAnAppOp() {
        val platformFgPerms =
            platformRuntimePerms.filter { !platformBgPermNames.contains(it.name) }

        for (perm in platformFgPerms) {
            assertThat(permissionToOp(perm.name)).named("AppOp for ${perm.name}").isNotNull()
        }
    }

    @Test
    fun groupOfRuntimePermissionsShouldBeUnknown() {
        for (perm in platformRuntimePerms) {
            assertThat(perm.group).named("Group of ${perm.name}").isEqualTo(UNDEFINED)
        }
    }

    @Test
    fun allAppOpPermissionNeedAnAppOp() {
        val platformAppOpPerms = platformPkg.permissions
                .filter { (it.protectionFlags and PROTECTION_FLAG_APPOP) != 0 }
                .filter {
                    // Grandfather incomplete definition of PACKAGE_USAGE_STATS
                    it.name != PACKAGE_USAGE_STATS
                }

        for (perm in platformAppOpPerms) {
            assertThat(permissionToOp(perm.name)).named("AppOp for ${perm.name}").isNotNull()
        }
    }

    /**
     * The permission of a background permission is the one of its foreground permission
     */
    @Test
    fun allRuntimeBackgroundPermissionCantHaveAnAppOp() {
        val platformBgPerms =
            platformRuntimePerms.filter { platformBgPermNames.contains(it.name) }

        for (perm in platformBgPerms) {
            assertThat(permissionToOp(perm.name)).named("AppOp for ${perm.name}").isNull()
        }
    }

    /**
     * Commonly a new runtime permission is created by splitting an old one into twice
     */
    @Test
    fun runtimePermissionsShouldHaveBeenSplitFromPreviousPermission() {
        // Runtime permissions in Android P
        val expectedPerms = mutableSetOf(READ_CONTACTS, WRITE_CONTACTS, GET_ACCOUNTS, READ_CALENDAR,
            WRITE_CALENDAR, SEND_SMS, RECEIVE_SMS, READ_SMS, RECEIVE_MMS, RECEIVE_WAP_PUSH,
            READ_CELL_BROADCASTS, READ_EXTERNAL_STORAGE, WRITE_EXTERNAL_STORAGE,
            ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION, READ_CALL_LOG, WRITE_CALL_LOG,
            PROCESS_OUTGOING_CALLS, READ_PHONE_STATE, READ_PHONE_NUMBERS, CALL_PHONE,
            ADD_VOICEMAIL, USE_SIP, ANSWER_PHONE_CALLS, ACCEPT_HANDOVER, RECORD_AUDIO, CAMERA,
            BODY_SENSORS)

        // Add permission split since P
        for (sdkVersion in Build.VERSION_CODES.P + 1..Build.VERSION_CODES.CUR_DEVELOPMENT + 1) {
            for (splitPerm in
                context.getSystemService(PermissionManager::class.java)!!.splitPermissions) {
                if (splitPerm.targetSdk == sdkVersion &&
                    expectedPerms.contains(splitPerm.splitPermission)) {
                    expectedPerms.addAll(splitPerm.newPermissions)
                }
            }
        }

        // Add runtime permission added in Q which were _not_ split from a previously existing
        // runtime permission
        expectedPerms.add(ACTIVITY_RECOGNITION)

        assertThat(expectedPerms).containsExactlyElementsIn(platformRuntimePerms.map { it.name })
    }
}
