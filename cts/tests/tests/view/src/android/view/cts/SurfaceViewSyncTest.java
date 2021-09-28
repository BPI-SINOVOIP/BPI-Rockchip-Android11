/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.view.cts;

import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.media.MediaPlayer;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.view.Gravity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;
import android.view.cts.surfacevalidator.AnimationFactory;
import android.view.cts.surfacevalidator.AnimationTestCase;
import android.view.cts.surfacevalidator.CapturedActivityWithResource;
import android.view.cts.surfacevalidator.PixelChecker;
import android.view.cts.surfacevalidator.ViewFactory;
import android.widget.FrameLayout;

import androidx.test.filters.LargeTest;
import androidx.test.filters.RequiresDevice;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@LargeTest
@SuppressLint("RtlHardcoded")
@RequiresDevice
public class SurfaceViewSyncTest {
    private static final String TAG = "SurfaceViewSyncTests";

    @Rule
    public ActivityTestRule<CapturedActivityWithResource> mActivityRule =
            new ActivityTestRule<>(CapturedActivityWithResource.class);

    @Rule
    public TestName mName = new TestName();

    private CapturedActivityWithResource mActivity;
    private MediaPlayer mMediaPlayer;

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
        mMediaPlayer = mActivity.getMediaPlayer();
    }

    /**
     * Want to be especially sure we don't leave up the permission dialog, so try and dismiss
     * after test.
     */
    @After
    public void tearDown() throws UiObjectNotFoundException {
        mActivity.dismissPermissionDialog();
    }

    private static ValueAnimator makeInfinite(ValueAnimator a) {
        a.setRepeatMode(ObjectAnimator.REVERSE);
        a.setRepeatCount(ObjectAnimator.INFINITE);
        a.setDuration(200);
        a.setInterpolator(new LinearInterpolator());
        return a;
    }

    ///////////////////////////////////////////////////////////////////////////
    // ViewFactories
    ///////////////////////////////////////////////////////////////////////////

    private ViewFactory sEmptySurfaceViewFactory = context -> {
        SurfaceView surfaceView = new SurfaceView(context);

        // prevent transparent region optimization, which is invalid for a SurfaceView moving around
        surfaceView.setWillNotDraw(false);

        return surfaceView;
    };

    private ViewFactory sGreenSurfaceViewFactory = context -> {
        SurfaceView surfaceView = new SurfaceView(context);

        // prevent transparent region optimization, which is invalid for a SurfaceView moving around
        surfaceView.setWillNotDraw(false);

        surfaceView.getHolder().setFixedSize(640, 480);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {}

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Canvas canvas = holder.lockCanvas();
                canvas.drawColor(Color.GREEN);
                holder.unlockCanvasAndPost(canvas);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {}
        });
        return surfaceView;
    };

    private ViewFactory sVideoViewFactory = context -> {
        SurfaceView surfaceView = new SurfaceView(context);

        // prevent transparent region optimization, which is invalid for a SurfaceView moving around
        surfaceView.setWillNotDraw(false);

        surfaceView.getHolder().setFixedSize(640, 480);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mMediaPlayer.setSurface(holder.getSurface());
                mMediaPlayer.start();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                mMediaPlayer.pause();
                mMediaPlayer.setSurface(null);
            }
        });
        return surfaceView;
    };

    ///////////////////////////////////////////////////////////////////////////
    // AnimationFactories
    ///////////////////////////////////////////////////////////////////////////

    private AnimationFactory sSmallScaleAnimationFactory = view -> {
        view.setPivotX(0);
        view.setPivotY(0);
        PropertyValuesHolder pvhX = PropertyValuesHolder.ofFloat(View.SCALE_X, 0.01f, 1f);
        PropertyValuesHolder pvhY = PropertyValuesHolder.ofFloat(View.SCALE_Y, 0.01f, 1f);
        return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view, pvhX, pvhY));
    };

    private AnimationFactory sBigScaleAnimationFactory = view -> {
        view.setTranslationX(10);
        view.setTranslationY(10);
        view.setPivotX(0);
        view.setPivotY(0);
        PropertyValuesHolder pvhX = PropertyValuesHolder.ofFloat(View.SCALE_X, 1f, 3f);
        PropertyValuesHolder pvhY = PropertyValuesHolder.ofFloat(View.SCALE_Y, 1f, 3f);
        return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view, pvhX, pvhY));
    };

    private AnimationFactory sTranslateAnimationFactory = view -> {
        PropertyValuesHolder pvhX = PropertyValuesHolder.ofFloat(View.TRANSLATION_X, 10f, 30f);
        PropertyValuesHolder pvhY = PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, 10f, 30f);
        return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view, pvhX, pvhY));
    };

    ///////////////////////////////////////////////////////////////////////////
    // Tests
    ///////////////////////////////////////////////////////////////////////////

    /** Draws a moving 10x10 black rectangle, validates 100 pixels of black are seen each frame */
    @Test
    public void testSmallRect() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                context -> new View(context) {
                    // draw a single pixel
                    final Paint sBlackPaint = new Paint();
                    @Override
                    protected void onDraw(Canvas canvas) {
                        canvas.drawRect(0, 0, 10, 10, sBlackPaint);
                    }

                    @SuppressWarnings("unused")
                    void setOffset(int offset) {
                        // Note: offset by integer values, to ensure no rounding
                        // is done in rendering layer, as that may be brittle
                        setTranslationX(offset);
                        setTranslationY(offset);
                    }
                },
                new FrameLayout.LayoutParams(100, 100, Gravity.LEFT | Gravity.TOP),
                view -> makeInfinite(ObjectAnimator.ofInt(view, "offset", 10, 30)),
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount >= 90 && blackishPixelCount <= 110;
                    }
                }), mName);
    }

    /**
     * Verifies that a SurfaceView without a surface is entirely black, with pixel count being
     * approximate to avoid rounding brittleness.
     */
    @Test
    public void testEmptySurfaceView() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sEmptySurfaceViewFactory,
                new FrameLayout.LayoutParams(100, 100, Gravity.LEFT | Gravity.TOP),
                sTranslateAnimationFactory,
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount > 9000 && blackishPixelCount < 11000;
                    }
                }), mName);
    }

    @Test
    public void testSurfaceViewSmallScale() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sGreenSurfaceViewFactory,
                new FrameLayout.LayoutParams(320, 240, Gravity.LEFT | Gravity.TOP),
                sSmallScaleAnimationFactory,
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }

    @Test
    public void testSurfaceViewBigScale() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sGreenSurfaceViewFactory,
                new FrameLayout.LayoutParams(640, 480, Gravity.LEFT | Gravity.TOP),
                sBigScaleAnimationFactory,
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }

    @Test
    public void testVideoSurfaceViewTranslate() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sVideoViewFactory,
                new FrameLayout.LayoutParams(640, 480, Gravity.LEFT | Gravity.TOP),
                sTranslateAnimationFactory,
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }

    @Test
    public void testVideoSurfaceViewRotated() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sVideoViewFactory,
                new FrameLayout.LayoutParams(100, 100, Gravity.LEFT | Gravity.TOP),
                view -> makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view,
                        PropertyValuesHolder.ofFloat(View.TRANSLATION_X, 10f, 30f),
                        PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, 10f, 30f),
                        PropertyValuesHolder.ofFloat(View.ROTATION, 45f, 45f))),
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }

    @Test
    public void testVideoSurfaceViewEdgeCoverage() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sVideoViewFactory,
                new FrameLayout.LayoutParams(640, 480, Gravity.CENTER),
                view -> {
                    ViewGroup parent = (ViewGroup) view.getParent();
                    final int x = parent.getWidth() / 2;
                    final int y = parent.getHeight() / 2;

                    // Animate from left, to top, to right, to bottom
                    return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view,
                            PropertyValuesHolder.ofFloat(View.TRANSLATION_X, -x, 0, x, 0, -x),
                            PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, 0, -y, 0, y, 0)));
                },
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }

    @Test
    public void testVideoSurfaceViewCornerCoverage() throws Throwable {
        mActivity.verifyTest(new AnimationTestCase(
                sVideoViewFactory,
                new FrameLayout.LayoutParams(640, 480, Gravity.CENTER),
                view -> {
                    ViewGroup parent = (ViewGroup) view.getParent();
                    final int x = parent.getWidth() / 2;
                    final int y = parent.getHeight() / 2;

                    // Animate from top left, to top right, to bottom right, to bottom left
                    return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view,
                            PropertyValuesHolder.ofFloat(View.TRANSLATION_X, -x, x, x, -x, -x),
                            PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, -y, -y, y, y, -y)));
                },
                new PixelChecker() {
                    @Override
                    public boolean checkPixels(int blackishPixelCount, int width, int height) {
                        return blackishPixelCount == 0;
                    }
                }), mName);
    }
}
