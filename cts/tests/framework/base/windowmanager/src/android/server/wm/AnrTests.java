/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.server.wm.app.Components.HOST_ACTIVITY;
import static android.server.wm.app.Components.UNRESPONSIVE_ACTIVITY;
import static android.server.wm.app.Components.UnresponsiveActivity;
import static android.server.wm.app.Components.UnresponsiveActivity.EXTRA_DELAY_UI_THREAD_MS;
import static android.server.wm.app.Components.UnresponsiveActivity.EXTRA_ON_CREATE_DELAY_MS;
import static android.server.wm.app.Components.UnresponsiveActivity.EXTRA_ON_KEYDOWN_DELAY_MS;
import static android.server.wm.app.Components.UnresponsiveActivity.EXTRA_ON_MOTIONEVENT_DELAY_MS;
import static android.view.Display.DEFAULT_DISPLAY;

import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.app.ActivityTaskManager;
import android.content.ComponentName;
import android.os.SystemClock;
import android.platform.test.annotations.Presubmit;
import android.server.wm.app.Components.RenderService;
import android.provider.Settings;
import android.server.wm.settings.SettingsSession;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;
import android.util.EventLog;
import android.util.Log;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.List;

import androidx.test.filters.FlakyTest;
import androidx.test.platform.app.InstrumentationRegistry;

/**
 * Test scenarios that lead to ANR dialog being shown.
 *
 * <p>Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:AnrTests
 */
@Presubmit
@FlakyTest(bugId = 143047723)
@android.server.wm.annotation.Group3
public class AnrTests extends ActivityManagerTestBase {
    private static final String TAG = "AnrTests";
    private LogSeparator mLogSeparator;
    private SettingsSession<Integer> mHideDialogSetting;

    @Before
    public void setup() throws Exception {
        super.setUp();
        assumeTrue(mAtm.currentUiModeSupportsErrorDialogs(mContext));

        mLogSeparator = separateLogs(); // add a new separator for logs
        mHideDialogSetting = new SettingsSession<>(
                Settings.Global.getUriFor(Settings.Global.HIDE_ERROR_DIALOGS),
                Settings.Global::getInt, Settings.Global::putInt);
        mHideDialogSetting.set(0);
    }

    @After
    public void teardown() {
        if (mHideDialogSetting != null) mHideDialogSetting.close();
        stopTestPackage(UNRESPONSIVE_ACTIVITY.getPackageName());
        stopTestPackage(HOST_ACTIVITY.getPackageName());
    }

    @Test
    public void slowOnCreateWithKeyEventTriggersAnr() {
        startUnresponsiveActivity(EXTRA_ON_CREATE_DELAY_MS, false /* waitForCompletion */,
                UNRESPONSIVE_ACTIVITY);
        // wait for app to be focused
        mWmState.waitAndAssertAppFocus(UNRESPONSIVE_ACTIVITY.getPackageName(),
                2000 /* waitTime_ms */);
        // wait for input manager to get the new focus app. This sleep can be removed once we start
        // listing to input about the focused app.
        SystemClock.sleep(500);
        injectKey(KeyEvent.KEYCODE_BACK, false /* longpress */, false /* sync */);
        clickCloseAppOnAnrDialog();
        assertEventLogsContainsAnr(UnresponsiveActivity.PROCESS_NAME);
    }

    @Test
    public void slowUiThreadWithKeyEventTriggersAnr() {
        startUnresponsiveActivity(EXTRA_DELAY_UI_THREAD_MS, true /* waitForCompletion */,
                UNRESPONSIVE_ACTIVITY);
        injectKey(KeyEvent.KEYCODE_BACK, false /* longpress */, false /* sync */);
        clickCloseAppOnAnrDialog();
        assertEventLogsContainsAnr(UnresponsiveActivity.PROCESS_NAME);
    }

    @Test
    public void slowOnKeyEventHandleTriggersAnr() {
        startUnresponsiveActivity(EXTRA_ON_KEYDOWN_DELAY_MS, true /* waitForCompletion */,
                UNRESPONSIVE_ACTIVITY);
        injectKey(KeyEvent.KEYCODE_BACK, false /* longpress */, false /* sync */);
        clickCloseAppOnAnrDialog();
        assertEventLogsContainsAnr(UnresponsiveActivity.PROCESS_NAME);
    }

    @Test
    public void slowOnTouchEventHandleTriggersAnr() {
        startUnresponsiveActivity(EXTRA_ON_MOTIONEVENT_DELAY_MS, true /* waitForCompletion */,
                UNRESPONSIVE_ACTIVITY);

        // TODO(b/143566069) investigate why we need multiple taps on display to trigger anr.
        mWmState.computeState();
        tapOnDisplayCenterAsync(DEFAULT_DISPLAY);
        SystemClock.sleep(1000);
        tapOnDisplayCenterAsync(DEFAULT_DISPLAY);

        clickCloseAppOnAnrDialog();
        assertEventLogsContainsAnr(UnresponsiveActivity.PROCESS_NAME);
    }

    /**
     * Verify embedded windows can trigger ANR and the verify embedded app is blamed.
     */
    @Test
    public void embeddedWindowTriggersAnr() {
        startUnresponsiveActivity(EXTRA_ON_MOTIONEVENT_DELAY_MS, true  /* waitForCompletion */,
                HOST_ACTIVITY);

        // TODO(b/143566069) investigate why we need multiple taps on display to trigger anr.
        mWmState.computeState();
        tapOnDisplayCenterAsync(DEFAULT_DISPLAY);
        SystemClock.sleep(1000);
        tapOnDisplayCenterAsync(DEFAULT_DISPLAY);

        clickCloseAppOnAnrDialog();
        assertEventLogsContainsAnr(RenderService.PROCESS_NAME);
    }

    private void assertEventLogsContainsAnr(String processName) {
        final List<EventLog.Event> events = getEventLogsForComponents(mLogSeparator,
                android.util.EventLog.getTagCode("am_anr"));
        for (EventLog.Event event : events) {
            Object[] arr = (Object[]) event.getData();
            final String name = (String) arr[2];
            if (name.equals(processName)) {
                return;
            }
        }
        fail("Could not find anr kill event for " + processName);
    }

    private void clickCloseAppOnAnrDialog() {
        // Find anr dialog and kill app
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        UiObject2 closeAppButton = uiDevice.wait(Until.findObject(By.res("android:id/aerr_close")),
                20000);
        if (closeAppButton != null) {
            Log.d(TAG, "found permission dialog after searching all windows, clicked");
            closeAppButton.click();
            return;
        }
        fail("Could not find anr dialog");
    }

    private void startUnresponsiveActivity(String delayTypeExtra, boolean waitForCompletion,
            ComponentName activity) {
        String flags = waitForCompletion ? " -W -n " : " -n ";
        String startCmd = "am start" + flags + activity.flattenToString() +
                " --ei " + delayTypeExtra + " 30000";
        executeShellCommand(startCmd);
    }
}
