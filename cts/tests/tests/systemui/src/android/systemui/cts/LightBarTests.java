/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package android.systemui.cts;

import static android.server.wm.BarTestUtils.assumeHasColoredNavigationBar;
import static android.server.wm.BarTestUtils.assumeHasColoredStatusBar;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertTrue;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.UiAutomation;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

/**
 * Test for light status bar.
 *
 * atest CtsSystemUiTestCases:LightBarTests
 */
@RunWith(AndroidJUnit4.class)
public class LightBarTests extends LightBarTestBase {

    public static final String TAG = "LightStatusBarTests";

    /**
     * Color may be slightly off-spec when resources are resized for lower densities. Use this error
     * margin to accommodate for that when comparing colors.
     */
    private static final int COLOR_COMPONENT_ERROR_MARGIN = 20;

    /**
     * It's possible for the device to have color sampling enabled in the nav bar -- in that
     * case we need to pick a background color that would result in the same dark icon tint
     * that matches the default visibility flags used when color sampling is not enabled.
     */
    private static final int LIGHT_BG_COLOR = Color.rgb(255, 128, 128);

    private final String NOTIFICATION_TAG = "TEST_TAG";
    private final String NOTIFICATION_CHANNEL_ID = "test_channel";
    private final String NOTIFICATION_GROUP_KEY = "test_group";
    private NotificationManager mNm;

    @Rule
    public ActivityTestRule<LightBarActivity> mActivityRule = new ActivityTestRule<>(
            LightBarActivity.class);
    @Rule
    public TestName mTestName = new TestName();

    @Test
    @AppModeFull // Instant apps cannot create notifications
    public void testLightStatusBarIcons() throws Throwable {
        assumeHasColoredStatusBar(mActivityRule);

        mNm = (NotificationManager) getInstrumentation().getContext()
                .getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel channel1 = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                NOTIFICATION_CHANNEL_ID, NotificationManager.IMPORTANCE_LOW);
        mNm.createNotificationChannel(channel1);

        // post 10 notifications to ensure enough icons in the status bar
        for (int i = 0; i < 10; i++) {
            Notification.Builder noti1 = new Notification.Builder(getInstrumentation().getContext(),
                    NOTIFICATION_CHANNEL_ID)
                    .setSmallIcon(R.drawable.ic_save)
                    .setChannelId(NOTIFICATION_CHANNEL_ID)
                    .setPriority(Notification.PRIORITY_LOW)
                    .setGroup(NOTIFICATION_GROUP_KEY);
            mNm.notify(NOTIFICATION_TAG, i, noti1.build());
        }

        requestLightBars(LIGHT_BG_COLOR);
        Thread.sleep(WAIT_TIME);

        Bitmap bitmap = takeStatusBarScreenshot(mActivityRule.getActivity());
        Stats s = evaluateLightBarBitmap(bitmap, LIGHT_BG_COLOR, 0);
        assertLightStats(bitmap, s);

