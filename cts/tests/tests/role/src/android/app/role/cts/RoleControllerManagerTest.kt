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

package android.app.role.cts

import android.app.Instrumentation

import android.app.role.RoleControllerManager
import android.app.role.RoleManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Process
import android.provider.Settings
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.compatibility.common.util.SystemUtil.runShellCommand
import com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity
import com.android.compatibility.common.util.ThrowingSupplier
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit
import java.util.function.Consumer

/**
 * Tests [RoleControllerManager].
 */
@RunWith(AndroidJUnit4::class)
class RoleControllerManagerTest {
    private val instrumentation: Instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context: Context = instrumentation.context
    private val packageManager: PackageManager = context.packageManager
    private val roleControllerManager: RoleControllerManager =
        context.getSystemService(RoleControllerManager::class.java)!!

    @Before
    fun installApp() {
        installPackage(APP_APK_PATH)
    }

    @After
    fun uninstallApp() {
        uninstallPackage(APP_PACKAGE_NAME)
    }

    @Test
    fun appIsVisibleForRole() {
        assumeRoleIsVisible()
        assertAppIsVisibleForRole(APP_PACKAGE_NAME, ROLE_NAME, true)
    }

    @Test
    fun settingsIsNotVisibleForHomeRole() {
        // Settings should never show as a possible home app even if qualified.
        val settingsPackageName = packageManager.resolveActivity(
            Intent(Settings.ACTION_SETTINGS), PackageManager.MATCH_DEFAULT_ONLY
            or PackageManager.MATCH_DIRECT_BOOT_AWARE or PackageManager.MATCH_DIRECT_BOOT_UNAWARE
        )!!.activityInfo.packageName
        assertAppIsVisibleForRole(settingsPackageName, RoleManager.ROLE_HOME, false)
    }

    @Test
    fun appIsNotVisibleForInvalidRole() {
        assertAppIsVisibleForRole(APP_PACKAGE_NAME, "invalid", false)
    }

    @Test
    fun invalidAppIsNotVisibleForRole() {
        assertAppIsVisibleForRole("invalid", ROLE_NAME, false)
    }

    private fun assertAppIsVisibleForRole(
        packageName: String,
        roleName: String,
        expectedIsVisible: Boolean
    ) {
        runWithShellPermissionIdentity {
            val future = CompletableFuture<Boolean>()
            roleControllerManager.isApplicationVisibleForRole(
                roleName, packageName, context.mainExecutor, Consumer { future.complete(it) }
            )
            val isVisible = future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)
            assertThat(isVisible).isEqualTo(expectedIsVisible)
        }
    }

    private fun assumeRoleIsVisible() {
        assumeTrue(isRoleVisible(ROLE_NAME))
    }

    @Test
    fun systemGalleryRoleIsNotVisible() {
        // The system gallery role should always be hidden.
        assertThat(isRoleVisible(SYSTEM_GALLERY_ROLE_NAME)).isEqualTo(false)
    }

    @Test
    fun invalidRoleIsNotVisible() {
        assertThat(isRoleVisible("invalid")).isEqualTo(false)
    }

    private fun isRoleVisible(roleName: String): Boolean =
        runWithShellPermissionIdentity(ThrowingSupplier {
            val future = CompletableFuture<Boolean>()
            roleControllerManager.isRoleVisible(
                roleName, context.mainExecutor, Consumer { future.complete(it) }
            )
            future.get(TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)
        })

    private fun installPackage(apkPath: String) {
        assertEquals(
            "Success",
            runShellCommand("pm install -r --user ${Process.myUserHandle().identifier} $apkPath")
                .trim()
        )
    }

    private fun uninstallPackage(packageName: String) {
        assertEquals(
            "Success",
            runShellCommand("pm uninstall --user ${Process.myUserHandle().identifier} $packageName")
                .trim()
        )
    }

    companion object {
        private const val ROLE_NAME = RoleManager.ROLE_BROWSER
        private const val APP_APK_PATH = "/data/local/tmp/cts/role/CtsRoleTestApp.apk"
        private const val APP_PACKAGE_NAME = "android.app.role.cts.app"
        private const val SYSTEM_GALLERY_ROLE_NAME = "android.app.role.SYSTEM_GALLERY"
        private const val TIMEOUT_MILLIS = 15 * 1000L
    }
}
