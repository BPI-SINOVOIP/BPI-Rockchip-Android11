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
import static android.view.WindowInsets.Type.statusBars;
import static android.view.WindowInsets.Type.systemBars;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;

import android.graphics.Insets;
import android.os.Bundle;
import android.os.SystemClock;
import android.server.wm.WindowInsetsAnimationTestBase.AnimCallback.AnimationStep;
import android.util.ArraySet;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.mockito.InOrder;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiPredicate;
import java.util.function.Function;
import java.util.function.Predicate;

/**
 * Base class for tests in {@link WindowInsetsAnimation} and {@link WindowInsetsAnimation.Callback}.
 */
public class WindowInsetsAnimationTestBase extends WindowManagerTestBase {

    protected TestActivity mActivity;
    protected View mRootView;

    protected void commonAnimationAssertions(TestActivity activity, WindowInsets before,
            boolean show, int types) {

        AnimCallback callback = activity.mCallback;

        InOrder inOrder = inOrder(activity.mCallback, activity.mListener);

        WindowInsets after = activity.mLastWindowInsets;
        inOrder.verify(callback).onPrepare(eq(callback.lastAnimation));
        inOrder.verify(activity.mListener).onApplyWindowInsets(any(), any());

        inOrder.verify(callback).onStart(eq(callback.lastAnimation), argThat(
                argument -> argument.getLowerBound().equals(NONE)
                        && argument.getUpperBound().equals(show
                        ? after.getInsets(types)
                        : before.getInsets(types))));

        inOrder.verify(callback, atLeast(2)).onProgress(any(), argThat(
                argument -> argument.size() == 1 && argument.get(0) == callback.lastAnimation));
        inOrder.verify(callback).onEnd(eq(callback.lastAnimation));

        if ((types & systemBars()) != 0) {
            assertTrue((callback.lastAnimation.getTypeMask() & systemBars()) != 0);
        }
        if ((types & ime()) != 0) {
            assertTrue((callback.lastAnimation.getTypeMask() & ime()) != 0);
        }
        assertTrue(callback.lastAnimation.getDurationMillis() > 0);
        assertNotNull(callback.lastAnimation.getInterpolator());
        assertBeforeAfterState(callback.animationSteps, before, after);
        assertAnimationSteps(callback.animationSteps, show /* increasing */);
    }

    private void assertBeforeAfterState(ArrayList<AnimationStep> steps, WindowInsets before,
            WindowInsets after) {
        assertEquals(before, steps.get(0).insets);
        assertEquals(after, steps.get(steps.size() - 1).insets);
    }

    protected static boolean hasWindowInsets(View rootView, int types) {
        return Insets.NONE != rootView.getRootWindowInsets().getInsetsIgnoringVisibility(types);
    }

    protected void assertAnimationSteps(ArrayList<AnimationStep> steps, boolean showAnimation) {
        assertAnimationSteps(steps, showAnimation, systemBars());
    }
    protected void assertAnimationSteps(ArrayList<AnimationStep> steps, boolean showAnimation,
            final int types) {
        assertTrue(steps.size() >= 2);
        assertEquals(0f, steps.get(0).fraction, 0f);
        assertEquals(0f, steps.get(0).interpolatedFraction, 0f);
        assertEquals(1f, steps.get(steps.size() - 1).fraction, 0f);
        assertEquals(1f, steps.get(steps.size() - 1).interpolatedFraction, 0f);
        if (showAnimation) {
            assertEquals(1f, steps.get(steps.size() - 1).alpha, 0f);
        } else {
            assertEquals(1f, steps.get(0).alpha, 0f);
        }

        assertListElements(steps, step -> step.fraction,
                (current, next) -> next >= current);
        assertListElements(steps, step -> step.interpolatedFraction,
                (current, next) -> next >= current);
        assertListElements(steps, step -> step.alpha, alpha -> alpha >= 0f);
        assertListElements(steps, step -> step.insets, compareInsets(types, showAnimation));
    }

    private BiPredicate<WindowInsets, WindowInsets> compareInsets(int types,
            boolean showAnimation) {
        if (showAnimation) {
            return (current, next) ->
                    next.getInsets(types).left >= current.getInsets(types).left
                            && next.getInsets(types).top >= current.getInsets(types).top
                            && next.getInsets(types).right >= current.getInsets(types).right
                            && next.getInsets(types).bottom >= current.getInsets(types).bottom;
        } else {
            return (current, next) ->
                    next.getInsets(types).left <= current.getInsets(types).left
                            && next.getInsets(types).top <= current.getInsets(types).top
                            && next.getInsets(types).right <= current.getInsets(types).right
                            && next.getInsets(types).bottom <= current.getInsets(types).bottom;
        }
    }

    private <T, V> void assertListElements(ArrayList<T> list, Function<T, V> getter,
            Predicate<V> predicate) {
        for (int i = 0; i <= list.size() - 1; i++) {
            V value = getter.apply(list.get(i));
            assertTrue("Predicate.test failed i=" + i + " value="
                    + value, predicate.test(value));
        }
    }

    private <T, V> void assertListElements(ArrayList<T> list, Function<T, V> getter,
            BiPredicate<V, V> comparator) {
        for (int i = 0; i <= list.size() - 2; i++) {
            V current = getter.apply(list.get(i));
            V next = getter.apply(list.get(i + 1));
            assertTrue(comparator.test(current, next));
        }
    }

    public static class AnimCallback extends WindowInsetsAnimation.Callback {

        public static class AnimationStep {

