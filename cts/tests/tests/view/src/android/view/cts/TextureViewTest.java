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

import static android.opengl.GLES20.GL_COLOR_BUFFER_BIT;
import static android.opengl.GLES20.GL_SCISSOR_TEST;
import static android.opengl.GLES20.glClear;
import static android.opengl.GLES20.glClearColor;
import static android.opengl.GLES20.glEnable;
import static android.opengl.GLES20.glScissor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.Half;
import android.view.PixelCopy;
import android.view.TextureView;
import android.view.View;
import android.view.Window;

import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SynchronousPixelCopy;
import com.android.compatibility.common.util.WidgetTestUtils;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.TimeoutException;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class TextureViewTest {

    static final int EGL_GL_COLORSPACE_SRGB_KHR = 0x3089;
    static final int EGL_GL_COLORSPACE_LINEAR_KHR = 0x308A;
    static final int EGL_GL_COLORSPACE_DISPLAY_P3_EXT = 0x3363;
    static final int EGL_GL_COLORSPACE_DISPLAY_P3_LINEAR_EXT = 0x3362;
    static final int EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT = 0x3490;
    static final int EGL_GL_COLORSPACE_SCRGB_EXT = 0x3351;
    static final int EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT = 0x3350;

    @Rule
    public ActivityTestRule<TextureViewCtsActivity> mActivityRule =
            new ActivityTestRule<>(TextureViewCtsActivity.class, false, false);

    @Test
    public void testFirstFrames() throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        activity.waitForEnterAnimationComplete();

        final Point center = new Point();
        final Window[] windowRet = new Window[1];
        mActivityRule.runOnUiThread(() -> {
            View content = activity.findViewById(android.R.id.content);
            int[] outLocation = new int[2];
            content.getLocationInWindow(outLocation);
            center.x = outLocation[0] + (content.getWidth() / 2);
            center.y = outLocation[1] + (content.getHeight() / 2);
            windowRet[0] = activity.getWindow();
        });
        final Window window = windowRet[0];
        assertTrue(center.x > 0);
        assertTrue(center.y > 0);
        waitForColor(window, center, Color.WHITE);
        activity.waitForSurface();
        activity.initGl();
        int updatedCount;
        updatedCount = activity.waitForSurfaceUpdateCount(0);
        assertEquals(0, updatedCount);
        activity.drawColor(Color.GREEN);
        updatedCount = activity.waitForSurfaceUpdateCount(1);
        assertEquals(1, updatedCount);
        assertEquals(Color.WHITE, getPixel(window, center));
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule,
                activity.findViewById(android.R.id.content), () -> activity.removeCover());

        int color = waitForChange(window, center, Color.WHITE);
        assertEquals(Color.GREEN, color);
        activity.drawColor(Color.BLUE);
        updatedCount = activity.waitForSurfaceUpdateCount(2);
        assertEquals(2, updatedCount);
        color = waitForChange(window, center, color);
        assertEquals(Color.BLUE, color);
    }

    @Test
    public void testScaling() throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        activity.drawFrame(TextureViewTest::drawGlQuad);
        final Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mActivityRule.runOnUiThread(() -> {
            activity.getTextureView().getBitmap(bitmap);
        });
        PixelCopyTest.assertBitmapQuadColor(bitmap,
                Color.RED, Color.GREEN, Color.BLUE, Color.BLACK);
    }

    @Test
    public void testRotateScale() throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        final TextureView textureView = activity.getTextureView();
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, activity.getTextureView(), null);
        Matrix rotate = new Matrix();
        rotate.setRotate(180, textureView.getWidth() / 2, textureView.getHeight() / 2);
        activity.drawFrame(rotate, TextureViewTest::drawGlQuad);
        final Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mActivityRule.runOnUiThread(() -> {
            activity.getTextureView().getBitmap(bitmap);
        });
        // Verify the matrix did not rotate content of getTextureView.getBitmap().
        PixelCopyTest.assertBitmapQuadColor(bitmap,
                Color.RED, Color.GREEN, Color.BLUE, Color.BLACK);

        // Remove cover and calculate TextureView position on the screen.
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule,
                activity.findViewById(android.R.id.content), () -> activity.removeCover());
        final Rect viewPos = new Rect();
        mActivityRule.runOnUiThread(() -> {
            int[] outLocation = new int[2];
            textureView.getLocationInWindow(outLocation);
            viewPos.left = outLocation[0];
            viewPos.top = outLocation[1];
            viewPos.right = viewPos.left + textureView.getWidth();
            viewPos.bottom = viewPos.top + textureView.getHeight();
        });

        // Capture the portion of the screen that contains the texture view only.
        Window window = activity.getWindow();
        Bitmap screenshot = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        int result = new SynchronousPixelCopy().request(window, viewPos, screenshot);
        assertEquals("Copy request failed", PixelCopy.SUCCESS, result);
        // Verify the matrix rotated the TextureView content drawn on the screen.
        PixelCopyTest.assertBitmapQuadColor(screenshot,
                Color.BLACK, Color.BLUE, Color.GREEN, Color.RED);
    }

    @Test
    public void testTransformScale() throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        final TextureView textureView = activity.getTextureView();
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, activity.getTextureView(), null);
        Matrix transform = new Matrix();
        final float translateY = 100.0f;
        final float scaleY = 0.25f;
        float[] values = {1, 0, 0, 0, scaleY, translateY, 0, 0, 1};
        transform.setValues(values);
        activity.drawFrame(transform, TextureViewTest::drawGlQuad);
        final Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        mActivityRule.runOnUiThread(() -> {
            activity.getTextureView().getBitmap(bitmap);
        });
        // Verify the matrix did not affect the content of getTextureView.getBitmap().
        PixelCopyTest.assertBitmapQuadColor(bitmap,
                Color.RED, Color.GREEN, Color.BLUE, Color.BLACK);

        // Remove cover and calculate TextureView position on the screen.
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule,
                activity.findViewById(android.R.id.content), () -> activity.removeCover());
        final Rect viewPos = new Rect();
        mActivityRule.runOnUiThread(() -> {
            int[] outLocation = new int[2];
            textureView.getLocationInWindow(outLocation);
            viewPos.left = outLocation[0];
            viewPos.top = outLocation[1];
            viewPos.right = viewPos.left + textureView.getWidth();
            viewPos.bottom = viewPos.top + textureView.getHeight();
        });

        // Capture the portion of the screen that contains the texture view only.
        Window window = activity.getWindow();
        Bitmap screenshot = Bitmap.createBitmap(viewPos.width(), viewPos.height(),
                Bitmap.Config.ARGB_8888);
        int result = new SynchronousPixelCopy().request(window, viewPos, screenshot);
        assertEquals("Copy request failed", PixelCopy.SUCCESS, result);
        // Verify the matrix scaled and translated the TextureView content drawn on the screen.
        // "texturePos" has SurfaceTexture position inside the TextureView.
        final Rect texturePos = new Rect(0, (int) translateY, viewPos.width(),
                (int) (viewPos.height() * scaleY + translateY));

        // Areas not covered by the texture are black, because FrameLayout background is set to
        // Color.BLACK in TextureViewCtsActivity.onCreate.
        assertEquals("above texture", Color.BLACK,
                screenshot.getPixel(10, texturePos.top - 10));
        assertEquals("below texture", Color.BLACK,
                screenshot.getPixel(10, texturePos.bottom + 10));
        assertEquals("top left", Color.RED,
                screenshot.getPixel(texturePos.left + 10, texturePos.top + 10));
        assertEquals("top right", Color.GREEN,
                screenshot.getPixel(texturePos.right - 10, texturePos.top + 10));
        assertEquals("Bottom left", Color.BLUE,
                screenshot.getPixel(texturePos.left + 10, texturePos.bottom - 10));
        assertEquals("Bottom right", Color.BLACK,
                screenshot.getPixel(texturePos.right - 10, texturePos.bottom - 10));
    }

    @Test
    public void testGetBitmap_8888_P3() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_DISPLAY_P3_EXT, ColorSpace.get(ColorSpace.Named.DISPLAY_P3),
                false, false, new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGetBitmap_8888_PassthroughP3() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT,
                ColorSpace.get(ColorSpace.Named.DISPLAY_P3), false, true,
                new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGetBitmap_FP16_PassthroughP3() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT,
                ColorSpace.get(ColorSpace.Named.DISPLAY_P3), true, true,
                new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGetBitmap_FP16_LinearP3() throws Throwable {
        ColorSpace.Rgb displayP3 = (ColorSpace.Rgb) ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
        ColorSpace.Rgb linearDisplayP3 = new ColorSpace.Rgb(
                "Display P3 Linear",
                displayP3.getTransform(),
                displayP3.getWhitePoint(),
                x -> x,
                x -> x,
                0.0f, 1.0f
        );

        testGetBitmap(EGL_GL_COLORSPACE_DISPLAY_P3_LINEAR_EXT, linearDisplayP3, true,
                true, new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGetBitmap_FP16_ExtendedSRGB() throws Throwable {
        // isLinear is "true", because the spec says
        // GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING is GL_LINEAR for EGL_GL_COLORSPACE_SCRGB_EXT.
        // See https://www.khronos.org/registry/EGL/extensions/EXT/EGL_EXT_gl_colorspace_scrgb.txt.
        testGetBitmap(EGL_GL_COLORSPACE_SCRGB_EXT,
                ColorSpace.get(ColorSpace.Named.EXTENDED_SRGB), true,
                true, new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGetBitmap_FP16_LinearExtendedSRGB() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT,
                ColorSpace.get(ColorSpace.Named.LINEAR_EXTENDED_SRGB), true,
                true, new FP16Compare(ColorSpace.Named.EXTENDED_SRGB));
    }

    @Test
    public void testGet565Bitmap_SRGB() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_SRGB_KHR, ColorSpace.get(ColorSpace.Named.SRGB),
                false, false, new SRGBCompare(Bitmap.Config.RGB_565));
    }

    @Test
    public void testGetBitmap_SRGB() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_SRGB_KHR, ColorSpace.get(ColorSpace.Named.SRGB),
                false, false, new SRGBCompare(Bitmap.Config.ARGB_8888));
    }

    @Test
    public void testGetBitmap_SRGBLinear() throws Throwable {
        testGetBitmap(EGL_GL_COLORSPACE_LINEAR_KHR, ColorSpace.get(ColorSpace.Named.LINEAR_SRGB),
                false, true, new SRGBCompare(Bitmap.Config.ARGB_8888));
    }

    /**
     *  Test that verifies TextureView is drawn with bilerp sampling, when the matrix is not
     *  an integer translate or identity.
     */
    @Test
    public void testSamplingWithTransform() throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        final TextureView textureView = activity.getTextureView();
        final int viewWidth = textureView.getWidth();
        final int viewHeight = textureView.getHeight();
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, activity.getTextureView(), null);
        // Remove cover and calculate TextureView position on the screen.
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule,
                activity.findViewById(android.R.id.content), () -> activity.removeCover());

        float[][] matrices = {
            {1, 0, 0, 0, 1, 0, 0, 0, 1},            // identity matrix
            {1, 0, 0, 0, 1, 10.3f, 0, 0, 1},        // translation matrix with a fractional offset
            {1, 0, 0, 0, 0.75f, 0, 0, 0, 1},        // scaling matrix
            {1, 0, 0, 0, 1, 10f, 0, 0, 1},          // translation matrix with an integer offset
            {0, -1, viewWidth, 1, 0, 0, 0, 0, 1},   // 90 rotation matrix + integer translate X
            {0, 1, 0, -1, 0, viewWidth, 0, 0, 1},   // 270 rotation matrix + integer translate Y
            {-1, 0, viewWidth, 0, 1, 0, 0, 0, 1},   // H flip matrix + integer translate X
            {1, 0, 0, 0, -1, viewHeight, 0, 0, 1},  // V flip matrix + integer translate Y
            {-1, 0, viewWidth, 0, -1, viewHeight, 0, 0, 1}, // 180 rotation + integer translate X Y
            {0, -1, viewWidth - 10.3f, 1, 0, 0, 0, 0, 1},  // 90 rotation matrix with a fractional
                                                           // offset
        };
        boolean[] nearestSampling = {
            true,  // nearest sampling for identity
            false, // bilerp sampling for fractional translate
            false, // bilerp sampling for scaling
            true,  // nearest sampling for integer translate
            true,  // nearest sampling for 90 rotation with integer translate
            true,  // nearest sampling for 270 rotation with integer translate
            true,  // nearest sampling for H flip with integer translate
            true,  // nearest sampling for V flip with integer translate
            true,  // nearest sampling for 180 rotation with integer translate
            false, // bilerp sampling for 90 rotation matrix with a fractional offset
        };
        for (int i = 0; i < nearestSampling.length; i++) {

            Matrix transform = new Matrix();
            transform.setValues(matrices[i]);

            // Test draws a set of black & white alternating lines.
            activity.drawFrame(transform, TextureViewTest::drawGlBlackWhiteLines);

            final Rect viewPos = new Rect();
            mActivityRule.runOnUiThread(() -> {
                int[] outLocation = new int[2];
                textureView.getLocationInWindow(outLocation);
                viewPos.left = outLocation[0];
                viewPos.top = outLocation[1];
                viewPos.right = viewPos.left + textureView.getWidth();
                viewPos.bottom = viewPos.top + textureView.getHeight();
            });

            // Capture the portion of the screen that contains the texture view only.
            Window window = activity.getWindow();
            Bitmap screenshot = Bitmap.createBitmap(viewPos.width(), viewPos.height(),
                    Bitmap.Config.ARGB_8888);
            int result = new SynchronousPixelCopy().request(window, viewPos, screenshot);
            assertEquals("Copy request failed", PixelCopy.SUCCESS, result);

            // "texturePos" has SurfaceTexture position inside the TextureView.
            RectF texturePosF = new RectF(0, 0, viewPos.width(), viewPos.height());
            transform.mapRect(texturePosF);
            // Clip parts outside TextureView.
            // Matrices are picked, so that the drawing area is not empty.
            assertTrue("empty test area",
                    texturePosF.intersect(0, 0, viewPos.width(), viewPos.height()));
            Rect texturePos = new Rect((int) Math.ceil(texturePosF.left),
                    (int) Math.ceil(texturePosF.top), (int) Math.floor(texturePosF.right),
                    (int) Math.floor(texturePosF.bottom));

            int[] pixels = new int[texturePos.width() * texturePos.height()];
            screenshot.getPixels(pixels, 0, texturePos.width(), texturePos.left, texturePos.top,
                    texturePos.width(), texturePos.height());

            boolean success = true;
            int failPosition = 0;
            if (nearestSampling[i]) {
                // Check all pixels are either black or white.
                for (int j = 0; j < pixels.length; j++) {
                    if (pixels[j] != Color.BLACK && pixels[j] != Color.WHITE) {
                        success = false;
                        failPosition = j;
                        break;
                    }
                }
            } else {
                // Check that a third of pixels are not black nor white, because bilerp sampling
                // changed pure black/white to a variety of gray intermediates.
                int nonBlackWhitePixels = 0;
                for (int j = 0; j < pixels.length; j++) {
                    if (pixels[j] != Color.BLACK && pixels[j] != Color.WHITE) {
                        nonBlackWhitePixels++;
                    } else {
                        failPosition = j;
                    }
                }
                if (nonBlackWhitePixels < pixels.length / 3) {
                    success = false;
                }
            }
            assertTrue("Unexpected color at position " + failPosition + " = "
                    + Integer.toHexString(pixels[failPosition]) + " " + transform.toString(),
                    success);
        }
    }

    interface CompareFunction {
        Bitmap.Config getConfig();
        ColorSpace getColorSpace();
        void verify(float[] srcColor, ColorSpace srcColorSpace, Bitmap dstBitmap);
    }

    private class FP16Compare implements CompareFunction {
        private ColorSpace mDstColorSpace;

        FP16Compare(ColorSpace.Named namedCS) {
            mDstColorSpace = ColorSpace.get(namedCS);
        }

        public Bitmap.Config getConfig() {
            return Bitmap.Config.RGBA_F16;
        }

        public ColorSpace getColorSpace() {
            return mDstColorSpace;
        }

        public void verify(float[] srcColor, ColorSpace srcColorSpace, Bitmap dstBitmap) {
            // read pixels into buffer and compare using colorspace connector
            ByteBuffer buffer = ByteBuffer.allocate(dstBitmap.getAllocationByteCount());
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            dstBitmap.copyPixelsToBuffer(buffer);
            Half alpha = Half.valueOf(buffer.getShort(6));
            assertEquals(1.0f, alpha.floatValue(), 0.0f);

            final ColorSpace dstSpace = getColorSpace();
            float[] expectedColor = ColorSpace.connect(srcColorSpace, dstSpace).transform(srcColor);
            float[] outputColor = {
                    Half.valueOf(buffer.getShort(0)).floatValue(),
                    Half.valueOf(buffer.getShort(2)).floatValue(),
                    Half.valueOf(buffer.getShort(4)).floatValue() };

            assertEquals(expectedColor[0], outputColor[0], 0.01f);
            assertEquals(expectedColor[1], outputColor[1], 0.01f);
            assertEquals(expectedColor[2], outputColor[2], 0.01f);
        }
    }

    private class SRGBCompare implements CompareFunction {
        private Bitmap.Config mConfig;

        SRGBCompare(Bitmap.Config config) {
            mConfig = config;
        }

        public Bitmap.Config getConfig() {
            return mConfig;
        }

        public ColorSpace getColorSpace() {
            return ColorSpace.get(ColorSpace.Named.SRGB);
        }

        public void verify(float[] srcColor, ColorSpace srcColorSpace, Bitmap dstBitmap) {
            int color = dstBitmap.getPixel(0, 0);
            assertEquals(1.0f, Color.alpha(color) / 255.0f, 0.0f);
            assertEquals(srcColor[0], Color.red(color) / 255.0f, 0.01f);
            assertEquals(srcColor[1], Color.green(color) / 255.0f, 0.01f);
            assertEquals(srcColor[2], Color.blue(color) / 255.0f, 0.01f);
        }
    }

    // isFramebufferLinear is true, when GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING is GL_LINEAR.
    // It is false, when GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING is GL_SRGB.
    private void testGetBitmap(int eglColorSpace, ColorSpace colorSpace,
            boolean useHalfFloat, boolean isFramebufferLinear,
            CompareFunction compareFunction) throws Throwable {
        final TextureViewCtsActivity activity = mActivityRule.launchActivity(null);
        activity.waitForSurface();

        try {
            activity.initGl(eglColorSpace, useHalfFloat);
        } catch (RuntimeException e) {
            // failure to init GL with the right colorspace is not a TextureView failure as some
            // devices may not support 16-bits or the colorspace extension
            if (!activity.initGLExtensionUnsupported()) {
                fail("Unable to initGL : " + e);
            }
            return;
        }

        final float[] inputColor = { 1.0f, 128 / 255.0f, 0.0f};

        int updatedCount;
        updatedCount = activity.waitForSurfaceUpdateCount(0);
        assertEquals(0, updatedCount);
        activity.drawColor(inputColor[0], inputColor[1], inputColor[2], 1.0f);
        updatedCount = activity.waitForSurfaceUpdateCount(1);
        assertEquals(1, updatedCount);

        final Bitmap bitmap = activity.getContents(compareFunction.getConfig(),
                compareFunction.getColorSpace());

        // If GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING is GL_SRGB, then glClear will treat the input
        // color as linear and write a converted sRGB color into the framebuffer.
        if (isFramebufferLinear) {
            compareFunction.verify(inputColor, colorSpace, bitmap);
        } else {
            ColorSpace.Connector connector;
            connector = ColorSpace.connect(ColorSpace.get(ColorSpace.Named.LINEAR_SRGB),
                    ColorSpace.get(ColorSpace.Named.SRGB));
            float[] outputColor = connector.transform(inputColor);
            compareFunction.verify(outputColor, colorSpace, bitmap);
        }
    }

    private static void drawGlQuad(int width, int height) {
        int cx = width / 2;
        int cy = height / 2;

        glEnable(GL_SCISSOR_TEST);

        glScissor(0, cy, cx, height - cy);
        clearColor(Color.RED);

        glScissor(cx, cy, width - cx, height - cy);
        clearColor(Color.GREEN);

        glScissor(0, 0, cx, cy);
        clearColor(Color.BLUE);

        glScissor(cx, 0, width - cx, cy);
        clearColor(Color.BLACK);
    }

    private static void drawGlBlackWhiteLines(int width, int height) {
        final int lineHeight = 1;
        glEnable(GL_SCISSOR_TEST);
        for (int y = 0; y < height / lineHeight; y++) {
            glScissor(0, lineHeight * y, width, lineHeight);
            clearColor((y % 2 == 0) ? Color.BLACK : Color.WHITE);
        }
    }

    private static void clearColor(int color) {
        glClearColor(Color.red(color) / 255.0f,
                Color.green(color) / 255.0f,
                Color.blue(color) / 255.0f,
                Color.alpha(color) / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    private int getPixel(Window window, Point point) {
        Bitmap screenshot = Bitmap.createBitmap(window.getDecorView().getWidth(),
                window.getDecorView().getHeight(), Bitmap.Config.ARGB_8888);
        int result = new SynchronousPixelCopy().request(window, screenshot);
        assertEquals("Copy request failed", PixelCopy.SUCCESS, result);
        int pixel = screenshot.getPixel(point.x, point.y);
        screenshot.recycle();
        return pixel;
    }

    private void waitForColor(Window window, Point point, int color)
            throws InterruptedException, TimeoutException {
        for (int i = 0; i < 20; i++) {
            int pixel = getPixel(window, point);
            if (pixel == color) {
                return;
            }
            Thread.sleep(16);
        }
        throw new TimeoutException();
    }

    private int waitForChange(Window window, Point point, int color)
            throws InterruptedException, TimeoutException {
        for (int i = 0; i < 30; i++) {
            int pixel = getPixel(window, point);
            if (pixel != color) {
                return pixel;
            }
            Thread.sleep(16);
        }
        throw new TimeoutException();
    }
}
