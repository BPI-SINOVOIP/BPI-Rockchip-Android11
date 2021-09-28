/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.app.Activity
import android.content.ComponentName
import android.content.Intent
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import java.util.concurrent.TimeUnit

/**
 * Tests for the platform permission policy around apps targeting API 25.
 */
class PermissionPolicyTest25 : BasePermissionTest() {
    companion object {
        const val APP_APK_PATH_25 = "$APK_DIRECTORY/CtsPermissionPolicyApp25.apk"
        const val APP_PACKAGE_NAME = "android.permission3.cts.permissionpolicy"
    }

    @Before
    fun installApp25() {
        uninstallPackage(APP_PACKAGE_NAME, requireSuccess = false)
        installPackage(APP_APK_PATH_25)
    }

    @After
    fun uninstallApp() {
        uninstallPackage(APP_PACKAGE_NAME, requireSuccess = false)
    }

    @Test
    fun testNoProtectionFlagsAddedToNonSignatureProtectionPermissions() {
        val future = startActivityForFuture(
            Intent().apply {
                component = ComponentName(
                    APP_PACKAGE_NAME, "$APP_PACKAGE_NAME.TestProtectionFlagsActivity"
                )
            }
        )
        val result = future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)
        assertEquals(Activity.RESULT_OK, result.resultCode)
        assertEquals("", result.resultData!!.getStringExtra("$APP_PACKAGE_NAME.ERROR_MESSAGE"))
    }
}
