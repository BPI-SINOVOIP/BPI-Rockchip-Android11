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

package android.widget.cts;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;

import android.app.UiAutomation;
import android.app.UiAutomation.AccessibilityEventFilter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.ConditionVariable;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;
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

import java.util.concurrent.CompletableFuture;
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
    private static final String ACTION_TRANSLUCENT_ACTIVITY_RESUMED =
            "android.widget.cts.app.TRANSLUCENT_ACTIVITY_RESUMED";
    private static final String ACTION_TRANSLUCENT_ACTIVITY_FINISH =
            "android.widget.cts.app.TRANSLUCENT_ACTIVITY_FINISH";
    private static final ComponentName COMPONENT_TRANSLUCENT_ACTIVITY =
            ComponentName.unflattenFromString("android.widget.cts.app/.TranslucentActivity");

    private Toast mToast;
    private Context mContext;
    private boolean mLayoutDone;
    private ViewTreeObserver.OnGlobalLayoutListener mLayoutListener;
    private ConditionVariable mToastShown;
    private ConditionVariable mToastHidden;

    @Rule
    public ActivityTestRule<CtsActivity> mActivityRule =
            new ActivityTestRule<>(CtsActivity.class);
    private UiAutomation mUiAutomation;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getTargetContext();
        mUiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
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

    private static void assertShowCustomToast(final View view) {
        PollingCheck.waitFor(TIME_OUT, () -> null != view.getParent());
    }

    private void assertShowToast(Toast toast) {
        assertTrue(mToastShown.block(TIME_OUT));
    }

    private static void assertShowAndHideCustomToast(final View view) {
        assertShowCustomToast(view);
        PollingCheck.waitFor(TIME_OUT, () -> null == view.getParent());
    }

    private void assertShowAndHide(Toast toast) {
        assertTrue(mToastShown.block(TIME_OUT));
        assertTrue(mToastHidden.block(TIME_OUT));
    }

    private static void assertNotShowCustomToast(final View view) {
        // sleep a while and then make sure do not show toast
        SystemClock.sleep(TIME_FOR_UI_OPERATION);
        assertNull(view.getParent());
    }

    private void assertNotShowToast(Toast toast) {
        assertFalse(mToastShown.block(TIME_FOR_UI_OPERATION));
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
        mToastShown = new ConditionVariable(false);
        mToastHidden = new ConditionVariable(false);
        mActivityRule.runOnUiThread(
                () -> {
                    mToast = Toast.makeText(mContext, TEST_TOAST_TEXT, Toast.LENGTH_LONG);
                    mToast.addCallback(new ConditionCallback(mToastShown, mToastHidden));
                });
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
    public void testShow_whenCustomToast() throws Throwable {
        makeCustomToast();

        final View view = mToast.getView();

        // view has not been attached to screen yet
        assertNull(view.getParent());
        assertEquals(View.VISIBLE, view.getVisibility());

        runOnMainAndDrawSync(view, mToast::show);

        // view will be attached to screen when show it
        assertEquals(View.VISIBLE, view.getVisibility());
        assertShowAndHideCustomToast(view);
    }

    @Test
    public void testShow_whenTextToast() throws Throwable {
        makeToast();

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(mToast);
    }

    @UiThreadTest
    @Test(expected=RuntimeException.class)
    public void testShowFailure() {
        Toast toast = new Toast(mContext);
        // do not have any views or text
        assertNull(toast.getView());
        toast.show();
    }

    @Test
    public void testCancel_whenCustomToast() throws Throwable {
        makeCustomToast();

        final View view = mToast.getView();

        // view has not been attached to screen yet
        assertNull(view.getParent());
        mActivityRule.runOnUiThread(() -> {
            mToast.show();
            mToast.cancel();
        });

        assertNotShowCustomToast(view);
    }

    @Test
    public void testAccessView_whenCustomToast() throws Throwable {
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
        assertShowAndHideCustomToast(imageView);
    }

    @Test
    public void testAccessDuration_whenCustomToast() throws Throwable {
        long start = SystemClock.uptimeMillis();
        makeCustomToast();
        runOnMainAndDrawSync(mToast.getView(), mToast::show);
        assertEquals(Toast.LENGTH_LONG, mToast.getDuration());

        View view = mToast.getView();
        assertShowAndHideCustomToast(view);
        long longDuration = SystemClock.uptimeMillis() - start;

        start = SystemClock.uptimeMillis();
        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        });
        assertEquals(Toast.LENGTH_SHORT, mToast.getDuration());

        view = mToast.getView();
        assertShowAndHideCustomToast(view);
        long shortDuration = SystemClock.uptimeMillis() - start;

        assertTrue(longDuration > shortDuration);
    }

    @Test
    public void testAccessDuration_whenTextToast() throws Throwable {
        long start = SystemClock.uptimeMillis();
        makeToast();
        mActivityRule.runOnUiThread(mToast::show);
        assertEquals(Toast.LENGTH_LONG, mToast.getDuration());

        assertShowAndHide(mToast);
        long longDuration = SystemClock.uptimeMillis() - start;

        start = SystemClock.uptimeMillis();
        makeToast();
        mActivityRule.runOnUiThread(() -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        });
        assertEquals(Toast.LENGTH_SHORT, mToast.getDuration());

        assertShowAndHide(mToast);
        long shortDuration = SystemClock.uptimeMillis() - start;

        assertTrue(longDuration > shortDuration);
    }

    @Test
    public void testAccessDuration_whenCustomToastAndWithA11yTimeoutEnabled() throws Throwable {
        makeCustomToast();
        final Runnable showToast = () -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        };
        long start = SystemClock.uptimeMillis();
        runOnMainAndDrawSync(mToast.getView(), showToast);
        assertShowAndHideCustomToast(mToast.getView());
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
            assertShowAndHideCustomToast(mToast.getView());
            final long a11yDuration = SystemClock.uptimeMillis() - start;
            assertTrue("Toast duration " + a11yDuration + "ms < A11y setting " + a11ySettingDuration
                    + "ms", a11yDuration >= a11ySettingDuration);
        } finally {
            putSecureSetting(SETTINGS_ACCESSIBILITY_UI_TIMEOUT, originalSetting);
        }
    }

    @Test
    public void testAccessDuration_whenTextToastAndWithA11yTimeoutEnabled() throws Throwable {
        makeToast();
        final Runnable showToast = () -> {
            mToast.setDuration(Toast.LENGTH_SHORT);
            mToast.show();
        };
        long start = SystemClock.uptimeMillis();
        mActivityRule.runOnUiThread(showToast);
        assertShowAndHide(mToast);
        final long shortDuration = SystemClock.uptimeMillis() - start;

        final String originalSetting = Settings.Secure.getString(mContext.getContentResolver(),
                SETTINGS_ACCESSIBILITY_UI_TIMEOUT);
        try {
            final int a11ySettingDuration = (int) shortDuration + 1000;
            putSecureSetting(SETTINGS_ACCESSIBILITY_UI_TIMEOUT,
                    Integer.toString(a11ySettingDuration));
            waitForA11yRecommendedTimeoutChanged(mContext,
                    ACCESSIBILITY_STATE_WAIT_TIMEOUT_MS, a11ySettingDuration);
            makeToast();
            start = SystemClock.uptimeMillis();
            mActivityRule.runOnUiThread(showToast);
            assertShowAndHide(mToast);
            final long a11yDuration = SystemClock.uptimeMillis() - start;
            assertTrue("Toast duration " + a11yDuration + "ms < A11y setting " + a11ySettingDuration
                    + "ms", a11yDuration >= a11ySettingDuration);
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
    public void testAccessMargin_whenCustomToast() throws Throwable {
        assumeFalse("Skipping test: Auto does not support toast with margin", isCar());

        makeCustomToast();
        View view = mToast.getView();
        assertFalse(view.getLayoutParams() instanceof WindowManager.LayoutParams);

        final float horizontal1 = 1.0f;
        final float vertical1 = 1.0f;
        runOnMainAndDrawSync(view, () -> {
            mToast.setMargin(horizontal1, vertical1);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        assertShowCustomToast(view);

        assertEquals(horizontal1, mToast.getHorizontalMargin(), 0.0f);
        assertEquals(vertical1, mToast.getVerticalMargin(), 0.0f);
        WindowManager.LayoutParams params1 = (WindowManager.LayoutParams) view.getLayoutParams();
        assertEquals(horizontal1, params1.horizontalMargin, 0.0f);
        assertEquals(vertical1, params1.verticalMargin, 0.0f);
        assertLayoutDone(view);

        int[] xy1 = new int[2];
        view.getLocationOnScreen(xy1);
        assertShowAndHideCustomToast(view);

        final float horizontal2 = 0.1f;
        final float vertical2 = 0.1f;
        runOnMainAndDrawSync(view, () -> {
            mToast.setMargin(horizontal2, vertical2);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        assertShowCustomToast(view);

        assertEquals(horizontal2, mToast.getHorizontalMargin(), 0.0f);
        assertEquals(vertical2, mToast.getVerticalMargin(), 0.0f);
        WindowManager.LayoutParams params2 = (WindowManager.LayoutParams) view.getLayoutParams();
        assertEquals(horizontal2, params2.horizontalMargin, 0.0f);
        assertEquals(vertical2, params2.verticalMargin, 0.0f);

        assertLayoutDone(view);
        int[] xy2 = new int[2];
        view.getLocationOnScreen(xy2);
        assertShowAndHideCustomToast(view);

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
    public void testAccessGravity_whenCustomToast() throws Throwable {
        assumeFalse("Skipping test: Auto does not support toast with gravity", isCar());

        makeCustomToast();
        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setGravity(Gravity.CENTER, 0, 0);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        View view = mToast.getView();
        assertShowCustomToast(view);
        assertEquals(Gravity.CENTER, mToast.getGravity());
        assertEquals(0, mToast.getXOffset());
        assertEquals(0, mToast.getYOffset());
        assertLayoutDone(view);
        int[] centerXY = new int[2];
        view.getLocationOnScreen(centerXY);
        assertShowAndHideCustomToast(view);

        runOnMainAndDrawSync(mToast.getView(), () -> {
            mToast.setGravity(Gravity.BOTTOM, 0, 0);
            mToast.show();
            registerLayoutListener(mToast.getView());
        });
        view = mToast.getView();
        assertShowCustomToast(view);
        assertEquals(Gravity.BOTTOM, mToast.getGravity());
        assertEquals(0, mToast.getXOffset());
        assertEquals(0, mToast.getYOffset());
        assertLayoutDone(view);
        int[] bottomXY = new int[2];
        view.getLocationOnScreen(bottomXY);
        assertShowAndHideCustomToast(view);

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
        assertShowCustomToast(view);
        assertEquals(Gravity.BOTTOM, mToast.getGravity());
        assertEquals(xOffset, mToast.getXOffset());
        assertEquals(yOffset, mToast.getYOffset());
        assertLayoutDone(view);
        int[] bottomOffsetXY = new int[2];
        view.getLocationOnScreen(bottomOffsetXY);
        assertShowAndHideCustomToast(view);

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
        assertNull(view);

        toast = Toast.makeText(mContext, "cts", Toast.LENGTH_LONG);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_LONG, toast.getDuration());
        view = toast.getView();
        assertNull(view);

        toast = Toast.makeText(mContext, null, Toast.LENGTH_LONG);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_LONG, toast.getDuration());
        view = toast.getView();
        assertNull(view);
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
        assertNull(view);

        toast = Toast.makeText(mContext, R.string.hello_android, Toast.LENGTH_SHORT);
        assertNotNull(toast);
        assertEquals(Toast.LENGTH_SHORT, toast.getDuration());
        view = toast.getView();
        assertNull(view);
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
    @Test(expected = IllegalStateException.class)
    public void testSetTextFromStringNonNullView() {
        Toast toast = Toast.makeText(mContext, R.string.text, Toast.LENGTH_LONG);
        toast.setView(new TextView(mContext));
        toast.setText(null);
    }

    @Test
    public void testRemovedCallbackIsNotCalled() throws Throwable {
        CompletableFuture<Void> toastShown = new CompletableFuture<>();
        CompletableFuture<Void> toastHidden = new CompletableFuture<>();
        Toast.Callback testCallback = new Toast.Callback() {
            @Override
            public void onToastShown() {
                toastShown.complete(null);
            }
            @Override
            public void onToastHidden() {
                toastHidden.complete(null);
            }
        };
        mToastShown = new ConditionVariable(false);
        mToastHidden = new ConditionVariable(false);
        mActivityRule.runOnUiThread(
                () -> {
                    mToast = Toast.makeText(mContext, TEST_TOAST_TEXT, Toast.LENGTH_LONG);
                    mToast.addCallback(testCallback);
                    mToast.addCallback(new ConditionCallback(mToastShown, mToastHidden));
                    mToast.removeCallback(testCallback);
                });

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(mToast);
        assertFalse(toastShown.isDone());
        assertFalse(toastHidden.isDone());
    }

    @Test(expected = NullPointerException.class)
    public void testAddCallback_whenNull_throws() throws Throwable {
        makeToast();
        mToast.addCallback(null);
    }

    @Test
    public void testCallback_whenTextToast_isCalled() throws Throwable {
        ConditionVariable toastShown = new ConditionVariable(false);
        ConditionVariable toastHidden = new ConditionVariable(false);
        mActivityRule.runOnUiThread(
                () -> {
                    mToast = Toast.makeText(mContext, TEST_TOAST_TEXT, Toast.LENGTH_LONG);
                    mToast.addCallback(new ConditionCallback(toastShown, toastHidden));
                });

        mActivityRule.runOnUiThread(mToast::show);

        assertTrue(toastShown.block(TIME_OUT));
        assertTrue(toastHidden.block(TIME_OUT));
    }

    @Test
    public void testCallback_whenCustomToast_isCalled() throws Throwable {
        makeCustomToast();
        ConditionVariable toastShown = new ConditionVariable(false);
        ConditionVariable toastHidden = new ConditionVariable(false);
        mActivityRule.runOnUiThread(
                () -> mToast.addCallback(new ConditionCallback(toastShown, toastHidden)));

        mActivityRule.runOnUiThread(mToast::show);

        assertTrue(toastShown.block(TIME_OUT));
        assertTrue(toastHidden.block(TIME_OUT));
    }

    @Test
    public void testTextToastAllowed_whenInTheForeground() throws Throwable {
        makeToast();

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(mToast);
    }

    @Test
    public void testCustomToastAllowed_whenInTheForeground() throws Throwable {
        makeCustomToast();
        View view = mToast.getView();
        // View has not been attached to screen yet
        assertNull(view.getParent());

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHideCustomToast(view);
    }

    @Test
    public void testTextToastAllowed_whenInTheBackground() throws Throwable {
        // Make it background
        mActivityRule.finishActivity();
        makeToast();

        mActivityRule.runOnUiThread(mToast::show);

        assertShowAndHide(mToast);
    }

    @Test
    public void testCustomToastBlocked_whenInTheBackground() throws Throwable {
        // Make it background
        mActivityRule.finishActivity();
        makeCustomToast();
        View view = mToast.getView();
        // View has not been attached to screen yet
        assertNull(view.getParent());

        mActivityRule.runOnUiThread(mToast::show);

        assertNotShowCustomToast(view);
    }

    @Test
    public void testCustomToastBlocked_whenBehindTranslucentActivity() throws Throwable {
        ConditionVariable activityStarted = registerBlockingReceiver(
                ACTION_TRANSLUCENT_ACTIVITY_RESUMED);
        Intent intent = new Intent();
        intent.setComponent(COMPONENT_TRANSLUCENT_ACTIVITY);
        intent.setFlags(FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        activityStarted.block();
        makeCustomToast();
        View view = mToast.getView();

        mActivityRule.runOnUiThread(mToast::show);

        // The custom toast should not be blocked in multi-window mode. Otherwise, it should be.
        if (mActivityRule.getActivity().isInMultiWindowMode()) {
            assertShowCustomToast(view);
        } else {
            assertNotShowCustomToast(view);
        }
        mContext.sendBroadcast(new Intent(ACTION_TRANSLUCENT_ACTIVITY_FINISH));
    }

    @UiThreadTest
    @Test
    public void testGetWindowParams_whenTextToast_returnsNull() {
        Toast toast = Toast.makeText(mContext, "Text", Toast.LENGTH_LONG);
        assertNull(toast.getWindowParams());
    }

    @UiThreadTest
    @Test
    public void testGetWindowParams_whenCustomToast_doesNotReturnNull() {
        Toast toast = new Toast(mContext);
        toast.setView(new TextView(mContext));
        assertNotNull(toast.getWindowParams());
    }

    @Test
    public void testShow_whenTextToast_sendsAccessibilityEvent() throws Throwable {
        makeToast();
        AccessibilityEventFilter filter =
                event -> event.getEventType() == AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED;

        AccessibilityEvent event = mUiAutomation.executeAndWaitForEvent(
                () -> uncheck(() -> mActivityRule.runOnUiThread(mToast::show)), filter, TIME_OUT);

        assertThat(event.getEventType()).isEqualTo(
                AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED);
        assertThat(event.getClassName()).isEqualTo(Toast.class.getCanonicalName());
        assertThat(event.getPackageName()).isEqualTo(mContext.getPackageName());
        assertThat(event.getText()).contains(TEST_TOAST_TEXT);
    }

    @Test
    public void testShow_whenCustomToast_sendsAccessibilityEvent() throws Throwable {
        makeCustomToast();
        AccessibilityEventFilter filter =
                event -> event.getEventType() == AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED;

        AccessibilityEvent event = mUiAutomation.executeAndWaitForEvent(
                () -> uncheck(() -> mActivityRule.runOnUiThread(mToast::show)), filter, TIME_OUT);

        assertThat(event.getEventType()).isEqualTo(
                AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED);
        assertThat(event.getClassName()).isEqualTo(Toast.class.getCanonicalName());
        assertThat(event.getPackageName()).isEqualTo(mContext.getPackageName());
        assertThat(event.getText()).contains(TEST_CUSTOM_TOAST_TEXT);
    }

    private ConditionVariable registerBlockingReceiver(String action) {
        ConditionVariable broadcastReceived = new ConditionVariable(false);
        IntentFilter filter = new IntentFilter(action);
        mContext.registerReceiver(new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                broadcastReceived.open();
            }
        }, filter);
        return broadcastReceived;
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

    private static void uncheck(ThrowingRunnable runnable) {
        try {
            runnable.run();
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    private interface ThrowingRunnable {
        void run() throws Throwable;
    }

    private static class ConditionCallback extends Toast.Callback {
        private final ConditionVariable mToastShown;
        private final ConditionVariable mToastHidden;

        ConditionCallback(ConditionVariable toastShown, ConditionVariable toastHidden) {
            mToastShown = toastShown;
            mToastHidden = toastHidden;
        }

        @Override
        public void onToastShown() {
            mToastShown.open();
        }

        @Override
        public void onToastHidden() {
            mToastHidden.open();
        }
    }
}
