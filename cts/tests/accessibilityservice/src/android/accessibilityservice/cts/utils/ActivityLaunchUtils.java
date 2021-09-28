/**
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts.utils;

import static android.accessibility.cts.common.ShellCommandBuilder.execShellCommand;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.content.pm.PackageManager.FEATURE_ACTIVITIES_ON_SECONDARY_DISPLAYS;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.app.ActivityOptions;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Rect;
import android.os.PowerManager;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.TestUtils;

import java.util.List;
import java.util.Objects;
import java.util.function.BooleanSupplier;
import java.util.stream.Collectors;

/**
 * Utilities useful when launching an activity to make sure it's all the way on the screen
 * before we start testing it.
 */
public class ActivityLaunchUtils {
    private static final String LOG_TAG = "ActivityLaunchUtils";
    private static final String AM_START_HOME_ACTIVITY_COMMAND =
            "am start -a android.intent.action.MAIN -c android.intent.category.HOME";
    private static final String AM_BROADCAST_CLOSE_SYSTEM_DIALOG_COMMAND =
            "am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS";

    // Using a static variable so it can be used in lambdas. Not preserving state in it.
    private static Activity mTempActivity;

    public static <T extends Activity> T launchActivityAndWaitForItToBeOnscreen(
            Instrumentation instrumentation, UiAutomation uiAutomation,
            ActivityTestRule<T> rule) throws Exception {
        ActivityLauncher activityLauncher = new ActivityLauncher() {
            @Override
            Activity launchActivity() {
                return rule.launchActivity(null);
            }
        };
        return launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(instrumentation,
                uiAutomation, activityLauncher, Display.DEFAULT_DISPLAY);
    }

