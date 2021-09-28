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

import static org.junit.Assert.assertEquals;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.FontMetricsInt;
import android.text.style.ReplacementSpan;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ReplacementSpanTest {
    @Test
    public void testUpdateMeasureState() {
        ReplacementSpan replacementSpan = new MockReplacementSpan();
        replacementSpan.updateMeasureState(null);
    }

    @Test
    public void testUpdateDrawState() {
        ReplacementSpan replacementSpan = new MockReplacementSpan();
        replacementSpan.updateDrawState(null);
    }

    @Test
    public void testContentDescription() {
        final String testContentDescription = "testContentDescription";
        ReplacementSpan replacementSpan = new MockReplacementSpan();

        replacementSpan.setContentDescription(testContentDescription);

        assertEquals(testContentDescription, replacementSpan.getContentDescription());
    }

    private class MockReplacementSpan extends ReplacementSpan {
        @Override
        public void draw(Canvas canvas, CharSequence text, int start, int end,
                float x, int top, int y, int bottom, Paint paint) {
        }

        @Override
        public int getSize(Paint paint, CharSequence text, int start, int end,
                FontMetricsInt fm) {
            return 0;
        }
    }
}
