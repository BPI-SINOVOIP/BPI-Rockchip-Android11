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

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.AccessibilityEventFilterUtils.filterWindowsChangedWithChangeTypes;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.DisplayUtils.VirtualDisplaySession;
import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
import static android.view.accessibility.AccessibilityEvent.WINDOWS_CHANGE_ADDED;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.AccessibilityService.ScreenshotResult;
import android.accessibilityservice.AccessibilityService.TakeScreenshotCallback;
import android.accessibilityservice.cts.activities.AccessibilityWindowQueryActivity;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.hardware.HardwareBuffer;
import android.os.SystemClock;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageView;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Test cases for accessibility service takeScreenshot API.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityTakeScreenshotTest {
    /**
     * The timeout for waiting screenshot had been taken done.
     */
    private static final long TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS = 1000;
    public static final int SECURE_WINDOW_CONTENT_COLOR = Color.BLUE;

    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private InstrumentedAccessibilityServiceTestRule<StubTakeScreenshotService> mServiceRule =
            new InstrumentedAccessibilityServiceTestRule<>(StubTakeScreenshotService.class);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    private StubTakeScreenshotService mService;
    private Context mContext;
    private Point mDisplaySize;
    private long mStartTestingTime;
    @Mock
    private TakeScreenshotCallback mCallback;
    @Captor
    private ArgumentCaptor<ScreenshotResult> mSuccessResultArgumentCaptor;

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation();
    }

    @AfterClass
    public static void finalTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mService = mServiceRule.getService();
        mContext = mService.getApplicationContext();

        WindowManager windowManager =
                (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        final Display display = windowManager.getDefaultDisplay();

        mDisplaySize = new Point();
        display.getRealSize(mDisplaySize);
    }

    @Test
    public void testTakeScreenshot_GetScreenshotResult() {
        takeScreenshot(Display.DEFAULT_DISPLAY);
        verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onSuccess(
                mSuccessResultArgumentCaptor.capture());

        verifyScreenshotResult(mSuccessResultArgumentCaptor.getValue());
    }

    @Test
    public void testTakeScreenshot_RequestIntervalTime() throws Exception {
        takeScreenshot(Display.DEFAULT_DISPLAY);
        verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onSuccess(
                mSuccessResultArgumentCaptor.capture());

        Thread.sleep(
                AccessibilityService.ACCESSIBILITY_TAKE_SCREENSHOT_REQUEST_INTERVAL_TIMES_MS / 2);
        // Requests the API again during interval time from calling the first time.
        takeScreenshot(Display.DEFAULT_DISPLAY);
        verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onFailure(
                AccessibilityService.ERROR_TAKE_SCREENSHOT_INTERVAL_TIME_SHORT);

        Thread.sleep(
                AccessibilityService.ACCESSIBILITY_TAKE_SCREENSHOT_REQUEST_INTERVAL_TIMES_MS / 2 +
                        1);
        // Requests the API again after interval time from calling the first time.
        takeScreenshot(Display.DEFAULT_DISPLAY);
        verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onSuccess(
                mSuccessResultArgumentCaptor.capture());
    }

    @Test
    public void testTakeScreenshotOnVirtualDisplay_GetScreenshotResult() throws Exception {
        try (VirtualDisplaySession displaySession = new VirtualDisplaySession()) {
            final int virtualDisplayId =
                    displaySession.createDisplayWithDefaultDisplayMetricsAndWait(mContext,
                            false).getDisplayId();
            // Launches an activity on virtual display.
            final Activity activity = launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
                    sInstrumentation, sUiAutomation, AccessibilityWindowQueryActivity.class,
                    virtualDisplayId);
            try {
                takeScreenshot(virtualDisplayId);
                verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onSuccess(
                        mSuccessResultArgumentCaptor.capture());

                verifyScreenshotResult(mSuccessResultArgumentCaptor.getValue());
            } finally {
                sInstrumentation.runOnMainSync(() -> {
                    activity.finish();
                });
            }
        }
    }

    @Test
    public void testTakeScreenshotOnPrivateDisplay_GetErrorCode() {
        try (VirtualDisplaySession displaySession = new VirtualDisplaySession()) {
            final int virtualDisplayId =
                    displaySession.createDisplayWithDefaultDisplayMetricsAndWait(mContext,
                            true).getDisplayId();
            takeScreenshot(virtualDisplayId);
            verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onFailure(
                    AccessibilityService.ERROR_TAKE_SCREENSHOT_INVALID_DISPLAY);
        }
    }

    @Test
    public void testTakeScreenshotWithSecureWindow_GetScreenshotAndVerifyBitmap() throws Throwable {
        final Activity activity = launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
                sInstrumentation, sUiAutomation, AccessibilityWindowQueryActivity.class,
                Display.DEFAULT_DISPLAY);

        final ImageView image = new ImageView(activity);
        image.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN);
        image.setImageDrawable(new ColorDrawable(SECURE_WINDOW_CONTENT_COLOR));

        final WindowManager.LayoutParams params = new WindowManager.LayoutParams();
        params.width = WindowManager.LayoutParams.MATCH_PARENT;
        params.height = WindowManager.LayoutParams.MATCH_PARENT;
        params.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        params.flags = WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                | WindowManager.LayoutParams.FLAG_SECURE;

        sUiAutomation.executeAndWaitForEvent(() -> sInstrumentation.runOnMainSync(
                () -> {
                    activity.getWindowManager().addView(image, params);
                }),
                filterWindowsChangedWithChangeTypes(WINDOWS_CHANGE_ADDED),
                DEFAULT_TIMEOUT_MS);
        takeScreenshot(Display.DEFAULT_DISPLAY);

        verify(mCallback, timeout(TIMEOUT_TAKE_SCREENSHOT_DONE_MILLIS)).onSuccess(
                mSuccessResultArgumentCaptor.capture());

        final AccessibilityService.ScreenshotResult newScreenshot =
                mSuccessResultArgumentCaptor.getValue();
        final Bitmap bitmap = Bitmap.wrapHardwareBuffer(newScreenshot.getHardwareBuffer(),
                newScreenshot.getColorSpace());

        assertTrue(doesBitmapDisplaySecureContent(activity, bitmap, SECURE_WINDOW_CONTENT_COLOR));
    }

    private boolean doesBitmapDisplaySecureContent(Activity activity, Bitmap screenshot, int color) {
        final Display display = activity.getWindowManager().getDefaultDisplay();
        final Point displaySize = new Point();
        display.getRealSize(displaySize);

        final int[] pixels = new int[displaySize.x * displaySize.y];
        final Bitmap bitmap = screenshot.copy(Bitmap.Config.ARGB_8888, false);
        bitmap.getPixels(pixels, 0, displaySize.x, 0, 0, displaySize.x,
                displaySize.y);
        for (int pixel : pixels) {
            if ((Color.red(pixel) == Color.red(color))
                    && (Color.green(pixel) == Color.green(color))
                    && (Color.blue(pixel) == Color.blue(color))) {
                return false;
            }
        }

        return true;
    }

    private void takeScreenshot(int displayId) {
        mStartTestingTime = SystemClock.uptimeMillis();
        mService.takeScreenshot(displayId, mContext.getMainExecutor(),
                mCallback);
    }

    private void verifyScreenshotResult(AccessibilityService.ScreenshotResult screenshot) {
        assertNotNull(screenshot);
        final HardwareBuffer hardwareBuffer = screenshot.getHardwareBuffer();
        assertEquals(mDisplaySize.x, hardwareBuffer.getWidth());
        assertEquals(mDisplaySize.y, hardwareBuffer.getHeight());

        // The colorSpace should not be null for taking the screenshot case.
        final ColorSpace colorSpace = screenshot.getColorSpace();
        assertNotNull(colorSpace);

        final long finishTestingTime = screenshot.getTimestamp();
        assertTrue(finishTestingTime > mStartTestingTime);

        // The bitmap should not be null for ScreenshotResult's payload.
        final Bitmap bitmap = Bitmap.wrapHardwareBuffer(hardwareBuffer, colorSpace);
        assertNotNull(bitmap);
    }
}
