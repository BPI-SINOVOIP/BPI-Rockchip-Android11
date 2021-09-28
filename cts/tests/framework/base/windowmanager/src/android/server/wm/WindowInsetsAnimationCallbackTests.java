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
import static android.view.WindowInsetsAnimation.Callback.DISPATCH_MODE_CONTINUE_ON_SUBTREE;
import static android.view.WindowInsetsAnimation.Callback.DISPATCH_MODE_STOP;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.content.Context;
import android.graphics.Insets;
import android.platform.test.annotations.Presubmit;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsAnimation;
import android.view.WindowInsetsAnimation.Bounds;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;

import java.util.Collections;
import java.util.List;

@Presubmit
public class WindowInsetsAnimationCallbackTests {

    private static final Bounds BOUNDS = new Bounds(NONE, Insets.of(1000, 2000, 3000, 4000));
    private static final WindowInsetsAnimation ANIMATION = new WindowInsetsAnimation(1, null, 1000);
    private static final WindowInsets INSETS = new WindowInsets.Builder()
            .setSystemWindowInsets(BOUNDS.getUpperBound()).build();
    private static final List<WindowInsetsAnimation> RUNNING = Collections.singletonList(ANIMATION);

    final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

    private ViewGroup mRoot;
    private ViewGroup mBlocking;
    private ViewGroup mSibling;
    private View mChild;
    private RecordingCallback mRootCallback;
    private RecordingCallback mBlockingCallback;
    private RecordingCallback mSiblingCallback;
    private RecordingCallback mChildCallback;

    @Before
    public void setUp() throws Exception {
        mRoot = new FrameLayout(mContext);
        mBlocking = new FrameLayout(mContext);
        mSibling = new FrameLayout(mContext);
        mChild = new View(mContext);
        mRoot.addView(mBlocking);
        mRoot.addView(mSibling);
        mBlocking.addView(mChild);

        mRootCallback = new RecordingCallback(DISPATCH_MODE_CONTINUE_ON_SUBTREE);
        mBlockingCallback = new RecordingCallback(DISPATCH_MODE_STOP);
        mSiblingCallback = new RecordingCallback(DISPATCH_MODE_STOP);
        mChildCallback = new RecordingCallback(DISPATCH_MODE_CONTINUE_ON_SUBTREE);

        mRoot.setWindowInsetsAnimationCallback(mRootCallback);
        mBlocking.setWindowInsetsAnimationCallback(mBlockingCallback);
        mSibling.setWindowInsetsAnimationCallback(mSiblingCallback);
        mChild.setWindowInsetsAnimationCallback(mChildCallback);
    }

    @Test
    public void dispatchWindowInsetsAnimationPrepare() {
        mRoot.dispatchWindowInsetsAnimationPrepare(ANIMATION);
        assertSame(ANIMATION, mRootCallback.mOnPrepare);
        assertSame(ANIMATION, mBlockingCallback.mOnPrepare);
        assertSame(ANIMATION, mSiblingCallback.mOnPrepare);
        assertNull(mChildCallback.mOnPrepare);
    }

    @Test
    public void dispatchWindowInsetsAnimationStart() {
        mRoot.dispatchWindowInsetsAnimationStart(ANIMATION, BOUNDS);
        assertSame(ANIMATION, mRootCallback.mOnStart);
        assertSame(ANIMATION, mBlockingCallback.mOnStart);
        assertSame(ANIMATION, mSiblingCallback.mOnStart);
        assertNull(mChildCallback.mOnStart);

        assertSame(BOUNDS, mRootCallback.mOnStartBoundsIn);
        assertSame(mRootCallback.mOnStartBoundsOut, mBlockingCallback.mOnStartBoundsIn);
        assertSame(mRootCallback.mOnStartBoundsOut, mSiblingCallback.mOnStartBoundsIn);
    }

    @Test
    public void dispatchWindowInsetsAnimationProgress() {
        mRoot.dispatchWindowInsetsAnimationProgress(INSETS, RUNNING);
        assertSame(RUNNING, mRootCallback.mOnProgress);
        assertSame(RUNNING, mBlockingCallback.mOnProgress);
        assertSame(RUNNING, mSiblingCallback.mOnProgress);
        assertNull(mChildCallback.mOnProgress);

        assertSame(INSETS, mRootCallback.mOnProgressIn);
        assertSame(mRootCallback.mOnProgressOut, mBlockingCallback.mOnProgressIn);
        assertSame(mRootCallback.mOnProgressOut, mSiblingCallback.mOnProgressIn);
    }

    @Test
    public void dispatchWindowInsetsAnimationEnd() {
        mRoot.dispatchWindowInsetsAnimationEnd(ANIMATION);
        assertSame(ANIMATION, mRootCallback.mOnEnd);
        assertSame(ANIMATION, mBlockingCallback.mOnEnd);
        assertSame(ANIMATION, mSiblingCallback.mOnEnd);
        assertNull(mChildCallback.mOnEnd);
    }

    @Test
    public void getDispatchMode() {
        assertEquals(DISPATCH_MODE_STOP,
                new RecordingCallback(DISPATCH_MODE_STOP).getDispatchMode());
        assertEquals(DISPATCH_MODE_CONTINUE_ON_SUBTREE,
                new RecordingCallback(DISPATCH_MODE_CONTINUE_ON_SUBTREE).getDispatchMode());
    }

    public static class RecordingCallback extends WindowInsetsAnimation.Callback {

        WindowInsetsAnimation mOnPrepare;
        WindowInsetsAnimation mOnStart;
        Bounds mOnStartBoundsIn;
        Bounds mOnStartBoundsOut;
        WindowInsetsAnimation mOnEnd;
        WindowInsets mOnProgressIn;
        WindowInsets mOnProgressOut;
        List<WindowInsetsAnimation> mOnProgress;

        public RecordingCallback(int dispatchMode) {
            super(dispatchMode);
        }

        @Override
        public void onPrepare(@NonNull WindowInsetsAnimation animation) {
            mOnPrepare = animation;
        }

        @NonNull
        @Override
        public Bounds onStart(@NonNull WindowInsetsAnimation animation, @NonNull Bounds bounds) {
            mOnStart = animation;
            mOnStartBoundsIn = bounds;
            return mOnStartBoundsOut = bounds.inset(Insets.of(1, 2, 3, 4));
        }

        @NonNull
        @Override
        public WindowInsets onProgress(@NonNull WindowInsets insets,
                @NonNull List<WindowInsetsAnimation> runningAnimations) {
            mOnProgress = runningAnimations;
            mOnProgressIn = insets;
            return mOnProgressOut = insets.inset(1, 2, 3, 4);
        }

        @Override
        public void onEnd(@NonNull WindowInsetsAnimation animation) {
            mOnEnd = animation;
        }
    }
}
