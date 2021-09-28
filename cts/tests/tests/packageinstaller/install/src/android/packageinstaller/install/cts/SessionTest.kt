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
package android.packageinstaller.install.cts

import android.app.Activity.RESULT_CANCELED
import android.content.pm.ApplicationInfo.CATEGORY_MAPS
import android.content.pm.ApplicationInfo.CATEGORY_UNDEFINED
import android.content.pm.PackageInstaller.STATUS_FAILURE_ABORTED
import android.content.pm.PackageInstaller.STATUS_SUCCESS
import android.platform.test.annotations.AppModeFull
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.compatibility.common.util.AppOpsUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.TimeUnit

private const val INSTALL_BUTTON_ID = "button1"
private const val CANCEL_BUTTON_ID = "button2"

@AppModeFull(reason = "Instant apps cannot create installer sessions")
@RunWith(AndroidJUnit4::class)
class SessionTest : PackageInstallerTestBase() {
    private val context = InstrumentationRegistry.getTargetContext()
    private val pm = context.packageManager

    /**
     * Check that we can install an app via a package-installer session
     */
    @Test
    fun confirmInstallation() {
        val installation = startInstallationViaSession()
        clickInstallerUIButton(INSTALL_BUTTON_ID)

        // Install should have succeeded
        assertEquals(STATUS_SUCCESS, getInstallSessionResult())
        assertInstalled()

        // Even when the install succeeds the install confirm dialog returns 'canceled'
        assertEquals(RESULT_CANCELED, installation.get(TIMEOUT, TimeUnit.MILLISECONDS))

        assertTrue(AppOpsUtils.allowedOperationLogged(context.packageName, APP_OP_STR))
    }

    /**
     * Check that we can set an app category for an app we installed
     */
    @Test
    fun setAppCategory() {
        val installation = startInstallationViaSession()
        clickInstallerUIButton(INSTALL_BUTTON_ID)

        // Wait for installation to finish
        getInstallSessionResult()

        assertEquals(CATEGORY_UNDEFINED, pm.getApplicationInfo(TEST_APK_PACKAGE_NAME, 0).category)

        // This app installed the app, hence we can set the category
        pm.setApplicationCategoryHint(TEST_APK_PACKAGE_NAME, CATEGORY_MAPS)

        assertEquals(CATEGORY_MAPS, pm.getApplicationInfo(TEST_APK_PACKAGE_NAME, 0).category)
    }

    /**
     * Install an app via a package-installer session, but then cancel it when the package installer
     * pops open.
     */
    @Test
    fun cancelInstallation() {
        val installation = startInstallationViaSession()
        clickInstallerUIButton(CANCEL_BUTTON_ID)

        // Install should have been aborted
        assertEquals(STATUS_FAILURE_ABORTED, getInstallSessionResult())
        assertEquals(RESULT_CANCELED, installation.get(TIMEOUT, TimeUnit.MILLISECONDS))
        assertNotInstalled()
    }

    /**
     * Check that can't install when FRP mode is enabled.
     */
    @Test
    fun confirmFrpInstallationFails() {
        try {
            setSecureFrp(true)

            try {
                val installation = startInstallationViaSession()
                clickInstallerUIButton(CANCEL_BUTTON_ID)

                fail("Package should not be installed")
            } catch (expected: SecurityException) {
            }

            // Install should never have started
            assertNotInstalled()
        } finally {
            setSecureFrp(false)
        }
    }
}
