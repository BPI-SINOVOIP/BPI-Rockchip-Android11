/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.server.wm;

import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.verify;
import static com.android.server.wm.SurfaceAnimator.ANIMATION_TYPE_APP_TRANSITION;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.intThat;

import android.platform.test.annotations.Presubmit;
import android.view.SurfaceControl;

import androidx.test.filters.FlakyTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;


/**
 * Animation related tests for the {@link ActivityRecord} class.
 *
 * Build/Install/Run:
 *  atest AppWindowTokenAnimationTests
 */
@SmallTest
@Presubmit
@RunWith(WindowTestRunner.class)
public class AppWindowTokenAnimationTests extends WindowTestsBase {

    private ActivityRecord mActivity;

    @Mock
    private AnimationAdapter mSpec;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivity = createTestActivityRecord(mDisplayContent, WINDOWING_MODE_FULLSCREEN,
                ACTIVITY_TYPE_STANDARD);
    }

    @Test
    public void clipAfterAnim_boundsLayerIsCreated() {
        mActivity.mNeedsAnimationBoundsLayer = true;

        mActivity.mSurfaceAnimator.startAnimation(mTransaction, mSpec, true /* hidden */,
                ANIMATION_TYPE_APP_TRANSITION);
        verify(mTransaction).reparent(eq(mActivity.getSurfaceControl()),
                eq(mActivity.mSurfaceAnimator.mLeash));
        verify(mTransaction).reparent(eq(mActivity.mSurfaceAnimator.mLeash),
                eq(mActivity.mAnimationBoundsLayer));
    }

    @Test
    public void clipAfterAnim_boundsLayerZBoosted() {
        mActivity.mNeedsAnimationBoundsLayer = true;
        mActivity.mNeedsZBoost = true;

        mActivity.mSurfaceAnimator.startAnimation(mTransaction, mSpec, true /* hidden */,
                ANIMATION_TYPE_APP_TRANSITION);
        verify(mTransaction).setLayer(eq(mActivity.mAnimationBoundsLayer),
                intThat(layer -> layer >= ActivityRecord.Z_BOOST_BASE));
    }

    @Test
    public void clipAfterAnim_boundsLayerIsDestroyed() {
        mActivity.mNeedsAnimationBoundsLayer = true;
        mActivity.mSurfaceAnimator.startAnimation(mTransaction, mSpec, true /* hidden */,
                ANIMATION_TYPE_APP_TRANSITION);
        final SurfaceControl leash = mActivity.mSurfaceAnimator.mLeash;
        final SurfaceControl animationBoundsLayer = mActivity.mAnimationBoundsLayer;
        final ArgumentCaptor<SurfaceAnimator.OnAnimationFinishedCallback> callbackCaptor =
                ArgumentCaptor.forClass(
                        SurfaceAnimator.OnAnimationFinishedCallback.class);
        verify(mSpec).startAnimation(any(), any(), eq(ANIMATION_TYPE_APP_TRANSITION),
                callbackCaptor.capture());

        callbackCaptor.getValue().onAnimationFinished(
                ANIMATION_TYPE_APP_TRANSITION, mSpec);
        verify(mTransaction).remove(eq(leash));
        verify(mTransaction).remove(eq(animationBoundsLayer));
        assertThat(mActivity.mNeedsAnimationBoundsLayer).isFalse();
    }

    @Test
    public void clipAfterAnimCancelled_boundsLayerIsDestroyed() {
        mActivity.mNeedsAnimationBoundsLayer = true;
        mActivity.mSurfaceAnimator.startAnimation(mTransaction, mSpec, true /* hidden */,
                ANIMATION_TYPE_APP_TRANSITION);
        final SurfaceControl leash = mActivity.mSurfaceAnimator.mLeash;
        final SurfaceControl animationBoundsLayer = mActivity.mAnimationBoundsLayer;

        mActivity.mSurfaceAnimator.cancelAnimation();
        verify(mTransaction).remove(eq(leash));
        verify(mTransaction).remove(eq(animationBoundsLayer));
        assertThat(mActivity.mNeedsAnimationBoundsLayer).isFalse();
    }

    @Test
    public void clipNoneAnim_boundsLayerIsNotCreated() {
        mActivity.mNeedsAnimationBoundsLayer = false;

        mActivity.mSurfaceAnimator.startAnimation(mTransaction, mSpec, true /* hidden */,
                ANIMATION_TYPE_APP_TRANSITION);
        verify(mTransaction).reparent(eq(mActivity.getSurfaceControl()),
                eq(mActivity.mSurfaceAnimator.mLeash));
        assertThat(mActivity.mAnimationBoundsLayer).isNull();
    }
}
