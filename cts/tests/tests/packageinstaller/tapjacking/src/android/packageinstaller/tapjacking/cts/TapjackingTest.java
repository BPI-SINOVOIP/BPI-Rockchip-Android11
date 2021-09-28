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
package android.packageinstaller.tapjacking.cts;

import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

@RunWith(AndroidJUnit4.class)
@MediumTest
@AppModeFull
public class TapjackingTest {

    private static final String LOG_TAG = TapjackingTest.class.getSimpleName();
    private static final String SYSTEM_PACKAGE_NAME = "android";
    private static final String INSTALL_BUTTON_ID = "button1";
    private static final String OVERLAY_ACTIVITY_TEXT_VIEW_ID = "overlay_description";
    private static final String WM_DISMISS_KEYGUARD_COMMAND = "wm dismiss-keyguard";
    private static final String TEST_APP_PACKAGE_NAME = "android.packageinstaller.emptytestapp.cts";

    private static final long WAIT_FOR_UI_TIMEOUT = 5000;

    private Context mContext;
    private String mPackageName;
    private UiDevice mUiDevice;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mPackageName = mContext.getPackageName();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        if (!mUiDevice.isScreenOn()) {
            mUiDevice.wakeUp();
        }
        mUiDevice.executeShellCommand(WM_DISMISS_KEYGUARD_COMMAND);
    }

    private void launchPackageInstaller() {
        Intent intent = new Intent(Intent.ACTION_INSTALL_PACKAGE);
        intent.setData(Uri.parse("package:" + TEST_APP_PACKAGE_NAME));
        intent.addFlags(FLAG_ACTIVITY_CLEAR_TASK | FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private void launchOverlayingActivity() {
        Intent intent = new Intent(mContext, OverlayingActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
    }

    private UiObject2 waitForView(String packageName, String id) {
        final BySelector selector = By.res(packageName, id);
        return mUiDevice.wait(Until.findObject(selector), WAIT_FOR_UI_TIMEOUT);
    }

    @Test
    public void testTapsDroppedWhenObscured() throws Exception {
        Log.i(LOG_TAG, "launchPackageInstaller");
        launchPackageInstaller();
        UiObject2 installButton = waitForView(SYSTEM_PACKAGE_NAME, INSTALL_BUTTON_ID);
        assertNotNull("Install button not shown", installButton);
        Log.i(LOG_TAG, "launchOverlayingActivity");
        launchOverlayingActivity();
        assertNotNull("Overlaying activity not started",
                waitForView(mPackageName, OVERLAY_ACTIVITY_TEXT_VIEW_ID));
        installButton = waitForView(SYSTEM_PACKAGE_NAME, INSTALL_BUTTON_ID);
        assertNotNull("Cannot find install button below overlay activity", installButton);
        Log.i(LOG_TAG, "installButton.click");
        installButton.click();
        assertFalse("Tap on install button succeeded", mUiDevice.wait(
                Until.gone(By.res(SYSTEM_PACKAGE_NAME, INSTALL_BUTTON_ID)),
                WAIT_FOR_UI_TIMEOUT));
        mUiDevice.pressBack();
    }

    @After
    public void tearDown() throws Exception {
        mUiDevice.pressHome();
    }
}
