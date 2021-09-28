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

import android.graphics.Paint.FontMetricsInt;
import android.os.Parcel;
import android.text.style.LineHeightSpan;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LineHeightSpan_StandardTest {
    @Test
    public void testHeightPositive() {
        // Shouldn't be any exception.
        new LineHeightSpan.Standard(100);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testHeightZero() {
        new LineHeightSpan.Standard(0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testHeightNegative() {
        new LineHeightSpan.Standard(-1);
    }

    @Test
    public void testChooseLineHeight() {
        final int expectHeight = 50;
        LineHeightSpan span = new LineHeightSpan.Standard(expectHeight);
        FontMetricsInt fm = new FontMetricsInt();

        fm.ascent = -20;
        fm.bottom = 20;
        span.chooseHeight("helloworld", 0, 9, 0, fm.descent - fm.ascent, fm);
        assertEquals(expectHeight, fm.descent - fm.ascent);

        fm.ascent = -50;
        fm.descent = 30;
        span.chooseHeight("helloworld", 0, 9, 0, fm.descent - fm.ascent, fm);
        assertEquals(expectHeight, fm.descent - fm.ascent);
    }

    @Test
    public void testChooseLineHeightWithNegativeOriginHeight() {
        // Nothing changes when origin height is negative
        final int ascent = 30;
        final int descent = -10;
        LineHeightSpan span = new LineHeightSpan.Standard(50);
        FontMetricsInt fm = new FontMetricsInt();

        fm.ascent = ascent;
        fm.descent = descent;
        span.chooseHeight("helloworld", 0, 9, 0, fm.descent - fm.ascent, fm);
        assertEquals(ascent, fm.ascent);
        assertEquals(descent, fm.descent);
    }

    @Test
    public void testGetHeight() {
        final int height = 23;
        LineHeightSpan.Standard span = new LineHeightSpan.Standard(height);

        assertEquals(height, span.getHeight());
    }

    @Test
    public void testWriteToParcel() {
        final LineHeightSpan.Standard span = new LineHeightSpan.Standard(20);
        final Parcel parcel = Parcel.obtain();
        span.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);

        final LineHeightSpan.Standard parcelSpan = new LineHeightSpan.Standard(parcel);
        assertEquals(span.getHeight(), parcelSpan.getHeight());
        assertEquals(span.getSpanTypeId(), parcelSpan.getSpanTypeId());
    }
}
