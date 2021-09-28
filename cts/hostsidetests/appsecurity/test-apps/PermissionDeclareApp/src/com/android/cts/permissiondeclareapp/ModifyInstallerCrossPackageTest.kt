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

package com.android.cts.permissiondeclareapp

import android.Manifest
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import org.junit.AfterClass
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith

/**
 * The companion to UsePermissionDiffCert's ModifyInstallerCrossPackageTest which sets its own
 * installer package. These are purposely named identically so that the host test doesn't need
 * to swap test class names.
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
    fun takeInstaller() {
        packageManager.setInstallerPackageName(context.packageName, context.packageName)
    }

    @Test
    fun clearInstaller() {
        packageManager.setInstallerPackageName(context.packageName, null)
    }
}
