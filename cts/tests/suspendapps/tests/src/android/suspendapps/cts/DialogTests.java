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

package android.suspendapps.cts;

import static android.content.pm.SuspendDialogInfo.BUTTON_ACTION_UNSUSPEND;
import static android.suspendapps.cts.Constants.TEST_APP_PACKAGE_NAME;
import static android.suspendapps.cts.SuspendTestUtils.assertSameExtras;
import static android.suspendapps.cts.SuspendTestUtils.createExtras;
import static android.suspendapps.cts.SuspendTestUtils.startTestAppActivity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.SuspendDialogInfo;
import android.os.Bundle;
import android.platform.test.annotations.SystemUserOnly;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

@RunWith(AndroidJUnit4.class)
public class DialogTests {
    private static final String TEST_APP_LABEL = "Suspend Test App";
    private static final long UI_TIMEOUT_MS = 30_000;

    /** Used to poll for the intents sent by the system to this package */
    static final SynchronousQueue<Intent> sIncomingIntent = new SynchronousQueue<>();

    private Context mContext;
    private TestAppInterface mTestAppInterface;
    private UiDevice mUiDevice;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        mTestAppInterface = new TestAppInterface(mContext);
        turnScreenOn();
    }

    private void turnScreenOn() throws Exception {
        if (!mUiDevice.isScreenOn()) {
            mUiDevice.wakeUp();
        }
        SystemUtil.runShellCommandForNoOutput("wm dismiss-keyguard");
    }

    @Test
    public void testInterceptorActivity_unsuspend() throws Exception {
        final SuspendDialogInfo dialogInfo = new SuspendDialogInfo.Builder()
                .setIcon(R.drawable.ic_settings)
                .setTitle(R.string.dialog_title)
                .setMessage(R.string.dialog_message)
                .setNeutralButtonText(R.string.unsuspend_button_text)
                .setNeutralButtonAction(BUTTON_ACTION_UNSUSPEND)
                .build();
        SuspendTestUtils.suspend(null, null, dialogInfo);
        // Ensure test app's activity is stopped before proceeding.
        assertTrue(mTestAppInterface.awaitTestActivityStop());

        final Bundle extrasForStart = new Bundle(createExtras("unsuspend", 90, "sval", 0.9));
        startTestAppActivity(extrasForStart);
        // Test activity should not start.
        assertNull("Test activity started while suspended",
                mTestAppInterface.awaitTestActivityStart(5_000));

        final String expectedTitle = mContext.getResources().getString(R.string.dialog_title);
        final String expectedButtonText = mContext.getResources().getString(
                R.string.unsuspend_button_text);

        assertNotNull("Given dialog title \"" + expectedTitle + "\" not shown",
                mUiDevice.wait(Until.findObject(By.text(expectedTitle)), UI_TIMEOUT_MS));

        // Sometimes, button texts can have styles that override case (e.g. b/134033532)
        final Pattern buttonTextIgnoreCase = Pattern.compile(Pattern.quote(expectedButtonText),
                Pattern.CASE_INSENSITIVE);
        final UiObject2 unsuspendButton = mUiDevice.findObject(
                By.clickable(true).text(buttonTextIgnoreCase));
        assertNotNull("\"" + expectedButtonText + "\" button not shown", unsuspendButton);

        // Tapping on the neutral button should:
        // 1. Tell the suspending package that the test app was unsuspended
        // 2. Launch the previously intercepted intent
        // 3. Unsuspend the test app
        unsuspendButton.click();

        final Intent incomingIntent = sIncomingIntent.poll(30, TimeUnit.SECONDS);
        assertNotNull(incomingIntent);
        assertEquals(Intent.ACTION_PACKAGE_UNSUSPENDED_MANUALLY, incomingIntent.getAction());
        assertEquals("Did not receive correct unsuspended package name", TEST_APP_PACKAGE_NAME,
                incomingIntent.getStringExtra(Intent.EXTRA_PACKAGE_NAME));

        final Intent activityIntent = mTestAppInterface.awaitTestActivityStart();
        assertNotNull("Test activity did not start on neutral button tap", activityIntent);
        assertSameExtras("Different extras passed to startActivity on unsuspend",
                extrasForStart, activityIntent.getExtras());

        assertFalse("Test package still suspended", mTestAppInterface.isTestAppSuspended());
    }

    @Test
    public void testInterceptorActivity_moreDetails() throws Exception {
        final SuspendDialogInfo dialogInfo = new SuspendDialogInfo.Builder()
                .setIcon(R.drawable.ic_settings)
                .setTitle(R.string.dialog_title)
                .setMessage(R.string.dialog_message)
                .setNeutralButtonText(R.string.more_details_button_text)
                .build();
        SuspendTestUtils.suspend(null, null, dialogInfo);
        // Ensure test app's activity is stopped before proceeding.
        assertTrue(mTestAppInterface.awaitTestActivityStop());

        startTestAppActivity(null);
        // Test activity should not start.
        assertNull("Test activity started while suspended",
                mTestAppInterface.awaitTestActivityStart(5_000));

        // The dialog should have correct specifications
        final String expectedTitle = mContext.getResources().getString(R.string.dialog_title);
        final String expectedMessage = mContext.getResources().getString(R.string.dialog_message,
                TEST_APP_LABEL);
        final String expectedButtonText = mContext.getResources().getString(
                R.string.more_details_button_text);

        assertNotNull("Given dialog title: " + expectedTitle + " not shown",
                mUiDevice.wait(Until.findObject(By.text(expectedTitle)), UI_TIMEOUT_MS));
        assertNotNull("Given dialog message: " + expectedMessage + " not shown",
                mUiDevice.findObject(By.text(expectedMessage)));
        // Sometimes, button texts can have styles that override case (e.g. b/134033532)
        final Pattern buttonTextIgnoreCase = Pattern.compile(Pattern.quote(expectedButtonText),
                Pattern.CASE_INSENSITIVE);
        final UiObject2 moreDetailsButton = mUiDevice.findObject(
                By.clickable(true).text(buttonTextIgnoreCase));
        assertNotNull(expectedButtonText + " button not shown", moreDetailsButton);

        // Tapping on the neutral button should start the correct intent.
        moreDetailsButton.click();
        final Intent incomingIntent = sIncomingIntent.poll(30, TimeUnit.SECONDS);
        assertNotNull(incomingIntent);
        assertEquals(Intent.ACTION_SHOW_SUSPENDED_APP_DETAILS, incomingIntent.getAction());
        assertEquals("Wrong package name sent with " + Intent.ACTION_SHOW_SUSPENDED_APP_DETAILS,
                TEST_APP_PACKAGE_NAME, incomingIntent.getStringExtra(Intent.EXTRA_PACKAGE_NAME));
    }

    @After
    public void tearDown() {
        if (mTestAppInterface != null) {
            mTestAppInterface.disconnect();
        }
    }
}
