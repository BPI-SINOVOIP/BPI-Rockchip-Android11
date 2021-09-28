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

import android.Manifest.permission.ACCESS_MEDIA_LOCATION
import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.Instrumentation
import android.app.UiAutomation
import android.content.Context
import android.content.pm.PackageManager
import android.platform.test.annotations.SecurityTest
import androidx.test.platform.app.InstrumentationRegistry
import com.android.compatibility.common.util.SystemUtil
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Test

class StorageEscalationTest {
    companion object {
        private const val APK_DIRECTORY = "/data/local/tmp/cts/permissions"
        const val APP_APK_PATH_28 = "$APK_DIRECTORY/CtsStorageEscalationApp28.apk"
        const val APP_APK_PATH_29_SCOPED = "$APK_DIRECTORY/CtsStorageEscalationApp29Scoped.apk"
        const val APP_APK_PATH_29_FULL = "$APK_DIRECTORY/CtsStorageEscalationApp29Full.apk"
        const val APP_PACKAGE_NAME = "android.permission3.cts.storageescalation"
        const val DELAY_TIME_MS: Long = 200
        val permissions = listOf<String>(READ_EXTERNAL_STORAGE, WRITE_EXTERNAL_STORAGE,
            ACCESS_MEDIA_LOCATION)
    }

    private val instrumentation: Instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context: Context = instrumentation.context
    private val uiAutomation: UiAutomation = instrumentation.uiAutomation

    @Before
    @After
    fun uninstallApp() {
        SystemUtil.runShellCommand("pm uninstall $APP_PACKAGE_NAME")
    }

    private fun installPackage(apk: String) {
        SystemUtil.runShellCommand("pm install -r $apk")
    }

    private fun grantStoragePermissions() {
        for (permName in permissions) {
            uiAutomation.grantRuntimePermission(APP_PACKAGE_NAME, permName)
        }
    }

    private fun assertStoragePermissionState(granted: Boolean) {
        for (permName in permissions) {
            Assert.assertEquals(granted, context.packageManager.checkPermission(permName,
                APP_PACKAGE_NAME) == PackageManager.PERMISSION_GRANTED)
        }
    }

    @Test
    @SecurityTest(minPatchLevel = "2021-03")
    fun testCannotEscalateWithSdkDowngrade() {
        runStorageEscalationTest(APP_APK_PATH_29_SCOPED, APP_APK_PATH_28)
    }

    @Test
    @SecurityTest(minPatchLevel = "2021-03")
    fun testCannotEscalateWithNewManifestLegacyRequest() {
        runStorageEscalationTest(APP_APK_PATH_29_SCOPED, APP_APK_PATH_29_FULL)
    }

    private fun runStorageEscalationTest(startPackageApk: String, finishPackageApk: String) {
        installPackage(startPackageApk)
        grantStoragePermissions()
        assertStoragePermissionState(granted = true)
        installPackage(finishPackageApk)
        // permission revoke is async, so wait a short period
        Thread.sleep(DELAY_TIME_MS)
        assertStoragePermissionState(granted = false)
    }
}
