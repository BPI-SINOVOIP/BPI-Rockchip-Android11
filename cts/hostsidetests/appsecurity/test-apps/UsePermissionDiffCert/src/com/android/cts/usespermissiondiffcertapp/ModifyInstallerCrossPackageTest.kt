/*
 * Copyright (C) 2009 The Android Open Source Project
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
package com.android.cts.usespermissiondiffcertapp

import android.Manifest
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import com.android.cts.permissiondeclareapp.UtilsProvider
import com.android.cts.usespermissiondiffcertapp.ModifyInstallerPackageTest.MY_PACKAGE
import com.android.cts.usespermissiondiffcertapp.ModifyInstallerPackageTest.OTHER_PACKAGE
import com.android.cts.usespermissiondiffcertapp.ModifyInstallerPackageTest.assertPackageInstallerAndInitiator
import org.junit.AfterClass
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.testng.Assert.assertThrows

/**
 * Tests that one application can and can not modify the installer package
 * of another application is appropriate.
 *
 * This is split from [ModifyInstallerPackageTest] to allow the host side test to instrument
 * both this app and PermissionDeclareApp in alternating fashion, using adoptShellPermissionIdentity
 * to grant them the INSTALL_PACKAGES permission they would normally not be able to hold inside CTS.
 */
@RunWith(AndroidJUnit4::class)
class ModifyInstallerCrossPackageTest {

    companion object {
        @JvmStatic
        @BeforeClass
        fun adoptPermissions() {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .adoptShellPermissionIdentity(Manifest.permission.INSTALL_PACKAGES)
        }

        @JvmStatic
        @AfterClass
        fun dropPermissions() {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity()
        }
    }

    private val context = InstrumentationRegistry.getContext()
    private val packageManager = context.packageManager

    /**
     * Excludes this test when running an instrumentation runner against the entire package,
     * requiring a specific call and parameter to have the method run. Parses as a string since
     * that's all that runDeviceTests supports.
     *
     * Must be @[Before] because testing reporting infrastructure doesn't support assumptions in
     * @[BeforeClass].
     */
    @Before
    fun assumeRunExplicit() {
        assumeTrue(InstrumentationRegistry.getArguments()
                .getString(UtilsProvider.EXTRA_RUN_EXPLICIT)?.toBoolean() ?: false)
    }

    @Test
    fun assertBefore() {
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, packageManager)
    }

    @Test
    fun attemptTakeOver() {
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, OTHER_PACKAGE, null, packageManager)
        assertThrows { packageManager.setInstallerPackageName(OTHER_PACKAGE, MY_PACKAGE) }
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, OTHER_PACKAGE, null, packageManager)
    }

    @Test
    fun assertAfter() {
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, packageManager)
    }
}
