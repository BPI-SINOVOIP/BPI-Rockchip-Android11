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

package android.text.style.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint.FontMetricsInt;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.cts.R;
import android.text.style.DynamicDrawableSpan;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class DynamicDrawableSpanTest {
    private static final int DRAWABLE_SIZE = 50;

    @Test
    public void testConstructor() {
        DynamicDrawableSpan d = new MyDynamicDrawableSpan();
        assertEquals(DynamicDrawableSpan.ALIGN_BOTTOM, d.getVerticalAlignment());

        d = new MyDynamicDrawableSpan(DynamicDrawableSpan.ALIGN_BASELINE);
        assertEquals(DynamicDrawableSpan.ALIGN_BASELINE, d.getVerticalAlignment());

        d = new MyDynamicDrawableSpan(DynamicDrawableSpan.ALIGN_BOTTOM);
        assertEquals(DynamicDrawableSpan.ALIGN_BOTTOM, d.getVerticalAlignment());

        d = new MyDynamicDrawableSpan(DynamicDrawableSpan.ALIGN_CENTER);
        assertEquals(DynamicDrawableSpan.ALIGN_CENTER, d.getVerticalAlignment());
    }

    @Test
    public void testGetSize() {
        DynamicDrawableSpan dynamicDrawableSpan = new MyDynamicDrawableSpan();
        FontMetricsInt fm = new FontMetricsInt();

        assertEquals(0, fm.ascent);
        assertEquals(0, fm.bottom);
        assertEquals(0, fm.descent);
        assertEquals(0, fm.leading);
        assertEquals(0, fm.top);

        Rect rect = dynamicDrawableSpan.getDrawable().getBounds();
        assertEquals(rect.right, dynamicDrawableSpan.getSize(null, null, 0, 0, fm));

        assertEquals(-rect.bottom, fm.ascent);
        assertEquals(0, fm.bottom);
        assertEquals(0, fm.descent);
        assertEquals(0, fm.leading);
        assertEquals(-rect.bottom, fm.top);

        assertEquals(rect.right, dynamicDrawableSpan.getSize(null, null, 0, 0, null));
    }

    @Test
    public void testDraw() {
        DynamicDrawableSpan dynamicDrawableSpan = new MyDynamicDrawableSpan();
        Canvas canvas = new Canvas();
        dynamicDrawableSpan.draw(canvas, null /* text */, 0 /* start */, 0 /* end */, 1.0f /* x */,
                0 /* top */, 0 /* y */, 1 /* bottom */, null /* paint */);
    }

    @Test(expected=NullPointerException.class)
    public void testDrawNullCanvas() {
        DynamicDrawableSpan dynamicDrawableSpan = new MyDynamicDrawableSpan();

        dynamicDrawableSpan.draw(null /* canvas */, null /* text */, 0 /* start */, 0 /* end */,
                1.0f /* x */, 0 /* top */, 0 /* y */, 1 /* bottom */, null /* paint */);
    }

    @Test
    public void testCenterAligned() {
        final DynamicDrawableSpan dynamicDrawableSpan =
                new MyDynamicDrawableSpan(DynamicDrawableSpan.ALIGN_CENTER);
        final int padding = 10;
        final int top = 10;
        final int bottom = top + DRAWABLE_SIZE + padding;

        final Bitmap bitmap = Bitmap.createBitmap(DRAWABLE_SIZE, bottom, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.RED);
        dynamicDrawableSpan.draw(canvas, null /* text */, 0 /* start */, 0 /* end */, 0f /* x */,
                top, 0 /* y */, bottom, null /* paint */);

        // Top should be completely red.
        for (int i = 0; i < top + padding / 2; i++) {
            assertThat(bitmap.getColor(0, i).toArgb()).isEqualTo(Color.RED);
        }
        // Center should have been filled with drawable.
        for (int i = 0; i < DRAWABLE_SIZE; i++) {
            assertThat(bitmap.getColor(0, top + i + padding / 2).toArgb()).isNotEqualTo(Color.RED);
        }
        // Bottom should be also red.
        for (int i = 0; i < padding / 2; i++) {
            assertThat(bitmap.getColor(0, top + i + DRAWABLE_SIZE + padding / 2).toArgb())
                    .isEqualTo(Color.RED);
        }
        bitmap.recycle();
    }

    private class MyDynamicDrawableSpan extends DynamicDrawableSpan {
        private final Drawable mDrawable;

        public MyDynamicDrawableSpan() {
            this(ALIGN_BOTTOM);
        }

        protected MyDynamicDrawableSpan(int verticalAlignment) {
            super(verticalAlignment);
            mDrawable = InstrumentationRegistry.getTargetContext().getDrawable(R.drawable.scenery);
            mDrawable.setBounds(0, 0, DRAWABLE_SIZE, DRAWABLE_SIZE);
        }

        @Override
        public Drawable getDrawable() {
            return mDrawable;
        }
    }
}
