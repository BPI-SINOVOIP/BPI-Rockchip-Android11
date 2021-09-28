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

package android.server.wm;

import static android.server.wm.UiDeviceUtils.pressBackButton;
import static android.server.wm.deprecatedsdk.Components.MAIN_ACTIVITY;

import android.platform.test.annotations.Presubmit;

import org.junit.After;
import org.junit.Test;

/**
 * Ensure that compatibility dialog is shown when launching an application
 * targeting a deprecated version of SDK.
 * <p>Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:DeprecatedTargetSdkTest
 */
@Presubmit
public class DeprecatedTargetSdkTest extends ActivityManagerTestBase {

    /** @see com.android.server.wm.DeprecatedTargetSdkVersionDialog */
    private static final String DEPRECATED_TARGET_SDK_VERSION_DIALOG =
            "DeprecatedTargetSdkVersionDialog";

    @After
    public void tearDown() {
        // Ensure app process is stopped.
        stopTestPackage(MAIN_ACTIVITY.getPackageName());
    }

    @Test
    public void testCompatibilityDialog() throws Exception {
        // Launch target app.
        launchActivity(MAIN_ACTIVITY);
        mWmState.assertActivityDisplayed(MAIN_ACTIVITY);
        mWmState.assertWindowDisplayed(DEPRECATED_TARGET_SDK_VERSION_DIALOG);

        // Go back to dismiss the warning dialog.
        pressBackButton();

        // Go back again to formally stop the app. If we just kill the process, it'll attempt to
        // resume rather than starting from scratch (as far as ActivityStack is concerned) and it
        // won't invoke the warning dialog.
        pressBackButton();
    }
}
