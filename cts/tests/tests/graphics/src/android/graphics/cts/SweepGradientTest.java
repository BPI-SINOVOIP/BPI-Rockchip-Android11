/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.graphics.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.util.Log;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ColorUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.function.Function;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SweepGradientTest {
    private static final int SIZE = 200;
    private static final int CENTER = SIZE / 2;
    private static final int RADIUS = 80;
    private static final int NUM_STEPS = 100;
    private static final int TOLERANCE = 10;

    private Paint mPaint;
    private Canvas mCanvas;
    private Bitmap mBitmap;

    @Before
    public void setup() {
        mPaint = new Paint();
        mBitmap = Bitmap.createBitmap(SIZE, SIZE, Config.ARGB_8888);
        mBitmap.eraseColor(Color.TRANSPARENT);
        mCanvas = new Canvas(mBitmap);
    }

    @Test
    public void test2Colors() {
        final int[] colors = new int[] { Color.GREEN, Color.RED };
        final float[] positions = new float[] { 0f, 1f };
        Shader shader = new SweepGradient(CENTER, CENTER, colors[0], colors[1]);
        mPaint.setShader(shader);
        mCanvas.drawRect(new Rect(0, 0, SIZE, SIZE), mPaint);
        verifyColors(colors, positions, TOLERANCE);
    }

    @Test
    public void testColorArray() {
        final int[] colors = new int[] { Color.GREEN, Color.RED, Color.BLUE };
        final float[] positions = new float[] { 0f, 0.3f, 1f };
        Shader shader = new SweepGradient(CENTER, CENTER, colors, positions);
        mPaint.setShader(shader);
        mCanvas.drawRect(new Rect(0, 0, SIZE, SIZE), mPaint);

        verifyColors(colors, positions, TOLERANCE);
    }

    @Test
    public void testMultiColor() {
        final int[] colors = new int[] { Color.GREEN, Color.RED, Color.BLUE, Color.GREEN };
        final float[] positions = new float[] { 0f, 0.25f, 0.5f, 1f };

        Shader shader = new SweepGradient(CENTER, CENTER, colors, positions);
        mPaint.setShader(shader);
        mCanvas.drawRect(new Rect(0, 0, SIZE, SIZE), mPaint);

        verifyColors(colors, positions, TOLERANCE);
    }

    private void verifyColors(int[] colors, float[] positions, int tolerance) {
        final double twoPi = Math.PI * 2;
        final double step = twoPi / NUM_STEPS;

        // exclude angle 0, which is not defined
        for (double rad = step; rad <= twoPi - step; rad += step) {
            int x = CENTER + (int)(Math.cos(rad) * RADIUS);
            int y = CENTER + (int)(Math.sin(rad) * RADIUS);

            float relPos = (float)(rad / twoPi);
            int idx;
            int color;
            for (idx = 0; idx < positions.length; idx++) {
                if (positions[idx] > relPos) {
                    break;
                }
            }
            if (idx == 0) {
                // use start color
                color = colors[0];
            } else if (idx == positions.length) {
                // clamp to end color
                color = colors[positions.length - 1];
            } else {
                // linear interpolation
                int i1 = idx - 1; // index of next lower color and position
                int i2 = idx; // index of next higher color and position
                double delta = (relPos - positions[i1]) / (positions[i2] - positions[i1]);
                int alpha = (int) ((1d - delta) * Color.alpha(colors[i1]) +
                        delta * Color.alpha(colors[i2]));
                int red = (int) ((1d - delta) * Color.red(colors[i1]) +
                        delta * Color.red(colors[i2]));
                int green = (int) ((1d - delta) * Color.green(colors[i1]) +
                        delta * Color.green(colors[i2]));
                int blue = (int) ((1d - delta) * Color.blue(colors[i1]) +
                        delta * Color.blue(colors[i2]));
                color = Color.argb(alpha, red, green, blue);
            }

            int pixel = mBitmap.getPixel(x, y);

            try {
                assertEquals(Color.alpha(color), Color.alpha(pixel), tolerance);
                assertEquals(Color.red(color), Color.red(pixel), tolerance);
                assertEquals(Color.green(color), Color.green(pixel), tolerance);
                assertEquals(Color.blue(color), Color.blue(pixel), tolerance);
            } catch (Error e) {
                Log.w(getClass().getName(), "rad=" + rad + ", x=" + x + ", y=" + y
                    + "pixel=" + Integer.toHexString(pixel) + ", color="
                    + Integer.toHexString(color));
                throw e;
            }
        }
    }

    @Test
    public void testZeroScaleMatrix() {
        SweepGradient gradient = new SweepGradient(1, 0.5f,
                new int[] {Color.BLUE, Color.RED, Color.BLUE}, null);
        Matrix m = new Matrix();
        m.setScale(0, 0);
        gradient.setLocalMatrix(m);

        Bitmap bitmap = Bitmap.createBitmap(2, 1, Config.ARGB_8888);
        bitmap.eraseColor(Color.BLACK);
        Canvas canvas = new Canvas(bitmap);

        Paint paint = new Paint();
        paint.setShader(gradient);
        canvas.drawPaint(paint);

        // red to left, blue to right
        ColorUtils.verifyColor(Color.BLACK, bitmap.getPixel(0, 0), 1);
        ColorUtils.verifyColor(Color.BLACK, bitmap.getPixel(1, 0), 1);
    }

    @Test(expected = NullPointerException.class)
    public void testNullColorInts() {
        int[] colors = null;
        new SweepGradient(1, 0.5f, colors, null);
    }

    @Test(expected = NullPointerException.class)
    public void testNullColorLongs() {
        long[] colors = null;
        new SweepGradient(1, 0.5f, colors, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNoColorInts() {
        new SweepGradient(1, 0.5f, new int[0], null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNoColorLongs() {
        new SweepGradient(1, 0.5f, new long[0], null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testOneColorInts() {
        new SweepGradient(1, 0.5f, new int[1], null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testOneColorLongs() {
        new SweepGradient(1, 0.5f, new long[1], null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchColorLongs() {
        long[] colors = new long[2];
        colors[0] = Color.pack(Color.BLUE);
        colors[1] = Color.pack(.5f, .5f, .5f, 1.0f, ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        new SweepGradient(1, 0.5f, colors, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchColorLongs2() {
        long color0 = Color.pack(Color.BLUE);
        long color1 = Color.pack(.5f, .5f, .5f, 1.0f, ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        new SweepGradient(1, 0.5f, color0, color1);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchPositionsInts() {
        new SweepGradient(1, 0.5f, new int[2], new float[3]);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchPositionsLongs() {
        new SweepGradient(1, 0.5f, new long[2], new float[3]);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLongs() {
        long[] colors = new long[2];
        colors[0] = -1L;
        colors[0] = -2L;
        new SweepGradient(1, 0.5f, colors, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLong() {
        new SweepGradient(1, 0.5f, -1L, Color.pack(Color.RED));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLong2() {
        new SweepGradient(1, 0.5f, Color.pack(Color.RED), -1L);
    }

    private String toString(double angle) {
        double factor = angle * Math.PI;
        return String.format("%.2f", angle) + "(pi)";
    }

    @Test
    public void testColorLong() {
        ColorSpace p3 = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
        long red = Color.pack(1, 0, 0, 1, p3);
        long blue = Color.pack(0, 0, 1, 1, p3);
        SweepGradient gradient = new SweepGradient(50, 50, red, blue);

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.RGBA_F16);
        bitmap.eraseColor(Color.TRANSPARENT);
        Canvas canvas = new Canvas(bitmap);

        Paint paint = new Paint();
        paint.setShader(gradient);
        canvas.drawPaint(paint);

        final ColorSpace bitmapColorSpace = bitmap.getColorSpace();
        Function<Long, Color> convert = (l) -> {
            return Color.valueOf(Color.convert(l, bitmapColorSpace));
        };

        Color lastColor = null;
        double lastAngle = 0;
        for (double angle = Math.PI / 8.0; angle < Math.PI * 2.0; angle += Math.PI / 8.0) {
            // currentColor is the Color at this angle.
            Color currentColor = null;
            double lastRadius = 0;
            for (double radius = 4; radius < 25; radius += 4) {
                double dx = Math.cos(angle) * radius;
                double dy = Math.sin(angle) * radius;
                int x = 50 + (int) (dx);
                int y = 50 + (int) (dy);
                Color c = bitmap.getColor(x, y);
                if (currentColor == null) {
                    // Checking the first radius at this angle.
                    currentColor = c;
                    if (lastColor == null) {
                        // This should be pretty close to the initial color.
                        ColorUtils.verifyColor("First color (at angle " + toString(angle)
                                + " and radius " + radius  + " should be mostly red",
                                convert.apply(red), c, .08f);
                        lastColor = currentColor;
                        lastAngle = angle;
                    } else {
                        assertTrue("Angle " + toString(angle)
                                + " should be less red than prior angle "
                                + toString(lastAngle), c.red() < lastColor.red());
                        assertTrue("Angle " + toString(angle)
                                + " should be more blue than prior angle "
                                + toString(lastAngle), c.blue() > lastColor.blue());
                    }
                } else {
                    // Already have a Color at this angle. This one should match.
                    ColorUtils.verifyColor("Radius " + radius + " at angle " + toString(angle)
                            + " should match same angle with radius " + lastRadius, currentColor,
                            c, .05f);
                }
                lastRadius = radius;
            }

            lastColor = currentColor;
            lastAngle = angle;
        }
    }

}
