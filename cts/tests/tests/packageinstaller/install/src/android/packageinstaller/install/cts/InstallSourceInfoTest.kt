/*
 * Copyright 2020 The Android Open Source Project
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

import android.app.Activity
import android.content.Intent
import android.content.Intent.ACTION_INSTALL_PACKAGE
import android.content.pm.PackageInstaller
import android.content.pm.PackageManager.MATCH_DEFAULT_ONLY
import android.net.Uri
import android.platform.test.annotations.AppModeFull
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.google.common.truth.Truth.assertThat
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.TimeUnit

private const val INSTALL_BUTTON_ID = "button1"

@RunWith(AndroidJUnit4::class)
@AppModeFull(reason = "Instant apps cannot install packages")
class InstallSourceInfoTest : PackageInstallerTestBase() {
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private val pm = context.packageManager
    private val ourPackageName = context.packageName

    @Test
    fun installViaIntent() {
        val packageInstallerPackageName = getPackageInstallerPackageName()

        val installation = startInstallationViaIntent()
        clickInstallerUIButton(INSTALL_BUTTON_ID)

        // Install should have succeeded
        assertThat(installation.get(TIMEOUT, TimeUnit.MILLISECONDS)).isEqualTo(Activity.RESULT_OK)

        val info = pm.getInstallSourceInfo(TEST_APK_PACKAGE_NAME)
        assertThat(info.getInstallingPackageName()).isEqualTo(packageInstallerPackageName)
        assertThat(info.getInitiatingPackageName()).isEqualTo(packageInstallerPackageName)
        assertThat(info.getOriginatingPackageName()).isNull()
    }

    @Test
    fun InstallViaSession() {
        startInstallationViaSession()
        clickInstallerUIButton(INSTALL_BUTTON_ID)

        // Install should have succeeded
        assertThat(getInstallSessionResult()).isEqualTo(PackageInstaller.STATUS_SUCCESS)

        val info = pm.getInstallSourceInfo(TEST_APK_PACKAGE_NAME)
        assertThat(info.getInstallingPackageName()).isEqualTo(ourPackageName)
        assertThat(info.getInitiatingPackageName()).isEqualTo(ourPackageName)
        assertThat(info.getOriginatingPackageName()).isNull()
    }

    private fun getPackageInstallerPackageName(): String {
        val installerIntent = Intent(ACTION_INSTALL_PACKAGE)
        installerIntent.setDataAndType(Uri.parse("content://com.example/"),
                "application/vnd.android.package-archive")
        return installerIntent.resolveActivityInfo(pm, MATCH_DEFAULT_ONLY).packageName
    }
}
