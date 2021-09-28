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

package android.permission.cts

import android.Manifest.permission.CAMERA
import android.Manifest.permission.READ_CALENDAR
import android.content.pm.PackageManager
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.os.Process
import android.platform.test.annotations.AppModeFull
import androidx.test.platform.app.InstrumentationRegistry
import com.android.compatibility.common.util.SystemUtil.runShellCommand
import com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class RevokePermissionTest {

    private val APP_PKG_NAME = "android.permission.cts.appthatrequestcustompermission"
    private val APK = "/data/local/tmp/cts/permissions/" +
            "CtsAppThatRequestsCalendarContactsBodySensorCustomPermission.apk"

    @Before
    fun installApp() {
        runShellCommand("pm install -r -g $APK")
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokePermission() {
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = READ_CALENDAR,
                isGranted = true)
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokePermissionNotRequested() {
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = CAMERA,
                throwableType = SecurityException::class.java,
                throwableMessage = "has not requested permission")
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokeFakePermission() {
        val fakePermissionName = "FAKE_PERMISSION"
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = fakePermissionName,
                throwableType = java.lang.IllegalArgumentException::class.java,
                throwableMessage = "Unknown permission: $fakePermissionName")
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokeFakePackage() {
        val fakePackageName = "fake.package.name.which.should.not.exist"
        assertPackageNotInstalled(fakePackageName)
        testRevoke(
                packageName = fakePackageName,
                permission = READ_CALENDAR)
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokePermissionWithReason() {
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = READ_CALENDAR,
                reason = "test reason",
                isGranted = true)
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokePermissionNotRequestedWithReason() {
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = CAMERA,
                reason = "test reason",
                throwableType = SecurityException::class.java,
                throwableMessage = "has not requested permission")
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokeFakePermissionWithReason() {
        val fakePermissionName = "FAKE_PERMISSION"
        testRevoke(
                packageName = APP_PKG_NAME,
                permission = fakePermissionName,
                reason = "test reason",
                throwableType = java.lang.IllegalArgumentException::class.java,
                throwableMessage = "Unknown permission: $fakePermissionName")
    }

    @Test
    @AppModeFull(reason = "Instant apps can't revoke permissions.")
    fun testRevokeFakePackageWithReason() {
        val fakePackageName = "fake.package.name.which.should.not.exist"
        assertPackageNotInstalled(fakePackageName)
        testRevoke(
                packageName = fakePackageName,
                permission = READ_CALENDAR,
                reason = "test reason")
    }

    @After
    fun uninstallApp() {
        runShellCommand("pm uninstall $APP_PKG_NAME")
    }

    private fun testRevoke(
        packageName: String,
        permission: String,
        reason: String? = null,
        isGranted: Boolean = false,
        throwableType: Class<*>? = null,
        throwableMessage: String = ""
    ) {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val pm = context.packageManager

        if (isGranted) {
            assertEquals(PERMISSION_GRANTED, pm.checkPermission(READ_CALENDAR, APP_PKG_NAME))
        }

        runWithShellPermissionIdentity {
            if (throwableType == null) {
                if (reason == null) {
                    pm.revokeRuntimePermission(packageName, permission, Process.myUserHandle())
                } else {
                    pm.revokeRuntimePermission(packageName, permission, Process.myUserHandle(),
                            reason)
                }
            } else {
                try {
                    if (reason == null) {
                        pm.revokeRuntimePermission(packageName, permission, Process.myUserHandle())
                    } else {
                        pm.revokeRuntimePermission(packageName, permission, Process.myUserHandle(),
                                reason)
                    }
                } catch (t: Throwable) {
                    if (t::class.java.name == throwableType.name &&
                            t.message!!.contains(throwableMessage)) {
                        return@runWithShellPermissionIdentity
                    }
                    throw RuntimeException("Unexpected throwable", t)
                }
                throw RuntimeException("revokeRuntimePermission expected to throw.")
            }
        }
    }

    private fun assertPackageNotInstalled(packageName: String) {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val pm = context.packageManager
        try {
            pm.getPackageInfo(packageName, 0)
            throw RuntimeException("$packageName exists on this device")
        } catch (e: PackageManager.NameNotFoundException) {
            // Expected
        }
    }
}