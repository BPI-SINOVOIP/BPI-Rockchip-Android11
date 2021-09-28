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
import static android.view.WindowInsets.Type.navigationBars;
import static android.view.WindowInsets.Type.statusBars;
import static android.view.WindowInsets.Type.systemBars;
import static android.view.WindowInsetsAnimation.Callback.DISPATCH_MODE_CONTINUE_ON_SUBTREE;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.CALLS_REAL_METHODS;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.withSettings;

import android.platform.test.annotations.Presubmit;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;
import android.view.WindowInsetsAnimation.Bounds;
import android.view.WindowInsetsAnimation.Callback;

import org.junit.Before;
import org.junit.Test;
import org.mockito.InOrder;

import java.util.List;

/**
 * Test whether {@link WindowInsetsAnimation.Callback} are properly dispatched to views.
 *
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:WindowInsetsAnimationTests
 */
@Presubmit
public class WindowInsetsAnimationTests extends WindowInsetsAnimationTestBase {

    @Before
    public void setup() throws Exception {
        super.setUp();
        mActivity = startActivity(TestActivity.class);
        mRootView = mActivity.getWindow().getDecorView();
        assumeTrue(hasWindowInsets(mRootView, systemBars()));
    }

    @Test
    public void testAnimationCallbacksHide() {
        WindowInsets before = mActivity.mLastWindowInsets;

        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().hide(systemBars()));

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);

        commonAnimationAssertions(mActivity, before, false /* show */, systemBars());
    }

    @Test
    public void testAnimationCallbacksShow() {
        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().hide(systemBars()));

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);
        mActivity.mCallback.animationDone = false;

        WindowInsets before = mActivity.mLastWindowInsets;

        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().show(systemBars()));

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);

        commonAnimationAssertions(mActivity, before, true /* show */, systemBars());
    }

    @Test
    public void testAnimationCallbacks_overlapping() {
        // Test requires navbar to create overlapping animations.
        assumeTrue(hasWindowInsets(mRootView, navigationBars()));

        WindowInsets before = mActivity.mLastWindowInsets;
        MultiAnimCallback callbackInner = new MultiAnimCallback();
        MultiAnimCallback callback = mock(MultiAnimCallback.class,
                withSettings()
                        .spiedInstance(callbackInner)
                        .defaultAnswer(CALLS_REAL_METHODS)
                        .verboseLogging());
        mActivity.mView.setWindowInsetsAnimationCallback(callback);
        callback.startRunnable = () -> mRootView.postDelayed(
                () -> mRootView.getWindowInsetsController().hide(statusBars()), 50);

        getInstrumentation().runOnMainSync(
                () -> mRootView.getWindowInsetsController().hide(navigationBars()));

        waitForOrFail("Waiting until animation done", () -> callback.animationDone);

        WindowInsets after = mActivity.mLastWindowInsets;

        InOrder inOrder = inOrder(callback, mActivity.mListener);

        inOrder.verify(callback).onPrepare(eq(callback.navBarAnim));

        inOrder.verify(mActivity.mListener).onApplyWindowInsets(any(), argThat(
                argument -> NONE.equals(argument.getInsets(navigationBars()))
                        && !NONE.equals(argument.getInsets(statusBars()))));

        inOrder.verify(callback).onStart(eq(callback.navBarAnim), argThat(
                argument -> argument.getLowerBound().equals(NONE)
                        && argument.getUpperBound().equals(before.getInsets(navigationBars()))));

        inOrder.verify(callback).onPrepare(eq(callback.statusBarAnim));
        inOrder.verify(mActivity.mListener).onApplyWindowInsets(
                any(), eq(mActivity.mLastWindowInsets));

        inOrder.verify(callback).onStart(eq(callback.statusBarAnim), argThat(
                argument -> argument.getLowerBound().equals(NONE)
                        && argument.getUpperBound().equals(before.getInsets(statusBars()))));

        inOrder.verify(callback).onEnd(eq(callback.navBarAnim));
        inOrder.verify(callback).onEnd(eq(callback.statusBarAnim));

        assertAnimationSteps(callback.navAnimSteps, false /* showAnimation */);
        assertAnimationSteps(callback.statusAnimSteps, false /* showAnimation */);

        assertEquals(before.getInsets(navigationBars()),
                callback.navAnimSteps.get(0).insets.getInsets(navigationBars()));
        assertEquals(after.getInsets(navigationBars()),
                callback.navAnimSteps.get(callback.navAnimSteps.size() - 1).insets
                        .getInsets(navigationBars()));

        assertEquals(before.getInsets(statusBars()),
                callback.statusAnimSteps.get(0).insets.getInsets(statusBars()));
        assertEquals(after.getInsets(statusBars()),
                callback.statusAnimSteps.get(callback.statusAnimSteps.size() - 1).insets
                        .getInsets(statusBars()));
    }

    @Test
    public void testAnimationCallbacks_consumedByDecor() {
        getInstrumentation().runOnMainSync(() -> {
            mActivity.getWindow().setDecorFitsSystemWindows(true);
            mRootView.getWindowInsetsController().hide(systemBars());
        });

        getWmState().waitFor(state -> !state.isWindowVisible("StatusBar"),
                "Waiting for status bar to be hidden");
        assertFalse(getWmState().isWindowVisible("StatusBar"));

        verifyZeroInteractions(mActivity.mCallback);
    }

    @Test
    public void testAnimationCallbacks_childDoesntGetCallback() {
        WindowInsetsAnimation.Callback childCallback = mock(WindowInsetsAnimation.Callback.class);

        getInstrumentation().runOnMainSync(() -> {
            mActivity.mChild.setWindowInsetsAnimationCallback(childCallback);
            mRootView.getWindowInsetsController().hide(systemBars());
        });

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);

        verifyZeroInteractions(childCallback);
    }

    @Test
    public void testAnimationCallbacks_childInsetting() {
        // test requires navbar.
        assumeTrue(hasWindowInsets(mRootView, navigationBars()));

        WindowInsets before = mActivity.mLastWindowInsets;
        boolean[] done = new boolean[1];
        WindowInsetsAnimation.Callback childCallback = mock(WindowInsetsAnimation.Callback.class);
        WindowInsetsAnimation.Callback callback = new Callback(DISPATCH_MODE_CONTINUE_ON_SUBTREE) {

            @Override
            public Bounds onStart(WindowInsetsAnimation animation, Bounds bounds) {
                return bounds.inset(before.getInsets(navigationBars()));
            }

            @Override
            public WindowInsets onProgress(WindowInsets insets,
                    List<WindowInsetsAnimation> runningAnimations) {
                return insets.inset(insets.getInsets(navigationBars()));
            }

            @Override
            public void onEnd(WindowInsetsAnimation animation) {
                done[0] = true;
            }
        };

        getInstrumentation().runOnMainSync(() -> {
            mActivity.mView.setWindowInsetsAnimationCallback(callback);
            mActivity.mChild.setWindowInsetsAnimationCallback(childCallback);
            mRootView.getWindowInsetsController().hide(systemBars());
        });

        waitForOrFail("Waiting until animation done", () -> done[0]);

        if (hasWindowInsets(mRootView, statusBars())) {
            verify(childCallback).onStart(any(), argThat(
                    bounds -> bounds.getUpperBound().equals(before.getInsets(statusBars()))));
        }
        if (hasWindowInsets(mRootView, navigationBars())) {
            verify(childCallback, atLeastOnce()).onProgress(argThat(
                    insets -> NONE.equals(insets.getInsets(navigationBars()))), any());
        }
    }

    @Test
    public void testAnimationCallbacks_withLegacyFlags() {
        getInstrumentation().runOnMainSync(() -> {
            mActivity.getWindow().setDecorFitsSystemWindows(true);
            mRootView.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
            mRootView.post(() -> {
                mRootView.getWindowInsetsController().hide(systemBars());
            });
        });

        waitForOrFail("Waiting until animation done", () -> mActivity.mCallback.animationDone);

        assertFalse(getWmState().isWindowVisible("StatusBar"));
        verify(mActivity.mCallback).onPrepare(any());
        verify(mActivity.mCallback).onStart(any(), any());
        verify(mActivity.mCallback, atLeastOnce()).onProgress(any(), any());
        verify(mActivity.mCallback).onEnd(any());
    }
}
