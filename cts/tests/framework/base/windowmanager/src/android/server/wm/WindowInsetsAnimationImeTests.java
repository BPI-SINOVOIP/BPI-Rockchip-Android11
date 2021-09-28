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

package android.server.wm;

import static android.graphics.Insets.NONE;
import static android.view.WindowInsets.Type.ime;
import static android.view.WindowInsets.Type.navigationBars;
import static android.view.WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.CALLS_REAL_METHODS;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.withSettings;

import android.app.Instrumentation;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.platform.test.annotations.Presubmit;
import android.view.WindowInsets;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.cts.mockime.ImeSettings;
import com.android.cts.mockime.MockImeSession;

import org.junit.Before;
import org.junit.Test;
import org.mockito.InOrder;

/**
 * Same as {@link WindowInsetsAnimationTests} but IME specific.
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:WindowInsetsAnimationImeTests
 */
@Presubmit
public class WindowInsetsAnimationImeTests extends WindowInsetsAnimationTestBase {

    private static final int KEYBOARD_HEIGHT = 600;

    @Before
    public void setup() throws Exception {
        super.setUp();
        assumeTrue("MockIme cannot be used for devices that do not support installable IMEs",
                mInstrumentation.getContext().getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_INPUT_METHODS));
    }

    private void initActivity(boolean useFloating) throws Exception {
        initMockImeSession(useFloating);

        mActivity = startActivity(TestActivity.class);
        mRootView = mActivity.getWindow().getDecorView();
    }

    private MockImeSession initMockImeSession(boolean useFloating) throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        return MockImeSession.create(
                instrumentation.getContext(), instrumentation.getUiAutomation(),
                useFloating ? getFloatingImeSettings()
                        : new ImeSettings.Builder().setInputViewHeight(KEYBOARD_HEIGHT)
                                .setDrawsBehindNavBar(true));
    }

    @Test
    public void testImeAnimationCallbacksShowAndHide() throws Exception {
        initActivity(false /* useFloating */);
        testShowAndHide();
    }

    @Test
    public void testAnimationCallbacks_overlapping_opposite() throws Exception {
        initActivity(false /* useFloating */);
        assumeTrue(hasWindowInsets(mRootView, navigationBars()));

        WindowInsets before = mActivity.mLastWindowInsets;

        MultiAnimCallback callbackInner = new MultiAnimCallback();
        MultiAnimCallback callback = mock(MultiAnimCallback.class,
                withSettings()
                        .spiedInstance(callbackInner)
                        .defaultAnswer(CALLS_REAL_METHODS)
                        .verboseLogging());
        mActivity.mView.setWindowInsetsAnimationCallback(callback);

        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().hide(navigationBars()));
        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().show(ime()));

        waitForOrFail("Waiting until IME animation starts", () -> callback.imeAnim != null);
        waitForOrFail("Waiting until animation done", () -> callback.runningAnims.isEmpty());

        WindowInsets after = mActivity.mLastWindowInsets;

        // When system bar and IME are animated together, order of events cannot be predicted
        // relative to one another: especially the end since animation durations are different.
        // Use individual inOrder for each.
        InOrder inOrderBar = inOrder(callback, mActivity.mListener);
        InOrder inOrderIme = inOrder(callback, mActivity.mListener);

        inOrderBar.verify(callback).onPrepare(eq(callback.navBarAnim));

        inOrderIme.verify(mActivity.mListener).onApplyWindowInsets(any(), argThat(
                argument -> NONE.equals(argument.getInsets(navigationBars()))
                        && NONE.equals(argument.getInsets(ime()))));

        inOrderBar.verify(callback).onStart(eq(callback.navBarAnim), argThat(
                argument -> argument.getLowerBound().equals(NONE)
                        && argument.getUpperBound().equals(before.getInsets(navigationBars()))));

        inOrderIme.verify(callback).onPrepare(eq(callback.imeAnim));
        inOrderIme.verify(mActivity.mListener).onApplyWindowInsets(
                any(), eq(mActivity.mLastWindowInsets));

        inOrderIme.verify(callback).onStart(eq(callback.imeAnim), argThat(
                argument -> argument.getLowerBound().equals(NONE)
                        && !argument.getUpperBound().equals(NONE)));

        inOrderBar.verify(callback).onEnd(eq(callback.navBarAnim));
        inOrderIme.verify(callback).onEnd(eq(callback.imeAnim));

        assertAnimationSteps(callback.navAnimSteps, false /* showAnimation */);
        assertAnimationSteps(callback.imeAnimSteps, true /* showAnimation */, ime());

        assertEquals(before.getInsets(navigationBars()),
                callback.navAnimSteps.get(0).insets.getInsets(navigationBars()));
        assertEquals(after.getInsets(navigationBars()),
                callback.navAnimSteps.get(callback.navAnimSteps.size() - 1).insets
                        .getInsets(navigationBars()));

        assertEquals(before.getInsets(ime()),
                callback.imeAnimSteps.get(0).insets.getInsets(ime()));
        assertEquals(after.getInsets(ime()),
                callback.imeAnimSteps.get(callback.imeAnimSteps.size() - 1).insets
                        .getInsets(ime()));
    }

    @Test
    public void testZeroInsetsImeAnimates() throws Exception {
        initActivity(true /* useFloating */);
        testShowAndHide();
    }

    private void testShowAndHide() {
        WindowInsets before = mActivity.mLastWindowInsets;
        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().show(ime()));

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);
        commonAnimationAssertions(mActivity, before, true /* show */, ime());
        mActivity.mCallback.animationDone = false;

        before = mActivity.mLastWindowInsets;

        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().hide(ime()));

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);

        commonAnimationAssertions(mActivity, before, false /* show */, ime());
    }

    private static ImeSettings.Builder getFloatingImeSettings() {
        final ImeSettings.Builder builder = new ImeSettings.Builder();
        builder.setWindowFlags(0, FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        // As documented, Window#setNavigationBarColor() is actually ignored when the IME window
        // does not have FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS.  We are calling setNavigationBarColor()
        // to ensure it.
        builder.setNavigationBarColor(Color.BLACK);
        return builder;
    }
}
