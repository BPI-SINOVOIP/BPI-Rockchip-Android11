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

package android.permission.cts;

import android.content.Context;
import android.content.pm.PackageManager;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Test;

public class BackgroundPermissionButtonLabelTest {

    // Name of the resource which provides background permission button string
    public static final String APP_PERMISSION_BUTTON_ALLOW_ALWAYS =
            "app_permission_button_allow_always";

    private final Context mContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private final String mPermissionController =
            mContext.getPackageManager().getPermissionControllerPackageName();

    @Test
    public void testBackgroundPermissionButtonLabel() {
        try {
            Context permissionControllerContext =
                    mContext.createPackageContext(mPermissionController, 0);
            int stringId = permissionControllerContext.getResources().getIdentifier(
                    APP_PERMISSION_BUTTON_ALLOW_ALWAYS, "string",
                    "com.android.permissioncontroller");

            Assert.assertEquals(mContext.getPackageManager().getBackgroundPermissionOptionLabel(),
                    permissionControllerContext.getString(stringId));
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }

    }

}