            AnimationStep(WindowInsets insets, float fraction, float interpolatedFraction,
                    float alpha) {
                this.insets = insets;
                this.fraction = fraction;
                this.interpolatedFraction = interpolatedFraction;
                this.alpha = alpha;
            }

            WindowInsets insets;
            float fraction;
            float interpolatedFraction;
            float alpha;
        }

        WindowInsetsAnimation lastAnimation;
        volatile boolean animationDone;
        final ArrayList<AnimationStep> animationSteps = new ArrayList<>();

        public AnimCallback(int dispatchMode) {
            super(dispatchMode);
        }

        @Override
        public void onPrepare(WindowInsetsAnimation animation) {
            animationSteps.clear();
            lastAnimation = animation;
        }

        @Override
        public WindowInsetsAnimation.Bounds onStart(
                WindowInsetsAnimation animation, WindowInsetsAnimation.Bounds bounds) {
            return bounds;
        }

        @Override
        public WindowInsets onProgress(WindowInsets insets,
                List<WindowInsetsAnimation> runningAnimations) {
            animationSteps.add(new AnimationStep(insets, lastAnimation.getFraction(),
                    lastAnimation.getInterpolatedFraction(), lastAnimation.getAlpha()));
            return WindowInsets.CONSUMED;
        }

        @Override
        public void onEnd(WindowInsetsAnimation animation) {
            animationDone = true;
        }
    }

    protected static class MultiAnimCallback extends WindowInsetsAnimation.Callback {

        WindowInsetsAnimation statusBarAnim;
        WindowInsetsAnimation navBarAnim;
        WindowInsetsAnimation imeAnim;
        volatile boolean animationDone;
        final ArrayList<AnimationStep> statusAnimSteps = new ArrayList<>();
        final ArrayList<AnimationStep> navAnimSteps = new ArrayList<>();
        final ArrayList<AnimationStep> imeAnimSteps = new ArrayList<>();
        Runnable startRunnable;
        final ArraySet<WindowInsetsAnimation> runningAnims = new ArraySet<>();

        public MultiAnimCallback() {
            super(DISPATCH_MODE_STOP);
        }

        @Override
        public void onPrepare(WindowInsetsAnimation animation) {
            if ((animation.getTypeMask() & statusBars()) != 0) {
                statusBarAnim = animation;
            }
            if ((animation.getTypeMask() & navigationBars()) != 0) {
                navBarAnim = animation;
            }
            if ((animation.getTypeMask() & ime()) != 0) {
                imeAnim = animation;
            }
        }

        @Override
        public WindowInsetsAnimation.Bounds onStart(
                WindowInsetsAnimation animation, WindowInsetsAnimation.Bounds bounds) {
            if (startRunnable != null) {
                startRunnable.run();
            }
            runningAnims.add(animation);
            return bounds;
        }

        @Override
        public WindowInsets onProgress(WindowInsets insets,
                List<WindowInsetsAnimation> runningAnimations) {
            if (statusBarAnim != null) {
                statusAnimSteps.add(new AnimationStep(insets, statusBarAnim.getFraction(),
                        statusBarAnim.getInterpolatedFraction(), statusBarAnim.getAlpha()));
            }
            if (navBarAnim != null) {
                navAnimSteps.add(new AnimationStep(insets, navBarAnim.getFraction(),
                        navBarAnim.getInterpolatedFraction(), navBarAnim.getAlpha()));
            }
            if (imeAnim != null) {
                imeAnimSteps.add(new AnimationStep(insets, imeAnim.getFraction(),
                        imeAnim.getInterpolatedFraction(), imeAnim.getAlpha()));
            }

            assertEquals(runningAnims.size(), runningAnimations.size());
            for (int i = runningAnimations.size() - 1; i >= 0; i--) {
                Assert.assertNotEquals(-1,
                        runningAnims.indexOf(runningAnimations.get(i)));
            }

            return WindowInsets.CONSUMED;
        }

        @Override
        public void onEnd(WindowInsetsAnimation animation) {
            runningAnims.remove(animation);
            if (runningAnims.isEmpty()) {
                animationDone = true;
            }
        }
    }

    public static class TestActivity extends FocusableActivity {

        private final String mEditTextMarker =
                "android.server.wm.WindowInsetsAnimationTestBase.TestActivity"
                        + SystemClock.elapsedRealtimeNanos();

        AnimCallback mCallback =
                spy(new AnimCallback(WindowInsetsAnimation.Callback.DISPATCH_MODE_STOP));
        WindowInsets mLastWindowInsets;

        View.OnApplyWindowInsetsListener mListener;
        LinearLayout mView;
        View mChild;
        EditText mEditor;

        public class InsetsListener implements View.OnApplyWindowInsetsListener {

            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                mLastWindowInsets = insets;
                return WindowInsets.CONSUMED;
            }
        }

        @NonNull
        String getEditTextMarker() {
            return mEditTextMarker;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            mListener = spy(new InsetsListener());
            mView = new LinearLayout(this);
            mView.setWindowInsetsAnimationCallback(mCallback);
            mView.setOnApplyWindowInsetsListener(mListener);
            mChild = new TextView(this);
            mEditor = new EditText(this);
            mEditor.setPrivateImeOptions(mEditTextMarker);
            mView.addView(mChild);
            mView.addView(mEditor);

            getWindow().setDecorFitsSystemWindows(false);
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            getWindow().setSoftInputMode(SOFT_INPUT_STATE_HIDDEN);
            setContentView(mView);
            mEditor.requestFocus();
        }
    }
}
