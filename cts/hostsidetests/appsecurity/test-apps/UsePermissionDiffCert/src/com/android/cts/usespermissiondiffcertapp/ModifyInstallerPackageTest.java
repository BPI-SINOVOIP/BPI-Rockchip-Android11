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

package com.android.cts.usespermissiondiffcertapp;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.InstallSourceInfo;
import android.content.pm.PackageManager;
import android.content.pm.SigningInfo;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.permissiondeclareapp.UtilsProvider;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that one application can and can not modify the installer package
 * of another application is appropriate.
 *
 * Accesses app cts/tests/appsecurity-tests/test-apps/PermissionDeclareApp/...
 */
@RunWith(AndroidJUnit4.class)
public class ModifyInstallerPackageTest {
    static final String OTHER_PACKAGE = "com.android.cts.permissiondeclareapp";
    static final String MY_PACKAGE = "com.android.cts.usespermissiondiffcertapp";

    static void assertPackageInstallerAndInitiator(String packageName,
            String expectedInstaller, String expectedInitiator, PackageManager packageManager)
            throws Exception {
        assertEquals(expectedInstaller, packageManager.getInstallerPackageName(packageName));
        final InstallSourceInfo installSourceInfo =
                packageManager.getInstallSourceInfo(packageName);
        assertEquals(expectedInstaller, installSourceInfo.getInstallingPackageName());
        assertEquals(expectedInitiator, installSourceInfo.getInitiatingPackageName());

        // We should get the initiator's signature iff we have an initiator.
        if (expectedInitiator == null) {
            assertNull(installSourceInfo.getInitiatingPackageSigningInfo());
        } else {
            final SigningInfo expectedSigning = packageManager.getPackageInfo(expectedInitiator,
                    PackageManager.GET_SIGNING_CERTIFICATES).signingInfo;
            final SigningInfo actualSigning = installSourceInfo.getInitiatingPackageSigningInfo();
            assertThat(actualSigning.getApkContentsSigners()).asList()
                    .containsExactlyElementsIn(expectedSigning.getApkContentsSigners());
        }
    }

    private Context context = InstrumentationRegistry.getContext();

    private PackageManager mPM = context.getPackageManager();

    @BeforeClass
    public static void adoptPermissions() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.INSTALL_PACKAGES);
    }

    @AfterClass
    public static void dropPermissions() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Test that we can set the installer package name (but not the initiating package name).
     */
    @Test
    public void setInstallPackage() throws Exception {
        // Pre-condition.
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, mPM);

        mPM.setInstallerPackageName(OTHER_PACKAGE, MY_PACKAGE);
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, MY_PACKAGE, null, mPM);

        // Clean up.
        mPM.setInstallerPackageName(OTHER_PACKAGE, null);
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, mPM);
    }

    /**
     * Test that we fail if trying to set an installer package with an unknown
     * target package name.
     */
    @Test
    public void setInstallPackageBadTarget() throws Exception {
        try {
            mPM.setInstallerPackageName("thisdoesnotexistihope!", MY_PACKAGE);
            fail("setInstallerPackageName did not throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // That's what we want!
        }
    }

    /**
     * Test that we fail if trying to set an installer package with an unknown
     * installer package name.
     */
    @Test
    public void setInstallPackageBadInstaller() throws Exception {
        try {
            mPM.setInstallerPackageName(OTHER_PACKAGE, "thisdoesnotexistihope!");
            fail("setInstallerPackageName did not throw IllegalArgumentException");
        } catch (IllegalArgumentException e) {
            // That's what we want!
        }
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, mPM);
    }

    /**
     * Test that we fail if trying to set an installer package that is not
     * signed with our cert.
     */
    @Test
    public void setInstallPackageWrongCertificate() throws Exception {
        // Pre-condition.
        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, mPM);

        try {
            mPM.setInstallerPackageName(OTHER_PACKAGE, OTHER_PACKAGE);
            fail("setInstallerPackageName did not throw SecurityException");
        } catch (SecurityException e) {
            // That's what we want!
        }

        assertPackageInstallerAndInitiator(OTHER_PACKAGE, null, null, mPM);
    }

    private void call(Intent intent) {
        final Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_INTENT, intent);
        context.getContentResolver().call(UtilsProvider.URI, "", "", extras);
    }
}
