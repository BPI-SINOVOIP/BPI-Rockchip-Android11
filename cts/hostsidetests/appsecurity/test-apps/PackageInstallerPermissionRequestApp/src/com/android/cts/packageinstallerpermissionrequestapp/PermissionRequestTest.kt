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

package com.android.cts.packageinstallerpermissionrequestapp

import android.Manifest
import android.appsecurity.cts.PackageSetInstallerConstants
import android.appsecurity.cts.PackageSetInstallerConstants.SHOULD_THROW_EXCEPTION_KEY
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.test.InstrumentationRegistry
import com.google.common.truth.Truth.assertThat
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.ExpectedException

class PermissionRequestTest {

    @get:Rule
    val expectedException = ExpectedException.none()

    private val arguments: Bundle = InstrumentationRegistry.getArguments()
    private val permission = arguments.getString(PackageSetInstallerConstants.PERMISSION_KEY)!!
    private val context: Context = InstrumentationRegistry.getContext()
    private val packageName = context.packageName
    private val packageManager = context.packageManager

    @Before
    @After
    fun verifyPermissionsDeniedAndInstallerUnchanged() {
        assertThat(context.checkSelfPermission(permission))
                .isEqualTo(PackageManager.PERMISSION_DENIED)
        assertThat(context.checkSelfPermission(Manifest.permission.INSTALL_PACKAGES))
                .isEqualTo(PackageManager.PERMISSION_DENIED)
        assertThat(packageManager.getInstallerPackageName(packageName))
                .isEqualTo(null)
    }

    @Test
    fun setSelfAsInstallerAndWhitelistPermission() {
        val shouldThrowException = arguments.getString(SHOULD_THROW_EXCEPTION_KEY)!!.toBoolean()
        if (shouldThrowException) {
            expectedException.expect(SecurityException::class.java)
        }

        try {
            // This set call should fail
            packageManager.setInstallerPackageName(packageName, packageName)
        } finally {
            // But also call the whitelist method to attempt to take the permission regardless.
            // If this fails, which it should, it should also be a SecurityException.
            try {
                packageManager.addWhitelistedRestrictedPermission(packageName,
                        permission, PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER)
                Assert.fail("addWhitelistedRestrictedPermission did not throw SecurityException")
            } catch (ignored: SecurityException) {
                // Ignore this to defer to shouldThrowException from above call
            }
        }
    }
}
