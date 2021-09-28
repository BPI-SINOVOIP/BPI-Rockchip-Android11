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

package android.uirendering.cts.testclasses;

import android.graphics.Bitmap;
import android.graphics.BlendMode;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.uirendering.cts.testinfrastructure.CanvasClient;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BlendModeTest extends ActivityTestBase {

    private static final int BG_COLOR = Color.WHITE;
    private static final int DST_COLOR = Color.RED;
    private static final int SRC_COLOR = Color.BLUE;

    private static final int LEFT_X = TEST_WIDTH / 4;
    private static final int RIGHT_X = TEST_WIDTH * 3 / 4;
    private static final int TOP_Y = TEST_HEIGHT / 4;
    private static final int BOTTOM_Y = TEST_HEIGHT * 3 / 4;

    private class BlendModeCanvasClient implements CanvasClient {
        final Paint mPaint = new Paint();

        private final Bitmap mSrcBitmap;
        private final Bitmap mDstBitmap;
        private final BlendMode mBlendmode;

        BlendModeCanvasClient(BlendMode mode, Bitmap dstBitmap, Bitmap srcBitmap) {
            mDstBitmap = dstBitmap;
            mSrcBitmap = srcBitmap;
            mBlendmode = mode;
        }

        @Override
        public void draw(Canvas canvas, int width, int height) {
            canvas.drawColor(Color.WHITE);

            int sc = canvas.saveLayer(0, 0, TEST_WIDTH, TEST_HEIGHT, null);

            canvas.drawBitmap(mDstBitmap, 0, 0, null);
            mPaint.setBlendMode(mBlendmode);

            canvas.drawBitmap(mSrcBitmap, 0, 0, mPaint);

            canvas.restoreToCount(sc);
        }
    }

    private static final Point[] SAMPLE_POINTS = {
        new Point(LEFT_X, TOP_Y),
        new Point(LEFT_X, BOTTOM_Y),
        new Point(RIGHT_X, BOTTOM_Y)
    };

    private void testBlendMode(BlendMode mode,
            int topLeftColor,
            int bottomLeftColor,
            int bottomRightColor) {

        BlendModeCanvasClient client = new BlendModeCanvasClient(mode,
                createBlendmodeDst(), createBlendmodeSrc());

        int[] colors = { topLeftColor, bottomLeftColor, bottomRightColor };
        createTest().addCanvasClient(client)
                .runWithVerifier(new SamplePointVerifier(SAMPLE_POINTS, colors));
    }

    private Bitmap createBlendmodeDst() {
        Bitmap srcB = Bitmap.createBitmap(TEST_WIDTH, TEST_HEIGHT, Bitmap.Config.ARGB_8888);
        Canvas srcCanvas = new Canvas(srcB);
        Paint srcPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        srcPaint.setColor(DST_COLOR);
        srcCanvas.drawRect(0, 0, TEST_WIDTH / 2, TEST_HEIGHT, srcPaint);
        return srcB;
    }

    private Bitmap createBlendmodeSrc() {
        Bitmap dstB = Bitmap.createBitmap(TEST_WIDTH, TEST_HEIGHT, Bitmap.Config.ARGB_8888);
        Canvas dstCanvas = new Canvas(dstB);
        Paint dstPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        dstPaint.setColor(SRC_COLOR);
        dstCanvas.drawRect(0, TEST_HEIGHT / 2, TEST_WIDTH, TEST_HEIGHT, dstPaint);
        return dstB;
    }

    @Test
    public void testBlendMode_CLEAR() {
        testBlendMode(BlendMode.CLEAR, Color.WHITE, Color.WHITE, Color.WHITE);
    }

    @Test
    public void testBlendMode_SRC() {
        testBlendMode(BlendMode.SRC, BG_COLOR, SRC_COLOR, SRC_COLOR);
    }

    @Test
    public void testBlendMode_DST() {
        testBlendMode(BlendMode.DST, DST_COLOR, DST_COLOR, BG_COLOR);
    }

    @Test
    public void testBlendMode_SRC_OVER() {
        testBlendMode(BlendMode.SRC_OVER, DST_COLOR, SRC_COLOR, SRC_COLOR);
    }

    @Test
    public void testBlendMode_DST_OVER() {
        testBlendMode(BlendMode.DST_OVER, Color.RED, Color.RED, Color.BLUE);
    }

    @Test
    public void testBlendMode_SRC_IN() {
        testBlendMode(BlendMode.SRC_IN, BG_COLOR, Color.BLUE, BG_COLOR);
    }

    @Test
    public void testBlendMode_DST_IN() {
        testBlendMode(BlendMode.DST_IN, BG_COLOR, DST_COLOR, BG_COLOR);
    }

    @Test
    public void testBlendMode_SRC_OUT() {
        testBlendMode(BlendMode.SRC_OUT, BG_COLOR, BG_COLOR, Color.BLUE);
    }

    @Test
    public void testBlendMode_DST_OUT() {
        testBlendMode(BlendMode.DST_OUT, DST_COLOR, BG_COLOR, BG_COLOR);
    }

    @Test
    public void testBlendMode_SRC_ATOP() {
        testBlendMode(BlendMode.SRC_ATOP, DST_COLOR, Color.BLUE, BG_COLOR);
    }

    @Test
    public void testBlendMode_DST_ATOP() {
        testBlendMode(BlendMode.DST_ATOP, BG_COLOR, DST_COLOR, Color.BLUE);
    }

    @Test
    public void testBlendMode_XOR() {
        testBlendMode(BlendMode.XOR, DST_COLOR, BG_COLOR, Color.BLUE);
    }

    @Test
    public void testBlendMode_PLUS() {
        testBlendMode(BlendMode.PLUS, DST_COLOR, Color.MAGENTA, Color.BLUE);
    }

    @Test
    public void testBlendMode_MODULATE() {
        int alpha = (Color.alpha(DST_COLOR) * Color.alpha(Color.BLUE)) / 255;
        int red = (Color.red(DST_COLOR) * Color.red(Color.BLUE)) / 255;
        int green = (Color.green(Color.GREEN) * Color.green(Color.BLUE)) / 255;
        int blue = (Color.blue(DST_COLOR) * Color.blue(Color.BLUE)) / 255;
        int resultColor = Color.argb(alpha, red, green, blue);
        testBlendMode(BlendMode.MODULATE, BG_COLOR, resultColor, BG_COLOR);
    }

    @Test
    public void testBlendMode_SCREEN() {
        testBlendMode(BlendMode.SCREEN, DST_COLOR, Color.MAGENTA, Color.BLUE);
    }

    @Test
    public void testBlendMode_OVERLAY() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alpha = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeOverlay(alphaSrc, alphaDst, redSrc, redDst);
        int green = computeOverlay(alphaSrc, alphaDst, greenSrc, greenDst);
        int blue = computeOverlay(alphaSrc, alphaDst, blueSrc, blueDst);
        int result = Color.argb(alpha, red, green, blue);
        testBlendMode(BlendMode.OVERLAY, DST_COLOR, result, SRC_COLOR);
    }

    private int computeOverlay(int alphaSrc, int alphaDst, int colorSrc, int colorDst) {
        if (2 * colorDst < alphaDst) {
            return (2 * colorSrc * colorDst) / 255;
        } else {
            return (alphaSrc * alphaDst) / 255
                    - (2 * (alphaDst - colorSrc) * (alphaSrc - colorDst));
        }
    }

    @Test
    public void testBlendMode_DARKEN() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeDarken(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeDarken(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeDarken(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.DARKEN, DST_COLOR, result, SRC_COLOR);
    }

    private int computeDarken(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        return (((255 - alphaDst)) * (colorSrc)) / 255 + ((255 - alphaSrc) * colorDst) / 255
                + Math.min(colorSrc, colorDst);
    }

    @Test
    public void testBlendMode_LIGHTEN() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeLighten(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeLighten(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeLighten(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.LIGHTEN, DST_COLOR, result, SRC_COLOR);
    }

    private int computeLighten(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        return (((255 - alphaDst)) * (colorSrc)) / 255 + ((255 - alphaSrc) * colorDst) / 255
                + Math.max(colorSrc, colorDst);
    }

    @Test
    public void testBlendMode_COLOR_DODGE() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeColorDodge(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeColorDodge(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeColorDodge(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.COLOR_DODGE, DST_COLOR, result, SRC_COLOR);

    }

    private int computeColorDodge(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        if (colorDst == 0) {
            return (colorSrc * (255 - alphaDst)) / 255;
        } else if (colorSrc == alphaSrc) {
            return colorSrc + alphaDst * (255 - alphaSrc) / 255;
        } else {
            float alphaRatio = (float) alphaSrc / ((float) alphaSrc - colorSrc);
            return Math.round((alphaSrc * Math.min(alphaDst, colorDst * alphaRatio)) / 255
                    + colorSrc * (255 - alphaDst) / 255.0f
                    + alphaDst * (255 - alphaSrc) / 255.0f);
        }
    }

    @Test
    public void testBlendMode_COLOR_BURN() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeColorBurn(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeColorBurn(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeColorBurn(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.COLOR_BURN, DST_COLOR, result, SRC_COLOR);
    }

    private int computeColorBurn(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        if (colorDst == alphaDst) {
            return colorDst + (colorSrc * (255 - alphaDst)) / 255;
        } else if (colorSrc == 0) {
            return alphaDst * (255 - alphaSrc) / 255;
        } else {
            return alphaSrc * (alphaDst - Math.min(alphaDst,
                    (alphaDst - colorDst) * alphaSrc / colorSrc))
                    + colorSrc * (255 - alphaDst) / 255 + alphaDst * (255 - alphaSrc) / 255;
        }
    }

    @Test
    public void testBlendMode_HARD_LIGHT() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeHardLight(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeHardLight(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeHardLight(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.HARD_LIGHT, DST_COLOR, result, SRC_COLOR);
    }

    private int computeHardLight(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        if (2 * colorSrc <= alphaSrc) {
            return 2 * colorSrc * colorDst / 255 + colorSrc * (255 - alphaDst) / 255
                    + colorDst * (255 - alphaSrc) / 255;
        } else {
            return colorSrc * (255 + alphaDst) / 255
                    + colorDst * (255 + alphaSrc) / 255
                    - (alphaSrc * alphaDst) / 255
                    - 2 * colorSrc * colorDst / 255;
        }
    }

    @Test
    public void testBlendMode_SOFT_LIGHT() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeSoftLight(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeSoftLight(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeSoftLight(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.SOFT_LIGHT, DST_COLOR, result, SRC_COLOR);
    }

    private int computeSoftLight(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        float m = alphaDst > 0 ? (float) colorDst / alphaDst : 0.0f;
        if (2 * colorSrc <= alphaSrc) {
            return Math.round(
                    colorDst * (alphaSrc + (2 * colorSrc - alphaSrc) * (1 - m)) / 255.0f
                            + colorSrc * (255 - alphaDst) / 255.0f
                            + (float) colorDst * (255 - alphaSrc) / 255.0f
            );
        } else if ((4 * colorDst) <= alphaDst) {
            return Math.round(
                    (alphaDst * (2 * colorSrc - alphaSrc)) / 255.0f
                            * (16 * m * m * m - 12 * m * m - 3 * m)
                            + colorSrc - (colorSrc * alphaDst) / 255.0f + colorDst
            );

        } else {
            return (int) Math.round(
                    (alphaDst * (2 * colorSrc - alphaSrc)) / 255.0f
                            * (Math.sqrt(m) - m)
                            + colorSrc - (colorSrc * alphaDst) / 255.0f + colorDst);
        }
    }

    @Test
    public void testBlendMode_DIFFERENCE() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeDifference(alphaDst, alphaSrc, redDst, redSrc);
        int green = computeDifference(alphaDst, alphaSrc, greenDst, greenSrc);
        int blue = computeDifference(alphaDst, alphaSrc, blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.DIFFERENCE, DST_COLOR, result, SRC_COLOR);
    }

    private int computeDifference(int alphaDst, int alphaSrc, int colorDst, int colorSrc) {
        return colorSrc + colorDst - 2 * Math.min(colorSrc * alphaDst, colorDst * alphaSrc) / 255;
    }

    @Test
    public void testBlendMode_EXCLUSION() {
        int alphaDst = Color.alpha(DST_COLOR);
        int alphaSrc = Color.alpha(SRC_COLOR);

        int redDst = Color.red(DST_COLOR);
        int redSrc = Color.red(SRC_COLOR);

        int greenDst = Color.green(DST_COLOR);
        int greenSrc = Color.green(SRC_COLOR);

        int blueDst = Color.blue(DST_COLOR);
        int blueSrc = Color.blue(SRC_COLOR);

        int alphaOut = (alphaSrc + alphaDst) - (alphaSrc * alphaDst) / 255;
        int red = computeExclusion(redDst, redSrc);
        int green = computeExclusion(greenDst, greenSrc);
        int blue = computeExclusion(blueDst, blueSrc);
        int result = Color.argb(alphaOut, red, green, blue);
        testBlendMode(BlendMode.EXCLUSION, DST_COLOR, result, SRC_COLOR);
    }

    private int computeExclusion(int colorDst, int colorSrc) {
        return colorSrc + colorDst - (2 * colorSrc * colorDst) / 255;
    }

    int computeMultiply(int dstColor, int srcColor, int dstAlpha, int srcAlpha) {
        return (srcColor * dstColor) / 255
                + (srcColor * (255 - dstAlpha)) / 255
                + (dstColor * (255 - srcAlpha)) / 255;
    }

    @Test
    public void testBlendMode_MULTIPLY() {
        int redAlpha = Color.alpha(Color.RED);
        int blueAlpha = Color.alpha(Color.BLUE);
        int alpha = (redAlpha + blueAlpha) - (redAlpha * blueAlpha) / 255;

        int dstRed = Color.red(DST_COLOR);
        int srcRed = Color.red(SRC_COLOR);

        int dstGreen = Color.green(DST_COLOR);
        int srcGreen = Color.green(SRC_COLOR);

        int dstBlue = Color.blue(DST_COLOR);
        int srcBlue = Color.blue(SRC_COLOR);

        int red = computeMultiply(dstRed, srcRed, redAlpha, blueAlpha);
        int green = computeMultiply(dstGreen, srcGreen, redAlpha, blueAlpha);
        int blue = computeMultiply(dstBlue, srcBlue, redAlpha, blueAlpha);
        int resultColor = Color.argb(alpha, red, green, blue);
        testBlendMode(BlendMode.MULTIPLY, DST_COLOR, resultColor, SRC_COLOR);
    }

    @Test
    public void testBlendMode_COLOR() {
        testBlendMode(BlendMode.COLOR, DST_COLOR, 0xFF3636FF, SRC_COLOR);
    }

    @Test
    public void testBlendMode_SATURATION() {
        testBlendMode(BlendMode.SATURATION, DST_COLOR, DST_COLOR, SRC_COLOR);
    }

    @Test
    public void testBlendMode_LUMINOSITY() {
        testBlendMode(BlendMode.LUMINOSITY, DST_COLOR, 0xFF5e0000, SRC_COLOR);
    }

    @Test
    public void testBlendMode_HUE() {
        testBlendMode(BlendMode.HUE, DST_COLOR, 0xFF3636FF, SRC_COLOR);
    }
}
