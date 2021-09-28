/*
 * Copyright 2018 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;

import android.animation.ObjectAnimator;
import android.animation.PropertyValuesHolder;
import android.animation.ValueAnimator;
import android.graphics.Canvas;
import android.graphics.Color;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.test.suitebuilder.annotation.LargeTest;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.animation.LinearInterpolator;
import android.view.cts.surfacevalidator.AnimationFactory;
import android.view.cts.surfacevalidator.CapturedActivity;
import android.view.cts.surfacevalidator.PixelChecker;
import android.view.cts.surfacevalidator.PixelColor;
import android.view.cts.surfacevalidator.SurfaceControlTestCase;

import androidx.test.filters.RequiresDevice;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class ASurfaceControlTest {
    static {
        System.loadLibrary("ctsview_jni");
    }

    private static final String TAG = ASurfaceControlTest.class.getSimpleName();
    private static final boolean DEBUG = false;

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
    }

    /**
     * Want to be especially sure we don't leave up the permission dialog, so try and dismiss
     * after test.
     */
    @After
    public void tearDown() throws UiObjectNotFoundException {
        mActivity.dismissPermissionDialog();
    }

    ///////////////////////////////////////////////////////////////////////////
    // SurfaceHolder.Callbacks
    ///////////////////////////////////////////////////////////////////////////

    private abstract class BasicSurfaceHolderCallback implements SurfaceHolder.Callback {
        private Set<Long> mSurfaceControls = new HashSet<Long>();
        private Set<Long> mBuffers = new HashSet<Long>();

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            Canvas canvas = holder.lockCanvas();
            canvas.drawColor(Color.YELLOW);
            holder.unlockCanvasAndPost(canvas);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            for (Long surfaceControl : mSurfaceControls) {
                reparent(surfaceControl, 0);
                nSurfaceControl_release(surfaceControl);
            }
            mSurfaceControls.clear();

            for (Long buffer : mBuffers) {
                nSurfaceTransaction_releaseBuffer(buffer);
            }
            mBuffers.clear();
        }

        public long createSurfaceTransaction() {
            long surfaceTransaction = nSurfaceTransaction_create();
            assertTrue("failed to create surface transaction", surfaceTransaction != 0);
            return surfaceTransaction;
        }

        public void applySurfaceTransaction(long surfaceTransaction) {
            nSurfaceTransaction_apply(surfaceTransaction);
        }

        public void deleteSurfaceTransaction(long surfaceTransaction) {
            nSurfaceTransaction_delete(surfaceTransaction);
        }

        public void applyAndDeleteSurfaceTransaction(long surfaceTransaction) {
            nSurfaceTransaction_apply(surfaceTransaction);
            nSurfaceTransaction_delete(surfaceTransaction);
        }

        public long createFromWindow(Surface surface) {
            long surfaceControl = nSurfaceControl_createFromWindow(surface);
            assertTrue("failed to create surface control", surfaceControl != 0);

            mSurfaceControls.add(surfaceControl);
            return surfaceControl;
        }

        public long create(long parentSurfaceControl) {
            long childSurfaceControl = nSurfaceControl_create(parentSurfaceControl);
            assertTrue("failed to create child surface control", childSurfaceControl != 0);

            mSurfaceControls.add(childSurfaceControl);
            return childSurfaceControl;
        }

        public void setSolidBuffer(
                long surfaceControl, long surfaceTransaction, int width, int height, int color) {
            long buffer = nSurfaceTransaction_setSolidBuffer(
                    surfaceControl, surfaceTransaction, width, height, color);
            assertTrue("failed to set buffer", buffer != 0);
            mBuffers.add(buffer);
        }

        public void setSolidBuffer(long surfaceControl, int width, int height, int color) {
            long surfaceTransaction = createSurfaceTransaction();
            setSolidBuffer(surfaceControl, surfaceTransaction, width, height, color);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setQuadrantBuffer(long surfaceControl, long surfaceTransaction, int width,
                int height, int colorTopLeft, int colorTopRight, int colorBottomRight,
                int colorBottomLeft) {
            long buffer = nSurfaceTransaction_setQuadrantBuffer(surfaceControl, surfaceTransaction,
                    width, height, colorTopLeft, colorTopRight, colorBottomRight, colorBottomLeft);
            assertTrue("failed to set buffer", buffer != 0);
            mBuffers.add(buffer);
        }

        public void setQuadrantBuffer(long surfaceControl, int width, int height, int colorTopLeft,
                int colorTopRight, int colorBottomRight, int colorBottomLeft) {
            long surfaceTransaction = createSurfaceTransaction();
            setQuadrantBuffer(surfaceControl, surfaceTransaction, width, height, colorTopLeft,
                    colorTopRight, colorBottomRight, colorBottomLeft);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setVisibility(long surfaceControl, long surfaceTransaction, boolean visible) {
            nSurfaceTransaction_setVisibility(surfaceControl, surfaceTransaction, visible);
        }

        public void setVisibility(long surfaceControl, boolean visible) {
            long surfaceTransaction = createSurfaceTransaction();
            setVisibility(surfaceControl, surfaceTransaction, visible);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setBufferOpaque(long surfaceControl, long surfaceTransaction, boolean opaque) {
            nSurfaceTransaction_setBufferOpaque(surfaceControl, surfaceTransaction, opaque);
        }

        public void setBufferOpaque(long surfaceControl, boolean opaque) {
            long surfaceTransaction = createSurfaceTransaction();
            setBufferOpaque(surfaceControl, surfaceTransaction, opaque);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setGeometry(long surfaceControl, long surfaceTransaction, int srcLeft,
                int srcTop, int srcRight, int srcBottom, int dstLeft, int dstTop, int dstRight,
                int dstBottom, int transform) {
            nSurfaceTransaction_setGeometry(
                    surfaceControl, surfaceTransaction, srcLeft, srcTop, srcRight, srcBottom,
                    dstLeft, dstTop, dstRight, dstBottom, transform);
        }

        public void setGeometry(long surfaceControl, int srcLeft, int srcTop, int srcRight,
                int srcBottom, int dstLeft, int dstTop, int dstRight, int dstBottom,
                int transform) {
            long surfaceTransaction = createSurfaceTransaction();
            setGeometry(surfaceControl, surfaceTransaction, srcLeft, srcTop, srcRight, srcBottom,
                    dstLeft, dstTop, dstRight, dstBottom, transform);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setDamageRegion(long surfaceControl, long surfaceTransaction, int left, int top,
                int right, int bottom) {
            nSurfaceTransaction_setDamageRegion(
                    surfaceControl, surfaceTransaction, left, top, right, bottom);
        }

        public void setDamageRegion(long surfaceControl, int left, int top, int right, int bottom) {
            long surfaceTransaction = createSurfaceTransaction();
            setDamageRegion(surfaceControl, surfaceTransaction, left, top, right, bottom);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setZOrder(long surfaceControl, long surfaceTransaction, int z) {
            nSurfaceTransaction_setZOrder(surfaceControl, surfaceTransaction, z);
        }

        public void setZOrder(long surfaceControl, int z) {
            long surfaceTransaction = createSurfaceTransaction();
            setZOrder(surfaceControl, surfaceTransaction, z);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setBufferAlpha(long surfaceControl, long surfaceTransaction, double alpha) {
            nSurfaceTransaction_setBufferAlpha(surfaceControl, surfaceTransaction, alpha);
        }

        public void setBufferAlpha(long surfaceControl, double alpha) {
            long surfaceTransaction = createSurfaceTransaction();
            setBufferAlpha(surfaceControl, surfaceTransaction, alpha);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void reparent(long surfaceControl, long newParentSurfaceControl,
                             long surfaceTransaction) {
            nSurfaceTransaction_reparent(surfaceControl, newParentSurfaceControl,
                                         surfaceTransaction);
        }

        public void reparent(long surfaceControl, long newParentSurfaceControl) {
            long surfaceTransaction = createSurfaceTransaction();
            reparent(surfaceControl, newParentSurfaceControl, surfaceTransaction);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }

        public void setColor(long surfaceControl, long surfaceTransaction, float red, float green,
                float blue, float alpha) {
            nSurfaceTransaction_setColor(surfaceControl, surfaceTransaction, red, green, blue,
                    alpha);
        }

        public void setColor(long surfaceControl, float red, float green, float blue, float alpha) {
            long surfaceTransaction = createSurfaceTransaction();
            setColor(surfaceControl, surfaceTransaction, red, green, blue, alpha);
            applyAndDeleteSurfaceTransaction(surfaceTransaction);
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // AnimationFactories
    ///////////////////////////////////////////////////////////////////////////

    private static ValueAnimator makeInfinite(ValueAnimator a) {
        a.setRepeatMode(ObjectAnimator.REVERSE);
        a.setRepeatCount(ObjectAnimator.INFINITE);
        a.setDuration(200);
        a.setInterpolator(new LinearInterpolator());
        return a;
    }

    private static AnimationFactory sTranslateAnimationFactory = view -> {
        PropertyValuesHolder pvhX = PropertyValuesHolder.ofFloat(View.TRANSLATION_X, 10f, 30f);
        PropertyValuesHolder pvhY = PropertyValuesHolder.ofFloat(View.TRANSLATION_Y, 10f, 30f);
        return makeInfinite(ObjectAnimator.ofPropertyValuesHolder(view, pvhX, pvhY));
    };

    ///////////////////////////////////////////////////////////////////////////
    // Tests
    ///////////////////////////////////////////////////////////////////////////

    private void verifyTest(SurfaceHolder.Callback callback, PixelChecker pixelChecker)
                throws Throwable {
        mActivity.verifyTest(new SurfaceControlTestCase(callback, sTranslateAnimationFactory,
                                                 pixelChecker,
                                                 DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                                 DEFAULT_BUFFER_WIDTH, DEFAULT_BUFFER_HEIGHT),
                mName);
    }

    @Test
    public void testSurfaceTransaction_create() {
        mActivity.dismissPermissionDialog();

        long surfaceTransaction = nSurfaceTransaction_create();
        assertTrue("failed to create surface transaction", surfaceTransaction != 0);

        nSurfaceTransaction_delete(surfaceTransaction);
    }

    @Test
    public void testSurfaceTransaction_apply() {
        mActivity.dismissPermissionDialog();

        long surfaceTransaction = nSurfaceTransaction_create();
        assertTrue("failed to create surface transaction", surfaceTransaction != 0);

        Log.e("Transaction", "created: " + surfaceTransaction);

        nSurfaceTransaction_apply(surfaceTransaction);
        nSurfaceTransaction_delete(surfaceTransaction);
    }

    // INTRO: The following tests run a series of commands and verify the
    // output based on the number of pixels with a certain color on the display.
    //
    // The interface being tested is a NDK api but the only way to record the display
    // through public apis is in through the SDK. So the test logic and test verification
    // is in Java but the hooks that call into the NDK api are jni code.
    //
    // The set up is done during the surfaceCreated callback. In most cases, the
    // test uses the opportunity to create a child layer through createFromWindow and
    // performs operations on the child layer.
    //
    // When there is no visible buffer for the layer(s) the color defaults to black.
    // The test cases allow a +/- 10% error rate. This is based on the error
    // rate allowed in the SurfaceViewSyncTests

    @Test
    public void testSurfaceControl_createFromWindow() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());
                    }
                },
                new PixelChecker(PixelColor.YELLOW) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceControl_create() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBuffer() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBuffer_parentAndChild() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);

                        setSolidBuffer(parentSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.BLUE);
                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBuffer_childOnly() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);

                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setVisibility_show() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setVisibility(surfaceControl, true);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setVisibility_hide() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setVisibility(surfaceControl, false);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBufferOpaque_opaque() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setBufferOpaque(surfaceControl, true);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBufferOpaque_transparent() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.TRANSPARENT_RED);
                        setBufferOpaque(surfaceControl, false);
                    }
                },
                // setBufferOpaque is an optimization that can be used by SurfaceFlinger.
                // It isn't required to affect SurfaceFlinger's behavior.
                //
                // Ideally we would check for a specific blending of red with a layer below
                // it. Unfortunately we don't know what blending the layer will use and
                // we don't know what variation the GPU/DPU/blitter might have. Although
                // we don't know what shade of red might be present, we can at least check
                // that the optimization doesn't cause the framework to drop the buffer entirely.
                new PixelChecker(PixelColor.YELLOW) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setGeometry(surfaceControl, 0, 0, 100, 100, 0, 0, 640, 480, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_small() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setGeometry(surfaceControl, 0, 0, 100, 100, 64, 48, 320, 240, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //1600
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 1440 && pixelCount < 1760;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_childSmall() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);

                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        setGeometry(childSurfaceControl, 0, 0, 100, 100, 64, 48, 320, 240, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //1600
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 1440 && pixelCount < 1760;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_extraLarge() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setGeometry(surfaceControl, 0, 0, 100, 100, -100, -100, 740, 580, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_childExtraLarge() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);

                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        setGeometry(childSurfaceControl, 0, 0, 100, 100, -100, -100, 740, 580, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_negativeOffset() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setGeometry(surfaceControl, 0, 0, 100, 100, -32, -24, 320, 240, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_outOfParentBounds() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setGeometry(surfaceControl, 0, 0, 100, 100, 320, 240, 704, 504, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDestinationRect_twoLayers() throws Throwable {
        BasicSurfaceHolderCallback callback = new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.BLUE);
                        setGeometry(surfaceControl1, 0, 0, 100, 100, 64, 48, 192, 192, 0);
                        setGeometry(surfaceControl2, 0, 0, 100, 100, 448, 96, 576, 240, 0);
                    }
                };
        verifyTest(callback,
                new PixelChecker(PixelColor.RED) { //600
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 540 && pixelCount < 660;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.BLUE) { //600
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 540 && pixelCount < 660;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setSourceRect() throws Throwable {
        BasicSurfaceHolderCallback callback = new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, 0, 0, 100, 100, 0, 0, 640, 480, 0);
                    }
                };
        verifyTest(callback,
                new PixelChecker(PixelColor.RED) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.BLUE) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.MAGENTA) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.GREEN) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setSourceRect_smallCentered() throws Throwable {
        BasicSurfaceHolderCallback callback = new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, 10, 10, 90, 90, 0, 0, 640, 480, 0);
                    }
                };
        verifyTest(callback,
                new PixelChecker(PixelColor.RED) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.BLUE) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.MAGENTA) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.GREEN) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setSourceRect_small() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, 60, 10, 90, 90, 0, 0, 640, 480, 0);
                    }
                },
                new PixelChecker(PixelColor.MAGENTA) { //5000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 4500 && pixelCount < 5500;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setSourceRect_extraLarge() throws Throwable {
        BasicSurfaceHolderCallback callback = new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, -50, -50, 150, 150, 0, 0, 640, 480, 0);
                    }
                };
        verifyTest(callback,
                new PixelChecker(PixelColor.RED) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.BLUE) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.MAGENTA) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.GREEN) { //2500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 2250 && pixelCount < 2750;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setSourceRect_badOffset() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, -50, -50, 50, 50, 0, 0, 640, 480, 0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setTransform_flipH() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, 60, 10, 90, 90, 0, 0, 640, 480,
                                    /*NATIVE_WINDOW_TRANSFORM_FLIP_H*/ 1);
                    }
                },
                new PixelChecker(PixelColor.BLUE) { //5000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 4500 && pixelCount < 5500;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setTransform_rotate180() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setQuadrantBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED, PixelColor.BLUE,
                                PixelColor.MAGENTA, PixelColor.GREEN);
                        setGeometry(surfaceControl, 60, 10, 90, 90, 0, 0, 640, 480,
                                    /*NATIVE_WINDOW_TRANSFORM_ROT_180*/ 3);
                    }
                },
                new PixelChecker(PixelColor.BLUE) { //5000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 4500 && pixelCount < 5500;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setDamageRegion_all() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);

                        long surfaceTransaction = createSurfaceTransaction();
                        setSolidBuffer(surfaceControl, surfaceTransaction, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.BLUE);
                        setDamageRegion(surfaceControl, surfaceTransaction, 0, 0, 100, 100);
                        applyAndDeleteSurfaceTransaction(surfaceTransaction);
                    }
                },
                new PixelChecker(PixelColor.BLUE) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setZOrder_zero() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.MAGENTA);

                        setZOrder(surfaceControl1, 1);
                        setZOrder(surfaceControl2, 0);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setZOrder_positive() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.MAGENTA);

                        setZOrder(surfaceControl1, 1);
                        setZOrder(surfaceControl2, 5);
                    }
                },
                new PixelChecker(PixelColor.RED) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setZOrder_negative() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.MAGENTA);

                        setZOrder(surfaceControl1, 1);
                        setZOrder(surfaceControl2, -15);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setZOrder_max() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.MAGENTA);

                        setZOrder(surfaceControl1, 1);
                        setZOrder(surfaceControl2, Integer.MAX_VALUE);
                    }
                },
                new PixelChecker(PixelColor.RED) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setZOrder_min() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl1 = createFromWindow(holder.getSurface());
                        long surfaceControl2 = createFromWindow(holder.getSurface());
                        setSolidBuffer(surfaceControl1, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setSolidBuffer(surfaceControl2, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.MAGENTA);

                        setZOrder(surfaceControl1, 1);
                        setZOrder(surfaceControl2, Integer.MIN_VALUE);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setOnComplete() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    private long mContext;

                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        long surfaceTransaction = createSurfaceTransaction();
                        setSolidBuffer(surfaceControl, surfaceTransaction, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        mContext = nSurfaceTransaction_setOnComplete(surfaceTransaction);
                        applyAndDeleteSurfaceTransaction(surfaceTransaction);
                    }
                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        super.surfaceDestroyed(holder);
                        nSurfaceTransaction_checkOnComplete(mContext, -1);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    @RequiresDevice // emulators can't support sync fences
    public void testSurfaceTransaction_setDesiredPresentTime_now() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    private long mContext;
                    private long mDesiredPresentTime;

                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        long surfaceTransaction = createSurfaceTransaction();
                        setSolidBuffer(surfaceControl, surfaceTransaction, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        mDesiredPresentTime = nSurfaceTransaction_setDesiredPresentTime(
                                surfaceTransaction, 0);
                        mContext = nSurfaceTransaction_setOnComplete(surfaceTransaction);
                        applyAndDeleteSurfaceTransaction(surfaceTransaction);
                    }
                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        super.surfaceDestroyed(holder);
                        nSurfaceTransaction_checkOnComplete(mContext, mDesiredPresentTime);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    @RequiresDevice // emulators can't support sync fences
    public void testSurfaceTransaction_setDesiredPresentTime_30ms() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    private long mContext;
                    private long mDesiredPresentTime;

                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        long surfaceTransaction = createSurfaceTransaction();
                        setSolidBuffer(surfaceControl, surfaceTransaction, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        mDesiredPresentTime = nSurfaceTransaction_setDesiredPresentTime(
                                surfaceTransaction, 30000000);
                        mContext = nSurfaceTransaction_setOnComplete(surfaceTransaction);
                        applyAndDeleteSurfaceTransaction(surfaceTransaction);
                    }
                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        super.surfaceDestroyed(holder);
                        nSurfaceTransaction_checkOnComplete(mContext, mDesiredPresentTime);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    @RequiresDevice // emulators can't support sync fences
    public void testSurfaceTransaction_setDesiredPresentTime_100ms() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    private long mContext;
                    private long mDesiredPresentTime;

                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        long surfaceTransaction = createSurfaceTransaction();
                        setSolidBuffer(surfaceControl, surfaceTransaction, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                        mDesiredPresentTime = nSurfaceTransaction_setDesiredPresentTime(
                                surfaceTransaction, 100000000);
                        mContext = nSurfaceTransaction_setOnComplete(surfaceTransaction);
                        applyAndDeleteSurfaceTransaction(surfaceTransaction);
                    }
                    @Override
                    public void surfaceDestroyed(SurfaceHolder holder) {
                        super.surfaceDestroyed(holder);
                        nSurfaceTransaction_checkOnComplete(mContext, mDesiredPresentTime);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBufferAlpha_1_0() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setBufferAlpha(surfaceControl, 1.0);
                    }
                },
                new PixelChecker(PixelColor.RED) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBufferAlpha_0_5() throws Throwable {
        BasicSurfaceHolderCallback callback = new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setBufferAlpha(surfaceControl, 0.5);
                    }
                };
        verifyTest(callback,
                new PixelChecker(PixelColor.YELLOW) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
        verifyTest(callback,
                new PixelChecker(PixelColor.RED) {
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount == 0;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setBufferAlpha_0_0() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long surfaceControl = createFromWindow(holder.getSurface());

                        setSolidBuffer(surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                                PixelColor.RED);
                        setBufferAlpha(surfaceControl, 0.0);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_reparent() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl1 = createFromWindow(holder.getSurface());
                        long parentSurfaceControl2 = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl1);

                        setGeometry(parentSurfaceControl1, 0, 0, 100, 100, 0, 0, 160, 480, 0);
                        setGeometry(parentSurfaceControl2, 0, 0, 100, 100, 160, 0, 640, 480, 0);

                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);

                        reparent(childSurfaceControl, parentSurfaceControl2);
                    }
                },
                new PixelChecker(PixelColor.RED) { //7500
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 6750 && pixelCount < 8250;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_reparent_null() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                    @Override
                    public void surfaceCreated(SurfaceHolder holder) {
                        long parentSurfaceControl = createFromWindow(holder.getSurface());
                        long childSurfaceControl = create(parentSurfaceControl);

                        setSolidBuffer(childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                                DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);

                        reparent(childSurfaceControl, 0);
                    }
                },
                new PixelChecker(PixelColor.YELLOW) { //10000
                    @Override
                    public boolean checkPixels(int pixelCount, int width, int height) {
                        return pixelCount > 9000 && pixelCount < 11000;
                    }
                });
    }

    @Test
    public void testSurfaceTransaction_setColor() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long surfaceControl = createFromWindow(holder.getSurface());

                    setColor(surfaceControl, 0, 1.0f, 0, 1.0f);
                }
            },
                new PixelChecker(PixelColor.GREEN) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_noColorNoBuffer() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long parentSurfaceControl = createFromWindow(holder.getSurface());
                    long childSurfaceControl = create(parentSurfaceControl);

                    setColor(parentSurfaceControl, 0, 1.0f, 0, 1.0f);
                }
            },
                new PixelChecker(PixelColor.GREEN) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_setColorAlpha() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long parentSurfaceControl = createFromWindow(holder.getSurface());
                    setColor(parentSurfaceControl, 0, 0, 1.0f, 0);
                }
            },
                new PixelChecker(PixelColor.YELLOW) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_setColorAndBuffer() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long surfaceControl = createFromWindow(holder.getSurface());

                    setSolidBuffer(
                            surfaceControl, DEFAULT_LAYOUT_WIDTH,
                            DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                    setColor(surfaceControl, 0, 1.0f, 0, 1.0f);
                }
            },
                new PixelChecker(PixelColor.RED) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_setColorAndBuffer_bufferAlpha_0_5() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long surfaceControl = createFromWindow(holder.getSurface());

                    setSolidBuffer(
                            surfaceControl, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                            PixelColor.RED);
                    setBufferAlpha(surfaceControl, 0.5);
                    setColor(surfaceControl, 0, 0, 1.0f, 1.0f);
                }
            },
                new PixelChecker(PixelColor.RED) {
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount == 0;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_setBufferNoColor_bufferAlpha_0() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long surfaceControlA = createFromWindow(holder.getSurface());
                    long surfaceControlB = createFromWindow(holder.getSurface());

                    setColor(surfaceControlA, 1.0f, 0, 0, 1.0f);
                    setSolidBuffer(surfaceControlB, DEFAULT_LAYOUT_WIDTH, DEFAULT_LAYOUT_HEIGHT,
                            PixelColor.TRANSPARENT);

                    setZOrder(surfaceControlA, 1);
                    setZOrder(surfaceControlB, 2);
                }
            },
                new PixelChecker(PixelColor.RED) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_setColorAndBuffer_hide() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long parentSurfaceControl = createFromWindow(holder.getSurface());
                    long childSurfaceControl = create(parentSurfaceControl);

                    setColor(parentSurfaceControl, 0, 1.0f, 0, 1.0f);

                    setSolidBuffer(
                            childSurfaceControl, DEFAULT_LAYOUT_WIDTH,
                            DEFAULT_LAYOUT_HEIGHT, PixelColor.RED);
                    setColor(childSurfaceControl, 0, 0, 1.0f, 1.0f);
                    setVisibility(childSurfaceControl, false);
                }
            },
                new PixelChecker(PixelColor.GREEN) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_zOrderMultipleSurfaces() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long surfaceControlA = createFromWindow(holder.getSurface());
                    long surfaceControlB = createFromWindow(holder.getSurface());

                    // blue color layer of A is above the green buffer and red color layer
                    // of B
                    setColor(surfaceControlA, 0, 0, 1.0f, 1.0f);
                    setSolidBuffer(
                            surfaceControlB, DEFAULT_LAYOUT_WIDTH,
                            DEFAULT_LAYOUT_HEIGHT, PixelColor.GREEN);
                    setColor(surfaceControlB, 1.0f, 0, 0, 1.0f);
                    setZOrder(surfaceControlA, 5);
                    setZOrder(surfaceControlB, 4);
                }
            },
                new PixelChecker(PixelColor.BLUE) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    @Test
    public void testSurfaceTransaction_zOrderMultipleSurfacesWithParent() throws Throwable {
        verifyTest(
                new BasicSurfaceHolderCallback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    long parentSurfaceControl = createFromWindow(holder.getSurface());
                    long surfaceControlA = create(parentSurfaceControl);
                    long surfaceControlB = create(parentSurfaceControl);

                    setColor(surfaceControlA, 0, 1.0f, 0, 1.0f);
                    setSolidBuffer(
                            surfaceControlA, DEFAULT_LAYOUT_WIDTH,
                            DEFAULT_LAYOUT_HEIGHT, PixelColor.GREEN);
                    setColor(surfaceControlB, 1.0f, 0, 0, 1.0f);
                    setZOrder(surfaceControlA, 3);
                    setZOrder(surfaceControlB, 4);
                }
            },
                new PixelChecker(PixelColor.RED) { // 10000
                @Override
                public boolean checkPixels(int pixelCount, int width, int height) {
                    return pixelCount > 9000 && pixelCount < 11000;
                }
            });
    }

    ///////////////////////////////////////////////////////////////////////////
    // Native function prototypes
    ///////////////////////////////////////////////////////////////////////////

    private static native long nSurfaceTransaction_create();
    private static native void nSurfaceTransaction_delete(long surfaceTransaction);
    private static native void nSurfaceTransaction_apply(long surfaceTransaction);
    private static native long nSurfaceControl_createFromWindow(Surface surface);
    private static native long nSurfaceControl_create(long surfaceControl);
    private static native void nSurfaceControl_release(long surfaceControl);
    private static native long nSurfaceTransaction_setSolidBuffer(
            long surfaceControl, long surfaceTransaction, int width, int height, int color);
    private static native long nSurfaceTransaction_setQuadrantBuffer(long surfaceControl,
            long surfaceTransaction, int width, int height, int colorTopLeft, int colorTopRight,
            int colorBottomRight, int colorBottomLeft);
    private static native void nSurfaceTransaction_releaseBuffer(long buffer);
    private static native void nSurfaceTransaction_setVisibility(
            long surfaceControl, long surfaceTransaction, boolean show);
    private static native void nSurfaceTransaction_setBufferOpaque(
            long surfaceControl, long surfaceTransaction, boolean opaque);
    private static native void nSurfaceTransaction_setGeometry(
            long surfaceControl, long surfaceTransaction, int srcRight, int srcTop, int srcLeft,
            int srcBottom, int dstRight, int dstTop, int dstLeft, int dstBottom, int transform);
    private static native void nSurfaceTransaction_setDamageRegion(
            long surfaceControl, long surfaceTransaction, int right, int top, int left, int bottom);
    private static native void nSurfaceTransaction_setZOrder(
            long surfaceControl, long surfaceTransaction, int z);
    private static native long nSurfaceTransaction_setOnComplete(long surfaceTransaction);
    private static native void nSurfaceTransaction_checkOnComplete(long context,
            long desiredPresentTime);
    private static native long nSurfaceTransaction_setDesiredPresentTime(long surfaceTransaction,
            long desiredPresentTimeOffset);
    private static native void nSurfaceTransaction_setBufferAlpha(long surfaceControl,
            long surfaceTransaction, double alpha);
    private static native void nSurfaceTransaction_reparent(long surfaceControl,
            long newParentSurfaceControl, long surfaceTransaction);
    private static native void nSurfaceTransaction_setColor(long surfaceControl,
            long surfaceTransaction, float r, float g, float b, float alpha);
}