    /**
     * If this activity would be launched at virtual display, please finishes this activity before
     * this test ended. Otherwise it will be displayed on default display and impacts the next test.
     */
    public static <T extends Activity> T launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
            Instrumentation instrumentation, UiAutomation uiAutomation, Class<T> clazz,
            int displayId) throws Exception {
        final ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(displayId);
        final Intent intent = new Intent(instrumentation.getTargetContext(), clazz);
        // Add clear task because this activity may on other display.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK|Intent.FLAG_ACTIVITY_NEW_TASK);

        ActivityLauncher activityLauncher = new ActivityLauncher() {
            @Override
            Activity launchActivity() {
                uiAutomation.adoptShellPermissionIdentity();
                try {
                    return instrumentation.startActivitySync(intent, options.toBundle());
                } finally {
                    uiAutomation.dropShellPermissionIdentity();
                }
            }
        };
        return launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(instrumentation,
                uiAutomation, activityLauncher, displayId);
    }

    public static CharSequence getActivityTitle(
            Instrumentation instrumentation, Activity activity) {
        final StringBuilder titleBuilder = new StringBuilder();
        instrumentation.runOnMainSync(() -> titleBuilder.append(activity.getTitle()));
        return titleBuilder;
    }

    public static AccessibilityWindowInfo findWindowByTitle(
            UiAutomation uiAutomation, CharSequence title) {
        final List<AccessibilityWindowInfo> windows = uiAutomation.getWindows();
        return findWindowByTitleWithList(title, windows);
    }

    public static AccessibilityWindowInfo findWindowByTitleAndDisplay(
            UiAutomation uiAutomation, CharSequence title, int displayId) {
        final SparseArray<List<AccessibilityWindowInfo>> allWindows =
                uiAutomation.getWindowsOnAllDisplays();
        final List<AccessibilityWindowInfo> windowsOfDisplay = allWindows.get(displayId);
        return findWindowByTitleWithList(title, windowsOfDisplay);
    }

    public static void homeScreenOrBust(Context context, UiAutomation uiAutomation) {
        wakeUpOrBust(context, uiAutomation);
        if (context.getPackageManager().isInstantApp()) return;
        if (isHomeScreenShowing(context, uiAutomation)) return;
        try {
            executeAndWaitOn(
                    uiAutomation,
                    () -> {
                        execShellCommand(uiAutomation, AM_START_HOME_ACTIVITY_COMMAND);
                        execShellCommand(uiAutomation, AM_BROADCAST_CLOSE_SYSTEM_DIALOG_COMMAND);
                    },
                    () -> isHomeScreenShowing(context, uiAutomation),
                    DEFAULT_TIMEOUT_MS,
                    "home screen");
        } catch (AssertionError error) {
            Log.e(LOG_TAG, "Timed out looking for home screen. Dumping window list");
            final List<AccessibilityWindowInfo> windows = uiAutomation.getWindows();
            if (windows == null) {
                Log.e(LOG_TAG, "Window list is null");
            } else if (windows.isEmpty()) {
                Log.e(LOG_TAG, "Window list is empty");
            } else {
                for (AccessibilityWindowInfo window : windows) {
                    Log.e(LOG_TAG, window.toString());
                }
            }

            fail("Unable to reach home screen");
        }
    }

    public static boolean supportsMultiDisplay(Context context) {
        return context.getPackageManager().hasSystemFeature(
                FEATURE_ACTIVITIES_ON_SECONDARY_DISPLAYS);
    }

    private static boolean isHomeScreenShowing(Context context, UiAutomation uiAutomation) {
        final List<AccessibilityWindowInfo> windows = uiAutomation.getWindows();
        final PackageManager packageManager = context.getPackageManager();
        final List<ResolveInfo> resolveInfos = packageManager.queryIntentActivities(
                new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME),
                PackageManager.MATCH_DEFAULT_ONLY);

        // Look for a window with a package name that matches the default home screen
        for (AccessibilityWindowInfo window : windows) {
            final AccessibilityNodeInfo root = window.getRoot();
            if (root != null) {
                final CharSequence packageName = root.getPackageName();
                if (packageName != null) {
                    for (ResolveInfo resolveInfo : resolveInfos) {
                        if ((resolveInfo.activityInfo != null)
                                && packageName.equals(resolveInfo.activityInfo.packageName)) {
                            return true;
                        }
                    }
                }
            }
        }
        // List unexpected package names of default home screen that invoking ResolverActivity
        final CharSequence homePackageNames = resolveInfos.stream()
                .map(r -> r.activityInfo).filter(Objects::nonNull)
                .map(a -> a.packageName).collect(Collectors.joining(", "));
        Log.v(LOG_TAG, "No window matched with package names of home screen: " + homePackageNames);
        return false;
    }

    private static void wakeUpOrBust(Context context, UiAutomation uiAutomation) {
        final long deadlineUptimeMillis = SystemClock.uptimeMillis() + DEFAULT_TIMEOUT_MS;
        final PowerManager powerManager = context.getSystemService(PowerManager.class);
        do {
            if (powerManager.isInteractive()) {
                Log.d(LOG_TAG, "Device is interactive");
                return;
            }

            Log.d(LOG_TAG, "Sending wakeup keycode");
            final long eventTime = SystemClock.uptimeMillis();
            uiAutomation.injectInputEvent(
                    new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN,
                            KeyEvent.KEYCODE_WAKEUP, 0 /* repeat */, 0 /* metastate */,
                            KeyCharacterMap.VIRTUAL_KEYBOARD, 0 /* scancode */, 0 /* flags */,
                            InputDevice.SOURCE_KEYBOARD), true /* sync */);
            uiAutomation.injectInputEvent(
                    new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP,
                            KeyEvent.KEYCODE_WAKEUP, 0 /* repeat */, 0 /* metastate */,
                            KeyCharacterMap.VIRTUAL_KEYBOARD, 0 /* scancode */, 0 /* flags */,
                            InputDevice.SOURCE_KEYBOARD), true /* sync */);
            try {
                Thread.sleep(50);
            } catch (InterruptedException e) {}
        } while (SystemClock.uptimeMillis() < deadlineUptimeMillis);
        fail("Unable to wake up screen");
    }

    /**
     * Executes a command and waits for a specified condition up to a given wait timeout. It checks
     * condition result each time when events delivered, and throws exception if the condition
     * result is not {@code true} within the given timeout.
     */
    private static void executeAndWaitOn(UiAutomation uiAutomation, Runnable command,
            BooleanSupplier condition, long timeoutMillis, String conditionName) {
        final Object waitObject = new Object();
        final long executionStartTimeMillis = SystemClock.uptimeMillis();
        try {
            uiAutomation.setOnAccessibilityEventListener((event) -> {
                if (event.getEventTime() < executionStartTimeMillis) {
                    return;
                }
                synchronized (waitObject) {
                    waitObject.notifyAll();
                }
            });
            command.run();
            TestUtils.waitOn(waitObject, condition, timeoutMillis, conditionName);
        } finally {
            uiAutomation.setOnAccessibilityEventListener(null);
        }
    }

    private static <T extends Activity> T launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
            Instrumentation instrumentation, UiAutomation uiAutomation,
            ActivityLauncher activityLauncher, int displayId) throws Exception {
        final int[] location = new int[2];
        final StringBuilder activityPackage = new StringBuilder();
        final Rect bounds = new Rect();
        final StringBuilder activityTitle = new StringBuilder();
        // Make sure we get window events, so we'll know when the window appears
        AccessibilityServiceInfo info = uiAutomation.getServiceInfo();
        info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
        uiAutomation.setServiceInfo(info);
        // There is no any window on virtual display even doing GLOBAL_ACTION_HOME, so only
        // checking the home screen for default display.
        if (displayId == Display.DEFAULT_DISPLAY) {
            homeScreenOrBust(instrumentation.getContext(), uiAutomation);
        }

        final AccessibilityEvent awaitedEvent = uiAutomation.executeAndWaitForEvent(
                () -> {
                    mTempActivity = activityLauncher.launchActivity();
                    instrumentation.runOnMainSync(() -> {
                        mTempActivity.getWindow().getDecorView().getLocationOnScreen(location);
                        activityPackage.append(mTempActivity.getPackageName());
                    });
                    instrumentation.waitForIdleSync();
                    activityTitle.append(getActivityTitle(instrumentation, mTempActivity));
                },
                (event) -> {
                    AccessibilityNodeInfo node = event.getSource();
                    if (node != null) {
                        final AccessibilityWindowInfo window = node.getWindow();
                        if(!TextUtils.equals(activityTitle, window.getTitle())) {
                            return  false;
                        }
                    }
                    final AccessibilityWindowInfo window =
                            findWindowByTitleAndDisplay(uiAutomation, activityTitle, displayId);
                    if (window == null) return false;
                    window.getBoundsInScreen(bounds);
                    mTempActivity.getWindow().getDecorView().getLocationOnScreen(location);
                    if (bounds.isEmpty()) {
                        return false;
                    }
                    return (!bounds.isEmpty())
                            && (bounds.left == location[0]) && (bounds.top == location[1]);
                }, DEFAULT_TIMEOUT_MS);
        assertNotNull(awaitedEvent);
        return (T) mTempActivity;
    }

    private static AccessibilityWindowInfo findWindowByTitleWithList(CharSequence title,
            List<AccessibilityWindowInfo> windows) {
        AccessibilityWindowInfo returnValue = null;
        if (windows != null && windows.size() > 0) {
            for (int i = 0; i < windows.size(); i++) {
                final AccessibilityWindowInfo window = windows.get(i);
                if (TextUtils.equals(title, window.getTitle())) {
                    returnValue = window;
                } else {
                    window.recycle();
                }
            }
        }
        return returnValue;
    }

    private static abstract class ActivityLauncher {
        abstract Activity launchActivity();
    }
}
