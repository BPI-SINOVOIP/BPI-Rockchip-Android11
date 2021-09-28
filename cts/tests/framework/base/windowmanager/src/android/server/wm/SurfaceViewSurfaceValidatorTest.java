/*
 * Copyright 2019 The Android Open Source Project
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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.view.cts.surfacevalidator.AnimationFactory;
import android.view.cts.surfacevalidator.CapturedActivity;
import android.view.cts.surfacevalidator.PixelChecker;
import android.view.cts.surfacevalidator.PixelColor;
import android.view.cts.surfacevalidator.SurfaceControlTestCase;
import android.view.Gravity;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

import androidx.test.rule.ActivityTestRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;

public class SurfaceViewSurfaceValidatorTest {
    private static final int DEFAULT_LAYOUT_WIDTH = 100;
    private static final int DEFAULT_LAYOUT_HEIGHT = 100;
    private static final int DEFAULT_BUFFER_WIDTH = 640;
    private static final int DEFAULT_BUFFER_HEIGHT = 480;

    @Rule
    public ActivityTestRule<CapturedActivity> mActivityRule =
            new ActivityTestRule<>(CapturedActivity.class);

    @Rule
    public TestName mName = new TestName();
    private CapturedActivity mActivity;

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
        mActivity.dismissPermissionDialog();
    }

    /**
     * Want to be especially sure we don't leave up the permission dialog, so try and dismiss
     * after test.
     */
    @After
    public void tearDown() throws UiObjectNotFoundException {
        mActivity.dismissPermissionDialog();
    }

    class SurfaceFiller implements SurfaceHolder.Callback {
        SurfaceView mSurfaceView;

        SurfaceFiller(Context c) {
            mSurfaceView = new SurfaceView(c);
            mSurfaceView.getHolder().addCallback(this);
        }

        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            Canvas canvas = holder.lockHardwareCanvas();
            canvas.drawColor(Color.GREEN);
            holder.unlockCanvasAndPost(canvas);
        }

        public void surfaceCreated(SurfaceHolder holder) {
        }

        public void surfaceDestroyed(SurfaceHolder holder) {
        }
    }

    /**
     * Verify that showing a SurfaceView on top but not drawing in to it will not produce a background.
     */
    @Test
    public void testOnTopHasNoBackground() throws Throwable {
        SurfaceControlTestCase.ParentSurfaceConsumer psc =
            new SurfaceControlTestCase.ParentSurfaceConsumer () {
                    @Override
                    public void addChildren(SurfaceControl parent) {
                    }
            };
        PixelChecker pixelChecker = new PixelChecker(PixelColor.BLACK) {
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount == 0;
                }
            };
        SurfaceControlTestCase t = new SurfaceControlTestCase(psc, null,
                pixelChecker, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                DEFAULT_BUFFER_WIDTH, DEFAULT_BUFFER_HEIGHT);
        mActivity.verifyTest(t, mName);
    }

    class TwoSurfaceViewTest extends SurfaceControlTestCase {
        TwoSurfaceViewTest(ParentSurfaceConsumer psc,
                AnimationFactory animationFactory, PixelChecker pixelChecker,
                int layoutWidth, int layoutHeight, int bufferWidth, int bufferHeight) {
            super(psc, animationFactory, pixelChecker, layoutWidth, layoutHeight, bufferWidth, bufferHeight);
        }
        @Override
        public void start(Context context, FrameLayout parent) {
            super.start(context, parent);
            SurfaceFiller sc = new SurfaceFiller(mActivity);
            parent.addView(sc.mSurfaceView,
                    new FrameLayout.LayoutParams(DEFAULT_LAYOUT_WIDTH,
                            DEFAULT_LAYOUT_HEIGHT, Gravity.LEFT | Gravity.TOP));
        }
    };

    // Here we add a second translucent surface view and verify that the background
    // is behind all SurfaceView (e.g. the first is not obscured)
    @Test
    public void testBackgroundIsBehindAllSurfaceView() throws Throwable {
        SurfaceControlTestCase.ParentSurfaceConsumer psc =
            new SurfaceControlTestCase.ParentSurfaceConsumer () {
                    @Override
                    public void addChildren(SurfaceControl parent) {
                    }
            };
        PixelChecker pixelChecker = new PixelChecker(PixelColor.BLACK) {
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount == 0;
                }
            };
        TwoSurfaceViewTest t = new TwoSurfaceViewTest(psc, null,
                pixelChecker, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                DEFAULT_BUFFER_WIDTH, DEFAULT_BUFFER_HEIGHT);
        mActivity.verifyTest(t, mName);
    }
}
