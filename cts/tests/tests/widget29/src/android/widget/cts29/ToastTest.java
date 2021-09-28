/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.widget.cts29;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;

import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityManager;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.SystemUtil;
import com.android.compatibility.common.util.TestUtils;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class ToastTest {
    private static final String TEST_TOAST_TEXT = "test toast";
    private static final String TEST_CUSTOM_TOAST_TEXT = "test custom toast";
    private static final String SETTINGS_ACCESSIBILITY_UI_TIMEOUT =
            "accessibility_non_interactive_ui_timeout_ms";
    private static final int ACCESSIBILITY_STATE_WAIT_TIMEOUT_MS = 3000;
    private static final long TIME_FOR_UI_OPERATION  = 1000L;
    private static final long TIME_OUT = 5000L;
    private Toast mToast;
    private Context mContext;
    private boolean mLayoutDone;
    private ViewTreeObserver.OnGlobalLayoutListener mLayoutListener;

    @Rule
    public ActivityTestRule<CtsActivity> mActivityRule =
            new ActivityTestRule<>(CtsActivity.class);

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getTargetContext();
        mLayoutListener = () -> mLayoutDone = true;
    }

    @UiThreadTest
    @Test
    public void testConstructor() {
        new Toast(mContext);
    }

    @UiThreadTest
    @Test(expected=NullPointerException.class)
    public void testConstructorNullContext() {
        new Toast(null);
    }

    private static void assertShowToast(final View view) {
        PollingCheck.waitFor(TIME_OUT, () -> null != view.getParent());
    }

    private static void assertShowAndHide(final View view) {
        assertShowToast(view);
        PollingCheck.waitFor(TIME_OUT, () -> null == view.getParent());
    }

    private static void assertNotShowToast(final View view) {
        // sleep a while and then make sure do not show toast
        SystemClock.sleep(TIME_FOR_UI_OPERATION);
        assertNull(view.getParent());
    }

    private void registerLayoutListener(final View view) {
        mLayoutDone = false;
        view.getViewTreeObserver().addOnGlobalLayoutListener(mLayoutListener);
    }

    private void assertLayoutDone(final View view) {
        PollingCheck.waitFor(TIME_OUT, () -> mLayoutDone);
        view.getViewTreeObserver().removeOnGlobalLayoutListener(mLayoutListener);
    }

    private void makeToast() throws Throwable {
        mActivityRule.runOnUiThread(
                () -> mToast = Toast.makeText(mContext, TEST_TOAST_TEXT, Toast.LENGTH_LONG));
    }

    private void makeCustomToast() throws Throwable {
        mActivityRule.runOnUiThread(
                () -> {
                    mToast = new Toast(mContext);
                    mToast.setDuration(Toast.LENGTH_LONG);
                    TextView view = new TextView(mContext);
                    view.setText(TEST_CUSTOM_TOAST_TEXT);
                    mToast.setView(view);
                }
        );
    }

    @Test
    public void testShow() throws Throwable {
        makeToast();

        final View view = mToast.getView();

        // view has not been attached to screen yet
        assertNull(view.getParent());
        assertEquals(View.VISIBLE, view.getVisibility());

        runOnMainAndDrawSync(view, mToast::show);

        // view will be attached to screen when show it
        assertEquals(View.VISIBLE, view.getVisibility());
        assertShowAndHide(view);
    }

    @UiThreadTest
    @Test(expected=RuntimeException.class)
    public void testShowFailure() {
        Toast toast = new Toast(mContext);
        // do not have any views.
        assertNull(toast.getView());
        toast.show();
    }

    @Test
    public void testCancel() throws Throwable {
        makeToast();

        final View view = mToast.getView();

        // view has not been attached to screen yet
        assertNull(view.getParent());
        mActivityRule.runOnUiThread(() -> {
            mToast.show();
            mToast.cancel();
        });

        assertNotShowToast(view);
    }

    @Test
    public void testAccessView() throws Throwable {
        makeToast();
        assertFalse(mToast.getView() instanceof ImageView);

        final ImageView imageView = new ImageView(mContext);
        Drawable drawable = mContext.getResources().getDrawable(R.drawable.pass);
        imageView.setImageDrawable(drawable);

        runOnMainAndDrawSync(imageView, () -> {
            mToast.setView(imageView);
            mToast.show();
        });
        assertSame(imageView, mToast.getView());
        assertShowAndHide(imageView);
    }

    @Test
    public void testAccessDuration() throws Throwable {
        long start = SystemClock.uptimeMillis();
        makeToast();
        runOnMainAndDrawSync(mToast.getView(), mToast::show);
        assertEquals(Toast.LENGTH_LONG, mToast.getDuration());

        View view = mToast.getView();
        assertShowAndHide(view);
        long longDuration = SystemClock.uptimeMillis() - start;

        start = SystemClock.uptimeMillis();
        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        });
        assertEquals(Toast.LENGTH_SHORT, mToast.getDuration());

        view = mToast.getView();
        assertShowAndHide(view);
        long shortDuration = SystemClock.uptimeMillis() - start;

        assertTrue(longDuration > shortDuration);
    }

    @Test
    public void testAccessDuration_withA11yTimeoutEnabled() throws Throwable {
        makeToast();
        final Runnable showToast = () -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        };
        long start = SystemClock.uptimeMillis();
        runOnMainAndDrawSync(mToast.getView(), showToast);
        assertShowAndHide(mToast.getView());
        final long shortDuration = SystemClock.uptimeMillis() - start;

        final String originalSetting = Settings.Secure.getString(mContext.getContentResolver(),
                SETTINGS_ACCESSIBILITY_UI_TIMEOUT);
        try {
            final int a11ySettingDuration = (int) shortDuration + 1000;
            putSecureSetting(SETTINGS_ACCESSIBILITY_UI_TIMEOUT,
                    Integer.toString(a11ySettingDuration));
            waitForA11yRecommendedTimeoutChanged(mContext,
                    ACCESSIBILITY_STATE_WAIT_TIMEOUT_MS, a11ySettingDuration);
            start = SystemClock.uptimeMillis();
            runOnMainAndDrawSync(mToast.getView(), showToast);
            assertShowAndHide(mToast.getView());
            final long a11yDuration = SystemClock.uptimeMillis() - start;
            assertTrue(a11yDuration >= a11ySettingDuration);
        } finally {
            putSecureSetting(SETTINGS_ACCESSIBILITY_UI_TIMEOUT, originalSetting);
        }
    }

    /**
     * Wait for accessibility recommended timeout changed and equals to expected timeout.
     *
     * @param expectedTimeoutMs expected recommended timeout
     */
    private void waitForA11yRecommendedTimeoutChanged(Context context,
            long waitTimeoutMs, int expectedTimeoutMs) {
        final AccessibilityManager manager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        final Object lock = new Object();
        AccessibilityManager.AccessibilityServicesStateChangeListener listener = (m) -> {
            synchronized (lock) {
                lock.notifyAll();
            }
        };
        manager.addAccessibilityServicesStateChangeListener(listener, null);
        try {
            TestUtils.waitOn(lock,
                    () -> manager.getRecommendedTimeoutMillis(0,
                            AccessibilityManager.FLAG_CONTENT_TEXT) == expectedTimeoutMs,
                    waitTimeoutMs,
                    "Wait for accessibility recommended timeout changed");
        } finally {
            manager.removeAccessibilityServicesStateChangeListener(listener);
        }
    }

    private void putSecureSetting(String name, String value) {
        final StringBuilder cmd = new StringBuilder("settings put secure ")
                .append(name).append(" ")
                .append(value);
        SystemUtil.runShellCommand(cmd.toString());
    }

    @Test
    public void testAccessMargin() throws Throwable {
        assumeFalse("Skipping test: Auto does not support toast with margin", isCar());

        makeToast();
        View view = mToast.getView();
        assertFalse(view.getLayoutParams() instanceof WindowManager.LayoutParams);

        final float horizontal1 = 1.0f;
        final float vertical1 = 1.0f;
        runOnMainAndDrawSync(view, () -> {
            mToast.setMargin(horizontal1, vertical1);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        assertShowToast(view);

        assertEquals(horizontal1, mToast.getHorizontalMargin(), 0.0f);
        assertEquals(vertical1, mToast.getVerticalMargin(), 0.0f);
        WindowManager.LayoutParams params1 = (WindowManager.LayoutParams) view.getLayoutParams();
        assertEquals(horizontal1, params1.horizontalMargin, 0.0f);
        assertEquals(vertical1, params1.verticalMargin, 0.0f);
        assertLayoutDone(view);

        int[] xy1 = new int[2];
        view.getLocationOnScreen(xy1);
        assertShowAndHide(view);

        final float horizontal2 = 0.1f;
        final float vertical2 = 0.1f;
        runOnMainAndDrawSync(view, () -> {
            mToast.setMargin(horizontal2, vertical2);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        assertShowToast(view);

        assertEquals(horizontal2, mToast.getHorizontalMargin(), 0.0f);
        assertEquals(vertical2, mToast.getVerticalMargin(), 0.0f);
        WindowManager.LayoutParams params2 = (WindowManager.LayoutParams) view.getLayoutParams();
        assertEquals(horizontal2, params2.horizontalMargin, 0.0f);
        assertEquals(vertical2, params2.verticalMargin, 0.0f);

        assertLayoutDone(view);
        int[] xy2 = new int[2];
        view.getLocationOnScreen(xy2);
        assertShowAndHide(view);

        /** Check if the test is being run on a watch.
         *
         * Change I8180e5080e0a6860b40dbb2faa791f0ede926ca7 updated how toast are displayed on the
         * watch. Unlike the phone, which displays toast centered horizontally at the bottom of the
         * screen, the watch now displays toast in the center of the screen.
         */
        if (mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_WATCH)) {
            assertTrue(xy1[0] > xy2[0]);
            assertTrue(xy1[1] > xy2[1]);
        } else {
            assertTrue(xy1[0] > xy2[0]);
            assertTrue(xy1[1] < xy2[1]);
        }
    }

    @Test
    public void testAccessGravity() throws Throwable {
        assumeFalse("Skipping test: Auto does not support toast with gravity", isCar());

        makeToast();
        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setGravity(Gravity.CENTER, 0, 0);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        View view = mToast.getView();
        assertShowToast(view);
        assertEquals(Gravity.CENTER, mToast.getGravity());
        assertEquals(0, mToast.getXOffset());
        assertEquals(0, mToast.getYOffset());
        assertLayoutDone(view);
        int[] centerXY = new int[2];
        view.getLocationOnScreen(centerXY);
        assertShowAndHide(view);

        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setGravity(Gravity.BOTTOM, 0, 0);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        view = mToast.getView();
        assertShowToast(view);
        assertEquals(Gravity.BOTTOM, mToast.getGravity());
        assertEquals(0, mToast.getXOffset());
        assertEquals(0, mToast.getYOffset());
        assertLayoutDone(view);
        int[] bottomXY = new int[2];
        view.getLocationOnScreen(bottomXY);
        assertShowAndHide(view);

        // x coordinate is the same
        assertEquals(centerXY[0], bottomXY[0]);
        // bottom view is below of center view
        assertTrue(centerXY[1] < bottomXY[1]);

        final int xOffset = 20;
        final int yOffset = 10;
        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setGravity(Gravity.BOTTOM, xOffset, yOffset);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        view = mToast.getView();
        assertShowToast(view);
        assertEquals(Gravity.BOTTOM, mToast.getGravity());
        assertEquals(xOffset, mToast.getXOffset());
        assertEquals(yOffset, mToast.getYOffset());
        assertLayoutDone(view);
        int[] bottomOffsetXY = new int[2];
        view.getLocationOnScreen(bottomOffsetXY);
        assertShowAndHide(view);

        assertEquals(bottomXY[0] + xOffset, bottomOffsetXY[0]);
        assertEquals(bottomXY[1] - yOffset, bottomOffsetXY[1]);
    }

    @UiThreadTest
    @Test
    public void testMakeTextFromString() {
        Toast toast = Toast.makeText(mContext, "android", Toast.LENGTH_SHORT);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_SHORT, toast.getDuration());
        View view = toast.getView();
        assertNotNull(view);

        toast = Toast.makeText(mContext, "cts", Toast.LENGTH_LONG);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_LONG, toast.getDuration());
        view = toast.getView();
        assertNotNull(view);

        toast = Toast.makeText(mContext, null, Toast.LENGTH_LONG);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_LONG, toast.getDuration());
        view = toast.getView();
        assertNotNull(view);
    }

    @UiThreadTest
    @Test(expected=NullPointerException.class)
    public void testMakeTextFromStringNullContext() {
        Toast.makeText(null, "test", Toast.LENGTH_LONG);
    }

    @UiThreadTest
    @Test
    public void testMakeTextFromResource() {
        Toast toast = Toast.makeText(mContext, R.string.hello_world, Toast.LENGTH_LONG);

        assertNotNull(toast);
        assertEquals(Toast.LENGTH_LONG, toast.getDuration());
        View view = toast.getView();
        assertNotNull(view);

        toast = Toast.makeText(mContext, R.string.hello_android, Toast.LENGTH_SHORT);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_SHORT, toast.getDuration());
        view = toast.getView();
        assertNotNull(view);
    }

    @UiThreadTest
    @Test(expected=NullPointerException.class)
    public void testMakeTextFromResourceNullContext() {
        Toast.makeText(null, R.string.hello_android, Toast.LENGTH_SHORT);
    }

    @UiThreadTest
    @Test
    public void testSetTextFromResource() {
        Toast toast = Toast.makeText(mContext, R.string.text, Toast.LENGTH_LONG);

        toast.setText(R.string.hello_world);
        // TODO: how to getText to assert?

        toast.setText(R.string.hello_android);
        // TODO: how to getText to assert?
    }

    @UiThreadTest
    @Test(expected=RuntimeException.class)
    public void testSetTextFromInvalidResource() {
        Toast toast = Toast.makeText(mContext, R.string.text, Toast.LENGTH_LONG);
        toast.setText(-1);
    }

    @UiThreadTest
    @Test
    public void testSetTextFromString() {
        Toast toast = Toast.makeText(mContext, R.string.text, Toast.LENGTH_LONG);

        toast.setText("cts");
        // TODO: how to getText to assert?

        toast.setText("android");
        // TODO: how to getText to assert?
    }

    @UiThreadTest
    @Test(expected=RuntimeException.class)
    public void testSetTextFromStringNullView() {
        Toast toast = Toast.makeText(mContext, R.string.text, Toast.LENGTH_LONG);
        toast.setView(null);
        toast.setText(null);
    }

    @Test
    public void testTextToastAllowed_whenInTheForeground() throws Throwable {
        makeToast();
        View view = mToast.getView();

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(view);
    }

    @Test
    public void testCustomToastAllowed_whenInTheForeground() throws Throwable {
        makeCustomToast();
        View view = mToast.getView();
        // View has not been attached to screen yet
        assertNull(view.getParent());

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(view);
    }

    @Test
    public void testTextToastAllowed_whenInTheBackground() throws Throwable {
        // Make it background
        mActivityRule.finishActivity();
        makeToast();
        View view = mToast.getView();

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(view);
    }

    @Test
    public void testCustomToastAllowed_whenInTheBackground() throws Throwable {
        // Make it background
        mActivityRule.finishActivity();
        makeCustomToast();
        View view = mToast.getView();
        // View has not been attached to screen yet
        assertNull(view.getParent());

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(view);
    }

    @UiThreadTest
    @Test
    public void testGetWindowParams_whenTextToast_doesNotReturnNull() {
        Toast toast = Toast.makeText(mContext, "Text", Toast.LENGTH_LONG);
        assertNotNull(toast.getWindowParams());
    }

    @UiThreadTest
    @Test
    public void testGetWindowParams_whenCustomToast_doesNotReturnNull() {
        Toast toast = new Toast(mContext);
        toast.setView(new TextView(mContext));
        assertNotNull(toast.getWindowParams());
    }

    private void runOnMainAndDrawSync(@NonNull final View toastView,
            @Nullable final Runnable runner) {
        final CountDownLatch latch = new CountDownLatch(1);

        try {
            mActivityRule.runOnUiThread(() -> {
                final ViewTreeObserver.OnDrawListener listener =
                        new ViewTreeObserver.OnDrawListener() {
                            @Override
                            public void onDraw() {
                                // posting so that the sync happens after the draw that's about
                                // to happen
                                toastView.post(() -> {
                                    toastView.getViewTreeObserver().removeOnDrawListener(this);
                                    latch.countDown();
                                });
                            }
                        };

                toastView.getViewTreeObserver().addOnDrawListener(listener);

                if (runner != null) {
                    runner.run();
                }
                toastView.invalidate();
            });

            Assert.assertTrue("Expected toast draw pass occurred within 5 seconds",
                    latch.await(5, TimeUnit.SECONDS));
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private boolean isCar() {
        PackageManager pm = mContext.getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
    }
}