        mNm.cancelAll();
        mNm.deleteNotificationChannel(NOTIFICATION_CHANNEL_ID);
    }

    @Test
    public void testLightNavigationBar() throws Throwable {
        assumeHasColoredNavigationBar(mActivityRule);

        requestLightBars(LIGHT_BG_COLOR);
        Thread.sleep(WAIT_TIME);

        // Inject a cancelled interaction with the nav bar to ensure it is at full opacity.
        int x = mActivityRule.getActivity().getWidth() / 2;
        int y = mActivityRule.getActivity().getBottom() + 10;
        injectCanceledTap(x, y);
        Thread.sleep(WAIT_TIME);

        LightBarActivity activity = mActivityRule.getActivity();
        Bitmap bitmap = takeNavigationBarScreenshot(activity);
        Stats s = evaluateLightBarBitmap(bitmap, LIGHT_BG_COLOR, activity.getBottom());
        assertLightStats(bitmap, s);
    }

    @Test
    public void testNavigationBarDivider() throws Throwable {
        assumeHasColoredNavigationBar(mActivityRule);

        mActivityRule.runOnUiThread(() -> {
            mActivityRule.getActivity().getWindow().setNavigationBarColor(Color.RED);
            mActivityRule.getActivity().getWindow().setNavigationBarDividerColor(Color.WHITE);
        });
        Thread.sleep(WAIT_TIME);

        checkNavigationBarDivider(mActivityRule.getActivity(), Color.WHITE, Color.RED,
                mTestName.getMethodName());
    }

    private void injectCanceledTap(int x, int y) {
        long downTime = SystemClock.uptimeMillis();
        injectEvent(MotionEvent.ACTION_DOWN, x, y, downTime);
        injectEvent(MotionEvent.ACTION_CANCEL, x, y, downTime);
    }

    private void injectEvent(int action, int x, int y, long downTime) {
        final UiAutomation automation = getInstrumentation().getUiAutomation();
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        assertTrue(automation.injectInputEvent(event, true));
        event.recycle();
    }

    private void assertLightStats(Bitmap bitmap, Stats s) {
        boolean success = false;
        try {
            assumeNavigationBarChangesColor(s.backgroundPixels, s.totalPixels());

            assertMoreThan("Not enough pixels colored as in the spec", 0.3f,
                    (float) s.iconPixels / (float) s.foregroundPixels(),
                    "Are the bar icons colored according to the spec "
                            + "(60% black and 24% black)?");

            assertLessThan("Too many lighter pixels lighter than the background", 0.05f,
                    (float) s.sameHueLightPixels / (float) s.foregroundPixels(),
                    "Are the bar icons dark?");

            assertLessThan("Too many pixels with a changed hue", 0.05f,
                    (float) s.unexpectedHuePixels / (float) s.foregroundPixels(),
                    "Are the bar icons color-free?");

            success = true;
        } finally {
            if (!success) {
                dumpBitmap(bitmap, mTestName.getMethodName());
            }
        }
    }

    private void requestLightBars(final int background) throws Throwable {
        final LightBarActivity activity = mActivityRule.getActivity();
        activity.runOnUiThread(() -> {
            activity.getWindow().setStatusBarColor(background);
            activity.getWindow().setNavigationBarColor(background);
            activity.setLightStatusBar(true);
            activity.setLightNavigationBar(true);
        });
    }

    private static class Stats {
        int backgroundPixels;
        int iconPixels;
        int sameHueDarkPixels;
        int sameHueLightPixels;
        int unexpectedHuePixels;

        int totalPixels() {
            return backgroundPixels + iconPixels + sameHueDarkPixels
                    + sameHueLightPixels + unexpectedHuePixels;
        }

        int foregroundPixels() {
            return iconPixels + sameHueDarkPixels
                    + sameHueLightPixels + unexpectedHuePixels;
        }

        @Override
        public String toString() {
            return String.format("{bg=%d, ic=%d, dark=%d, light=%d, bad=%d}",
                    backgroundPixels, iconPixels, sameHueDarkPixels, sameHueLightPixels,
                    unexpectedHuePixels);
        }
    }

    private Stats evaluateLightBarBitmap(Bitmap bitmap, int background, int shiftY) {
        int iconColor = 0x99000000;
        int iconPartialColor = 0x3d000000;

        int mixedIconColor = mixSrcOver(background, iconColor);
        int mixedIconPartialColor = mixSrcOver(background, iconPartialColor);
        float [] hsvMixedIconColor = new float[3];
        float [] hsvMixedPartialColor = new float[3];
        Color.RGBToHSV(Color.red(mixedIconColor), Color.green(mixedIconColor),
                Color.blue(mixedIconColor), hsvMixedIconColor);
        Color.RGBToHSV(Color.red(mixedIconPartialColor), Color.green(mixedIconPartialColor),
                Color.blue(mixedIconPartialColor), hsvMixedPartialColor);

        float maxHsvValue = Math.max(hsvMixedIconColor[2], hsvMixedPartialColor[2]);
        float minHsvValue = Math.min(hsvMixedIconColor[2], hsvMixedPartialColor[2]);

        int[] pixels = new int[bitmap.getHeight() * bitmap.getWidth()];
        bitmap.getPixels(pixels, 0, bitmap.getWidth(), 0, 0, bitmap.getWidth(), bitmap.getHeight());

        Stats s = new Stats();
        float eps = 0.005f;

        loadCutout(mActivityRule.getActivity());
        float [] hsvPixel = new float[3];
        int i = 0;
        for (int c : pixels) {
            int x = i % bitmap.getWidth();
            int y = i / bitmap.getWidth();
            i++;
            if (isInsideCutout(x, shiftY + y)) {
                continue;
            }

            if (isColorSame(c, background)) {
                s.backgroundPixels++;
                continue;
            }

            // What we expect the icons to be colored according to the spec.
            Color.RGBToHSV(Color.red(c), Color.green(c), Color.blue(c), hsvPixel);
            if (isColorSame(c, mixedIconColor) || isColorSame(c, mixedIconPartialColor)
                    || (hsvPixel[2] >= minHsvValue && hsvPixel[2] <= maxHsvValue)) {
                s.iconPixels++;
                continue;
            }

            // Due to anti-aliasing, there will be deviations from the ideal icon color, but it
            // should still be mostly the same hue.
            float hueDiff = Math.abs(ColorUtils.hue(background) - ColorUtils.hue(c));
            if (hueDiff < eps || hueDiff > 1 - eps) {
                // .. it shouldn't be lighter than the original background though.
                if (ColorUtils.brightness(c) > ColorUtils.brightness(background)) {
                    s.sameHueLightPixels++;
                } else {
                    s.sameHueDarkPixels++;
                }
                continue;
            }

            s.unexpectedHuePixels++;
        }

        return s;
    }

    private int mixSrcOver(int background, int foreground) {
        int bgAlpha = Color.alpha(background);
        int bgRed = Color.red(background);
        int bgGreen = Color.green(background);
        int bgBlue = Color.blue(background);

        int fgAlpha = Color.alpha(foreground);
        int fgRed = Color.red(foreground);
        int fgGreen = Color.green(foreground);
        int fgBlue = Color.blue(foreground);

        return Color.argb(fgAlpha + (255 - fgAlpha) * bgAlpha / 255,
                    fgRed + (255 - fgAlpha) * bgRed / 255,
                    fgGreen + (255 - fgAlpha) * bgGreen / 255,
                    fgBlue + (255 - fgAlpha) * bgBlue / 255);
    }

    /**
     * Check if two colors' diff is in the error margin as defined in
     * {@link #COLOR_COMPONENT_ERROR_MARGIN}.
     */
    private boolean isColorSame(int c1, int c2){
        return Math.abs(Color.alpha(c1) - Color.alpha(c2)) < COLOR_COMPONENT_ERROR_MARGIN
                && Math.abs(Color.red(c1) - Color.red(c2)) < COLOR_COMPONENT_ERROR_MARGIN
                && Math.abs(Color.green(c1) - Color.green(c2)) < COLOR_COMPONENT_ERROR_MARGIN
                && Math.abs(Color.blue(c1) - Color.blue(c2)) < COLOR_COMPONENT_ERROR_MARGIN;
    }
}
