/*
 * Copyright (C) 2008 The Android Open Source Project
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
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Shader.TileMode;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ColorUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.function.Function;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LinearGradientTest {
    @Test
    public void testLinearGradient() {
        Bitmap b;
        LinearGradient lg;
        int[] color = { Color.BLUE, Color.GREEN, Color.RED };
        float[] position = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f };

        lg = new LinearGradient(0, 0, 0, 40, color, position, TileMode.CLAMP);
        b = drawLinearGradient(lg, Config.ARGB_8888);

        // The pixels in same gradient line should be equivalent
        assertEquals(b.getPixel(10, 10), b.getPixel(20, 10));
        // BLUE -> GREEN, B sub-value decreasing while G sub-value increasing
        assertTrue(Color.blue(b.getPixel(10, 0)) > Color.blue(b.getPixel(10, 5)));
        assertTrue(Color.blue(b.getPixel(10, 5)) > Color.blue(b.getPixel(10, 10)));
        assertTrue(Color.green(b.getPixel(10, 0)) < Color.green(b.getPixel(10, 5)));
        assertTrue(Color.green(b.getPixel(10, 5)) < Color.green(b.getPixel(10, 10)));
        // GREEN -> RED, G sub-value decreasing while R sub-value increasing
        assertTrue(Color.green(b.getPixel(10, 15)) > Color.green(b.getPixel(10, 20)));
        assertTrue(Color.green(b.getPixel(10, 20)) > Color.green(b.getPixel(10, 25)));
        assertTrue(Color.red(b.getPixel(10, 15)) < Color.red(b.getPixel(10, 20)));
        assertTrue(Color.red(b.getPixel(10, 20)) < Color.red(b.getPixel(10, 25)));

        lg = new LinearGradient(0, 0, 0, 40, Color.RED, Color.BLUE, TileMode.CLAMP);
        b = drawLinearGradient(lg, Config.ARGB_8888);

        // The pixels in same gradient line should be equivalent
        assertEquals(b.getPixel(10, 10), b.getPixel(20, 10));
        // RED -> BLUE, R sub-value decreasing while B sub-value increasing
        assertTrue(Color.red(b.getPixel(10, 0)) > Color.red(b.getPixel(10, 15)));
        assertTrue(Color.red(b.getPixel(10, 15)) > Color.red(b.getPixel(10, 30)));
        assertTrue(Color.blue(b.getPixel(10, 0)) < Color.blue(b.getPixel(10, 15)));
        assertTrue(Color.blue(b.getPixel(10, 15)) < Color.blue(b.getPixel(10, 30)));
    }

    @Test
    public void testLinearGradientLong() {
        ColorSpace p3 = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
        long red = Color.pack(1, 0, 0, 1, p3);
        long green = Color.pack(0, 1, 0, 1, p3);
        long blue = Color.pack(0, 0, 1, 1, p3);
        long[] colors = new long[] { blue, green, red };
        float[] positions = null;

        LinearGradient lg = new LinearGradient(0, 0, 0, 40, colors, positions, TileMode.CLAMP);
        Bitmap b = drawLinearGradient(lg, Config.RGBA_F16);
        final ColorSpace bitmapColorSpace = b.getColorSpace();
        Function<Long, Color> convert = (l) -> {
            return Color.valueOf(Color.convert(l, bitmapColorSpace));
        };

        ColorUtils.verifyColor("Top-most color should be mostly blue!",
                convert.apply(blue), b.getColor(0, 0), 0.09f);

        ColorUtils.verifyColor("Middle color should be mostly green!",
                convert.apply(green), b.getColor(0, 20), 0.09f);

        ColorUtils.verifyColor("Bottom-most color should be mostly red!",
                convert.apply(red), b.getColor(0, 39), 0.08f);

        ColorUtils.verifyColor("The pixels in same gradient line should be equivalent!",
                b.getColor(10, 10), b.getColor(20, 10), 0f);
        // BLUE -> GREEN, B sub-value decreasing while G sub-value increasing
        assertTrue(b.getColor(10, 0).blue() > b.getColor(10, 5).blue());
        assertTrue(b.getColor(10, 5).blue() > b.getColor(10, 10).blue());
        assertTrue(b.getColor(10, 0).green() < b.getColor(10, 5).green());
        assertTrue(b.getColor(10, 5).green() < b.getColor(10, 10).green());
        // GREEN -> RED, G sub-value decreasing while R sub-value increasing
        assertTrue(b.getColor(10, 20).green() > b.getColor(10, 30).green());
        assertTrue(b.getColor(10, 30).green() > b.getColor(10, 35).green());
        assertTrue(b.getColor(10, 20).red() < b.getColor(10, 30).red());
        assertTrue(b.getColor(10, 30).red() < b.getColor(10, 35).red());

        lg = new LinearGradient(0, 0, 0, 40, red, blue, TileMode.CLAMP);
        b = drawLinearGradient(lg, Config.RGBA_F16);

        ColorUtils.verifyColor("Top-most color should be mostly red!",
                convert.apply(red), b.getColor(0, 0), .03f);

        ColorUtils.verifyColor("Bottom-most color should be mostly blue!",
                convert.apply(blue), b.getColor(0, 39), 0.016f);


        ColorUtils.verifyColor("The pixels in same gradient line should be equivalent!",
                b.getColor(10, 10), b.getColor(20, 10), 0f);
        // RED -> BLUE, R sub-value decreasing while B sub-value increasing
        assertTrue(b.getColor(10, 0).red() > b.getColor(10, 15).red());
        assertTrue(b.getColor(10, 15).red() > b.getColor(10, 30).red());
        assertTrue(b.getColor(10, 0).blue() < b.getColor(10, 15).blue());
        assertTrue(b.getColor(10, 15).blue() < b.getColor(10, 30).blue());
    }

    private Bitmap drawLinearGradient(LinearGradient lg, Config c) {
        Paint paint = new Paint();
        paint.setShader(lg);
        Bitmap b = Bitmap.createBitmap(40, 40, c);
        b.eraseColor(Color.BLACK);
        Canvas canvas = new Canvas(b);
        canvas.drawPaint(paint);
        return b;
    }

    @Test
    public void testZeroScaleMatrix() {
        LinearGradient gradient = new LinearGradient(0.5f, 0, 1.5f, 0,
                Color.RED, Color.BLUE, TileMode.CLAMP);
        Matrix m = new Matrix();
        m.setScale(0, 0);
        gradient.setLocalMatrix(m);
        Bitmap bitmap = drawLinearGradient(gradient, Config.ARGB_8888);

        ColorUtils.verifyColor(Color.BLACK, bitmap.getPixel(0, 0), 1);
        ColorUtils.verifyColor(Color.BLACK, bitmap.getPixel(20, 20), 1);
    }

    @Test(expected = NullPointerException.class)
    public void testNullColorInts() {
        int[] colors = null;
        new LinearGradient(0.5f, 0, 1.5f, 0, colors, null, TileMode.CLAMP);
    }

    @Test(expected = NullPointerException.class)
    public void testNullColorLongs() {
        long[] colors = null;
        new LinearGradient(0.5f, 0, 1.5f, 0, colors, null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNoColorInts() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new int[0], null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNoColorLongs() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new long[0], null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testOneColorInts() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new int[1], null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testOneColorLongs() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new long[1], null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchColorLongs() {
        long[] colors = new long[2];
        colors[0] = Color.pack(Color.BLUE);
        colors[1] = Color.pack(.5f, .5f, .5f, 1.0f, ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        new LinearGradient(0.5f, 0, 1.5f, 0, colors, null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchColorLongs2() {
        long color0 = Color.pack(Color.BLUE);
        long color1 = Color.pack(.5f, .5f, .5f, 1.0f, ColorSpace.get(ColorSpace.Named.DISPLAY_P3));
        new LinearGradient(0.5f, 0, 1.5f, 0, color0, color1, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchPositionsInts() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new int[2], new float[3], TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testMismatchPositionsLongs() {
        new LinearGradient(0.5f, 0, 1.5f, 0, new long[2], new float[3], TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLongs() {
        long[] colors = new long[2];
        colors[0] = -1L;
        colors[0] = -2L;
        new LinearGradient(0.5f, 0, 1.5f, 0, colors, null, TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLong() {
        new LinearGradient(0.5f, 0, 1.5f, 0, -1L, Color.pack(Color.RED), TileMode.CLAMP);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testInvalidColorLong2() {
        new LinearGradient(0.5f, 0, 1.5f, 0, Color.pack(Color.RED), -1L, TileMode.CLAMP);
    }
}
