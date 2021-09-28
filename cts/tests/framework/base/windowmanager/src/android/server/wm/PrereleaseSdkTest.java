/*
 * Copyright (C) 2017 The Android Open Source Project
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
import static android.server.wm.prerelease.Components.MAIN_ACTIVITY;

import android.platform.test.annotations.Presubmit;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Ensure that compatibility dialog is shown when launching an application built
 * against a prerelease SDK.
 * <p>Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:PrereleaseSdkTest
 */
@Presubmit
public class PrereleaseSdkTest extends ActivityManagerTestBase {

    /** @see com.android.server.wm.UnsupportedCompileSdkDialog */
    private static final String UNSUPPORTED_COMPILE_SDK_DIALOG = "UnsupportedCompileSdkDialog";

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        SystemUtil.runWithShellPermissionIdentity(
                () -> mAm.alwaysShowUnsupportedCompileSdkWarning(MAIN_ACTIVITY));
    }

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
        mWmState.assertWindowDisplayed(UNSUPPORTED_COMPILE_SDK_DIALOG);

        // Go back to dismiss the warning dialog.
        pressBackButton();

        // Go back again to formally stop the app. If we just kill the process, it'll attempt to
        // resume rather than starting from scratch (as far as ActivityStack is concerned) and it
        // won't invoke the warning dialog.
        pressBackButton();
    }
}
