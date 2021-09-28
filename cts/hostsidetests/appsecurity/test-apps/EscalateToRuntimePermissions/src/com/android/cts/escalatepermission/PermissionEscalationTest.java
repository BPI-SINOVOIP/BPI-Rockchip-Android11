/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.cts.escalatepermission;

import static org.junit.Assert.assertSame;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.cts.escalate.permission.Manifest;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PermissionEscalationTest {
    @Test
    public void testCannotEscalateNonRuntimePermissionsToRuntime() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();

        // Ensure normal permission cannot be made dangerous
        PermissionInfo stealAudio1Permission1 = context.getPackageManager()
                .getPermissionInfo(Manifest.permission.STEAL_AUDIO1, 0);
        assertSame("Shouldn't be able to change normal permission to dangerous",
                PermissionInfo.PROTECTION_NORMAL, (stealAudio1Permission1.protectionLevel
                        & PermissionInfo.PROTECTION_MASK_BASE));

        // Ensure signature permission cannot be made dangerous
        PermissionInfo stealAudio1Permission2 = context.getPackageManager()
                .getPermissionInfo(Manifest.permission.STEAL_AUDIO2, 0);
        assertSame("Shouldn't be able to change signature permission to dangerous",
                PermissionInfo.PROTECTION_SIGNATURE, (stealAudio1Permission2.protectionLevel
                        & PermissionInfo.PROTECTION_MASK_BASE));
     }
     
     @Test
     public void testRuntimePermissionsAreNotGranted() throws Exception {
         // TODO (b/172366747): It is weird that the permission cannot become a runtime permission
         //                     during runtime but can become one during reboot.
         Context context = InstrumentationRegistry.getTargetContext();

         // Ensure permission is now dangerous but denied
         PermissionInfo stealAudio1Permission1 = context.getPackageManager()
                 .getPermissionInfo(Manifest.permission.STEAL_AUDIO1, 0);
         assertSame("Signature permission can become dangerous after reboot",
                 PermissionInfo.PROTECTION_DANGEROUS, (stealAudio1Permission1.protectionLevel
                        & PermissionInfo.PROTECTION_MASK_BASE));

         assertSame("Permission should be denied",
                 context.checkSelfPermission(Manifest.permission.STEAL_AUDIO1),
                 PackageManager.PERMISSION_DENIED);

         // Ensure permission is now dangerous but denied
         PermissionInfo stealAudio1Permission2 = context.getPackageManager()
                 .getPermissionInfo(Manifest.permission.STEAL_AUDIO2, 0);
         assertSame("Signature permission can become dangerous after reboot",
                 PermissionInfo.PROTECTION_DANGEROUS, (stealAudio1Permission2.protectionLevel
                         & PermissionInfo.PROTECTION_MASK_BASE));

         assertSame("Permission should be denied",
                 context.checkSelfPermission(Manifest.permission.STEAL_AUDIO2),
                 PackageManager.PERMISSION_DENIED);
    }
}
