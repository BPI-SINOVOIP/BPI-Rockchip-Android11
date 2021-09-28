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

package com.android.cts.packageinstallerpermissionwhitelistapp

import android.Manifest
import android.appsecurity.cts.PackageSetInstallerConstants
import android.appsecurity.cts.PackageSetInstallerConstants.PERMISSION_KEY
import android.appsecurity.cts.PackageSetInstallerConstants.SHOULD_THROW_EXCEPTION_KEY
import android.appsecurity.cts.PackageSetInstallerConstants.TARGET_PKG
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.test.InstrumentationRegistry
import com.android.compatibility.common.util.ShellIdentityUtils
import com.google.common.truth.Truth.assertThat
import org.junit.Before
import org.junit.Test
import org.testng.Assert.assertThrows

class PermissionWhitelistTest {

    private val arguments: Bundle = InstrumentationRegistry.getArguments()
    private val shouldThrowException = arguments.getString(SHOULD_THROW_EXCEPTION_KEY)
            ?.toBoolean() ?: false
    private val permission = arguments.getString(PERMISSION_KEY)!!
    private val context: Context = InstrumentationRegistry.getContext()
    private val packageName = context.packageName
    private val packageManager = context.packageManager

    @Before
    fun verifyNoInstallPackagesPermissions() {
        // This test adopts shell permissions to get INSTALL_PACKAGES. That ensures that the
        // whitelist package having INSTALL_PACKAGES doesn't bypass any checks. In a realistic
        // scenario, this whitelisting app would request install, not directly install the target
        // package.
        assertThat(context.checkSelfPermission(Manifest.permission.INSTALL_PACKAGES))
                .isEqualTo(PackageManager.PERMISSION_DENIED)
    }

    @Test
    fun setTargetInstallerPackage() {
        assertTargetInstaller(null)

        val block = {
            packageManager.setInstallerPackageName(TARGET_PKG, packageName)
        }

        if (shouldThrowException) {
            assertThrows(SecurityException::class.java) { block() }
        } else {
            block()
        }

        assertTargetInstaller(null)

        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(packageManager,
                ShellIdentityUtils.ShellPermissionMethodHelperNoReturn {
                    it.setInstallerPackageName(TARGET_PKG, packageName)
                }, Manifest.permission.INSTALL_PACKAGES)
        assertTargetInstaller(packageName)
    }

    @Test
    fun removeWhitelistRestrictedPermission() {
        val block = {
            packageManager.removeWhitelistedRestrictedPermission(TARGET_PKG, permission,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER)
        }

        val shouldSucceed = arguments.getString(
                PackageSetInstallerConstants.SHOULD_SUCCEED_KEY)
        if (shouldSucceed == null) {
            assertThrows(SecurityException::class.java) { block() }
        } else {
            assertThat(block()).isEqualTo(shouldSucceed.toBoolean())
        }

        assertThat(packageManager.checkPermission(permission, TARGET_PKG))
                .isEqualTo(PackageManager.PERMISSION_DENIED)
    }

    private fun assertTargetInstaller(installer: String?) {
        assertThat(packageManager.getInstallerPackageName(TARGET_PKG))
                .isEqualTo(installer)
    }
}
