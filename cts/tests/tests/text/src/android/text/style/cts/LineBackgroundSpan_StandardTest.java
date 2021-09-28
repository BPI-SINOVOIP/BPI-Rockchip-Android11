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

package android.text.style.cts;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.Parcel;
import android.text.style.LineBackgroundSpan;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LineBackgroundSpan_StandardTest {

    static final int COLOR = 0x04CB2F;

    @Test
    public void testGetColor() {
        final LineBackgroundSpan.Standard span = new LineBackgroundSpan.Standard(COLOR);
        assertEquals(COLOR, span.getColor());
    }


    @Test
    public void testWriteToParcel() {
        final LineBackgroundSpan.Standard span = new LineBackgroundSpan.Standard(COLOR);
        final Parcel p = Parcel.obtain();
        try {
            span.writeToParcel(p, 0);
            p.setDataPosition(0);
            final LineBackgroundSpan.Standard parcelSpan = new LineBackgroundSpan.Standard(p);
            assertEquals(COLOR, parcelSpan.getColor());
            assertEquals(span.getSpanTypeId(), parcelSpan.getSpanTypeId());
        } finally {
            p.recycle();
        }
    }

    @Test
    public void testDrawBackground() {
        final LineBackgroundSpan span = new LineBackgroundSpan.Standard(COLOR);

        final Canvas canvas = mock(Canvas.class);
        final Paint paint = mock(Paint.class);
        final int left = 10;
        final int right = 200;
        final int top = 20;
        final int baseline = 40;
        final int bottom = 50;
        final String text = "this is the test text!";

        span.drawBackground(canvas, paint, left, right, top, baseline, bottom,
                text, 0, text.length(), 0);
        verify(canvas).drawRect(left, top, right, bottom, paint);
    }
}

