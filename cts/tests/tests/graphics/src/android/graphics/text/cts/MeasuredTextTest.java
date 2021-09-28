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

package android.graphics.text.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.text.MeasuredText;
import android.text.PrecomputedText;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class MeasuredTextTest {
    private static Paint sPaint;

    @BeforeClass
    public static void classSetUp() {
        sPaint = new Paint();
        Context context = InstrumentationRegistry.getTargetContext();
        AssetManager am = context.getAssets();
        Typeface tf = new Typeface.Builder(am, "fonts/layout/linebreak.ttf").build();
        sPaint.setTypeface(tf);
        sPaint.setTextSize(10.0f);  // Make 1em = 10px
    }

    @Test
    public void testBuilder() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
    }

    @Test
    public void testBuilder_FromExistingMeasuredText() {
        String text = "Hello, World";
        final MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        assertNotNull(new MeasuredText.Builder(mt)
                .appendStyleRun(sPaint, text.length(), true /* isRtl */).build());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_FromExistingMeasuredText_differentLayoutParam() {
        String text = "Hello, World";
        final MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .setComputeLayout(false)
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        new MeasuredText.Builder(mt)
                .appendStyleRun(sPaint, text.length(), true /* isRtl */).build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_FromExistingMeasuredText_differentHyphenationParam() {
        String text = "Hello, World";
        final MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .setComputeHyphenation(false)
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        new MeasuredText.Builder(mt)
                .setComputeHyphenation(true)
                .appendStyleRun(sPaint, text.length(), true /* isRtl */).build();
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_NullText() {
        new MeasuredText.Builder((char[]) null);
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_NullMeasuredText() {
        new MeasuredText.Builder((MeasuredText) null);
    }

    @Test(expected = NullPointerException.class)
    public void testBuilder_NullPaint() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray()).appendStyleRun(null, text.length(), false);
    }

    @Test
    public void testGetWidth() {
        String text = "Hello, World";
        MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        assertEquals(0.0f, mt.getWidth(0, 0), 0.0f);
        assertEquals(10.0f, mt.getWidth(0, 1), 0.0f);
        assertEquals(20.0f, mt.getWidth(0, 2), 0.0f);
        assertEquals(10.0f, mt.getWidth(1, 2), 0.0f);
        assertEquals(20.0f, mt.getWidth(1, 3), 0.0f);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetWidth_StartSmallerThanZero() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getWidth(-1, 0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetWidth_StartLargerThanLength() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getWidth(text.length() + 1, 0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetWidth_EndSmallerThanZero() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getWidth(0, -1);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetWidth_EndLargerThanLength() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getWidth(0, text.length() + 1);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetWidth_StartLargerThanEnd() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getWidth(1, 0);
    }

    @Test
    public void testGetBounds() {
        String text = "Hello, World";
        MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        final Rect emptyRect = new Rect(0, 0, 0, 0);
        final Rect singleCharRect = new Rect(0, -10, 10, 0);
        final Rect twoCharRect = new Rect(0, -10, 20, 0);
        Rect out = new Rect();
        mt.getBounds(0, 0, out);
        assertEquals(emptyRect, out);
        mt.getBounds(0, 1, out);
        assertEquals(singleCharRect, out);
        mt.getBounds(0, 2, out);
        assertEquals(twoCharRect, out);
        mt.getBounds(1, 2, out);
        assertEquals(singleCharRect, out);
        mt.getBounds(1, 3, out);
        assertEquals(twoCharRect, out);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetBounds_StartSmallerThanZero() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(-1, 0, rect);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetBounds_StartLargerThanLength() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(text.length() + 1, 0, rect);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetBounds_EndSmallerThanZero() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(0, -1, rect);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetBounds_EndLargerThanLength() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(0, text.length() + 1, rect);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetBounds_StartLargerThanEnd() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(1, 0, rect);
    }

    @Test(expected = NullPointerException.class)
    public void testGetBounds_NullRect() {
        String text = "Hello, World";
        Rect rect = new Rect();
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getBounds(0, 0, null);
    }

    @Test
    public void testGetCharWidthAt() {
        String text = "Hello, World";
        MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */).build();
        assertEquals(10.0f, mt.getCharWidthAt(0), 0.0f);
        assertEquals(10.0f, mt.getCharWidthAt(1), 0.0f);
        assertEquals(10.0f, mt.getCharWidthAt(2), 0.0f);
        assertEquals(10.0f, mt.getCharWidthAt(3), 0.0f);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetCharWidthAt_OffsetSmallerThanZero() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getCharWidthAt(-1);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testGetCharWidthAt_OffsetLargerThanLength() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */)
                .build()
                .getCharWidthAt(text.length());
    }

    @Test(expected = IllegalStateException.class)
    public void testBuilder_reuse_throw_exception() {
        String text = "Hello, World";
        MeasuredText.Builder b = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false /* isRtl */);
        b.build();
        b.build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_tooSmallLengthStyle() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray()).appendStyleRun(sPaint, -1, false /* isRtl */);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_tooLargeLengthStyle() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length() + 1, false /* isRtl */);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_tooSmallLengthReplacement() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray()).appendReplacementRun(sPaint, -1, 1.0f);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_tooLargeLengthReplacement() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendReplacementRun(sPaint, text.length() + 1, 1.0f);
    }

    @Test(expected = IllegalStateException.class)
    public void testBuilder_notEnoughStyle() {
        String text = "Hello, World";
        new MeasuredText.Builder(text.toCharArray())
                .appendReplacementRun(sPaint, text.length() - 1, 1.0f).build();
    }

    public Bitmap makeBitmapFromSsb(String text, TextPaint paint, boolean isRtl) {
        // drawTetRun is not aware of multiple style text.
        final SpannableStringBuilder ssb = new SpannableStringBuilder(text);
        final Bitmap ssbBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Canvas ssbCanvas = new Canvas(ssbBitmap);
        ssbCanvas.save();
        ssbCanvas.translate(0, 0);
        ssbCanvas.drawTextRun(ssb, 0, ssb.length(), 0, ssb.length(),
                0.0f /* x */, 240.0f /* y */, isRtl, paint);
        ssbCanvas.restore();
        return ssbBitmap;
    }

    public Bitmap makeBitmapFromMtWithSamePaint(String text, TextPaint paint, boolean isRtl) {
        // MeasuredText uses measured result if the given paint is equal to the applied one.
        final MeasuredText mt = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(paint, text.length(), isRtl)
                .build();
        final Bitmap mtBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Canvas mtCanvas = new Canvas(mtBitmap);
        mtCanvas.save();
        mtCanvas.translate(0, 0);
        mtCanvas.drawTextRun(mt, 0, text.length(), 0, text.length(),
                0.0f /* x */, 240.0f /* y */, isRtl, paint);
        mtCanvas.restore();
        return mtBitmap;
    }

    public Bitmap makeBitmapFromMtWithDifferentPaint(String text, TextPaint paint, boolean isRtl) {
        // If different paint is provided when drawing, MeasuredText discards the measured result
        // and recompute immediately. Thus the final output must be the same with given one.
        final MeasuredText mt2 = new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(new Paint(), text.length(), isRtl)
                .build();
        final Bitmap mt2Bitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Canvas mt2Canvas = new Canvas(mt2Bitmap);
        mt2Canvas.save();
        mt2Canvas.translate(0, 0);
        mt2Canvas.drawTextRun(mt2, 0, text.length(), 0, text.length(),
                0.0f /* x */, 240.0f /* y */, isRtl, paint);
        mt2Canvas.restore();
        return mt2Bitmap;
    }

    public Bitmap makeBitmapFromPct(String text, TextPaint paint, boolean isRtl) {
        final PrecomputedText pct = PrecomputedText.create(
                text, new PrecomputedText.Params.Builder(paint).build());
        final Bitmap pctBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Canvas pctCanvas = new Canvas(pctBitmap);
        pctCanvas.save();
        pctCanvas.translate(0, 0);
        pctCanvas.drawTextRun(pct, 0, text.length(), 0, text.length(),
                0.0f /* x */, 240.0f /* y */, isRtl, paint);
        pctCanvas.restore();
        return pctBitmap;
    }

    @Test
    public void testCanvasDrawTextRun_sameOutputTestForLatinText() {
        final TextPaint paint = new TextPaint();
        paint.setTextSize(30.0f);

        final String text = "Hello, World";

        final Bitmap blankBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Bitmap ssbBitmap = makeBitmapFromSsb(text, paint, false);
        assertFalse(blankBitmap.sameAs(ssbBitmap));

        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithSamePaint(text, paint, false)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithDifferentPaint(text, paint, false)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromPct(text, paint, false)));
    }

    @Test
    public void testCanvasDrawTextRun_sameOutputTestForCJKText() {
        final TextPaint paint = new TextPaint();
        paint.setTextSize(30.0f);

        final String text = "\u3042\u3044\u3046\u3048\u304A";

        final Bitmap blankBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Bitmap ssbBitmap = makeBitmapFromSsb(text, paint, false);
        assertFalse(blankBitmap.sameAs(ssbBitmap));

        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithSamePaint(text, paint, false)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithDifferentPaint(text, paint, false)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromPct(text, paint, false)));
    }

    @Test
    public void testCanvasDrawTextRun_sameOutputTestForRTLText() {
        final TextPaint paint = new TextPaint();
        paint.setTextSize(30.0f);

        final String text = "\u05D0\u05D1\u05D2\u05D3\u05D4";

        final Bitmap blankBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
        final Bitmap ssbBitmap = makeBitmapFromSsb(text, paint, true);
        assertFalse(blankBitmap.sameAs(ssbBitmap));

        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithSamePaint(text, paint, true)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromMtWithDifferentPaint(text, paint, true)));
        assertTrue(ssbBitmap.sameAs(makeBitmapFromPct(text, paint, true)));
    }
}
