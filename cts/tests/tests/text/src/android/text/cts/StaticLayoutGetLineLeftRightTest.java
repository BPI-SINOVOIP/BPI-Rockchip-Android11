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

package android.text.cts;

import static org.junit.Assert.assertEquals;

import android.text.Layout;
import android.text.Layout.Alignment;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.StaticLayout;
import android.text.TextDirectionHeuristic;
import android.text.TextDirectionHeuristics;
import android.text.TextPaint;
import android.text.style.LeadingMarginSpan;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.util.ArrayList;
import java.util.Collection;

@SmallTest
@RunWith(Parameterized.class)
public final class StaticLayoutGetLineLeftRightTest {

    // Here we should have a string without any space in case layout omit the
    // space, which will cause troubles for us to verify the length  of each line.
    // And in this test, we assume TEST_STR is LTR. It's initialized in setUp()
    static final String TEST_STR;

    static {
        String test_str = "";
        for (int i = 0; i < 100; ++i) {
            test_str += "a";
        }
        TEST_STR = test_str;
    }
    // Because Alignment.LEFT and Alignment.RIGHT are hidden,
    // following integers are used to represent alignment.
    static final int ALIGN_LEFT = 0;
    static final int ALIGN_RIGHT = 1;
    static final int ALIGN_CENTER = 2;

    final CharSequence mText;
    final TextPaint mPaint;
    final int mWidth;
    final Layout mLayout;

    final int mLeadingMargin;
    final Alignment mAlign;
    final TextDirectionHeuristic mDir;
    final int mExpectedAlign;

    @Parameterized.Parameters(name = "leadingMargin {0} align {1} dir {2}")
    public static Collection<Object[]> cases() {
        final Alignment[] aligns = new Alignment[]{Alignment.ALIGN_NORMAL, Alignment.ALIGN_OPPOSITE,
                Alignment.ALIGN_CENTER, null};
        // Use String to generate a meaningful test name,
        final TextDirectionHeuristic[] dirs = new TextDirectionHeuristic[]
                {TextDirectionHeuristics.LTR, TextDirectionHeuristics.RTL};
        final int[] leadingMargins = new int[] {0, 17, 23, 37};

        final ArrayList<Object[]> params = new ArrayList<>();
        for (int leadingMargin: leadingMargins) {
            for (Alignment align: aligns) {
                for (TextDirectionHeuristic dir: dirs) {
                    final String dirName =
                            dir == TextDirectionHeuristics.LTR ? "DIR_LTR" : "DIR_RTL";
                    params.add(new Object[] {leadingMargin, align, dirName, dir});
                }
            }
        }
        return params;
    }

    public StaticLayoutGetLineLeftRightTest(int leadingMargin, Alignment align,
            String dirName, TextDirectionHeuristic dir) {
        mLeadingMargin = leadingMargin;
        mAlign = align;
        mDir = dir;

        if (mLeadingMargin > 0) {
            final SpannableString ss = new SpannableString(TEST_STR);
            ss.setSpan(new LeadingMarginSpan.Standard(mLeadingMargin),
                    0, ss.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            mText = ss;
        } else {
            mText = TEST_STR;
        }

        mPaint = new TextPaint();
        // Set TextSize to 32 in case the default is too small.
        mPaint.setTextSize(32);
        // Try to make 10 lines.
        mWidth = mLeadingMargin + (int) mPaint.measureText(mText, 0, mText.length()) / 10;
        mLayout = StaticLayout.Builder.obtain(mText, 0, mText.length(), mPaint, mWidth)
                .setAlignment(mAlign)
                .setTextDirection(mDir)
                .build();

        if (mAlign == null) {
            mExpectedAlign = ALIGN_CENTER;
            return;
        }
        switch (mAlign) {
            case ALIGN_NORMAL:
                if (dir == TextDirectionHeuristics.RTL) {
                    mExpectedAlign = ALIGN_RIGHT;
                } else {
                    mExpectedAlign = ALIGN_LEFT;
                }
                break;
            case ALIGN_OPPOSITE:
                if (dir == TextDirectionHeuristics.RTL) {
                    mExpectedAlign = ALIGN_LEFT;
                } else {
                    mExpectedAlign = ALIGN_RIGHT;
                }
                break;
            default: /* mAlign == Alignment.ALIGN_CENTER */
                mExpectedAlign = ALIGN_CENTER;
        }
    }

    @Test
    public void testGetLineLeft() {
        final int lineCount = mLayout.getLineCount();
        for (int line = 0; line < lineCount; ++line) {
            final int start = mLayout.getLineStart(line);
            final int end = mLayout.getLineEnd(line);
            final float textWidth = mPaint.measureText(mText, start, end);
            final float expectedLeft;
            if (mExpectedAlign == ALIGN_LEFT) {
                expectedLeft = 0.0f;
            } else if (mExpectedAlign == ALIGN_RIGHT) {
                expectedLeft = mWidth - textWidth - mLeadingMargin;
            } else {
                if (mDir == TextDirectionHeuristics.RTL) {
                    expectedLeft = (float) Math.floor((mWidth - mLeadingMargin - textWidth) / 2);
                } else {
                    expectedLeft = (float) Math.floor(mLeadingMargin
                            + (mWidth - mLeadingMargin - textWidth) / 2);
                }
            }
            assertEquals(expectedLeft, mLayout.getLineLeft(line), 0.0f);
        }
    }

    @Test
    public void testGetLineRight() {
        final int lineCount = mLayout.getLineCount();
        for (int line = 0; line < lineCount; ++line) {
            final int start = mLayout.getLineStart(line);
            final int end = mLayout.getLineEnd(line);
            final float textWidth = mPaint.measureText(mText, start, end);
            final float expectedRight;
            if (mExpectedAlign == ALIGN_LEFT) {
                expectedRight = mLeadingMargin + textWidth;
            } else if (mExpectedAlign == ALIGN_RIGHT) {
                expectedRight = mWidth;
            } else {
                if (mDir == TextDirectionHeuristics.RTL) {
                    expectedRight = (float) Math.ceil(mWidth - mLeadingMargin
                            - (mWidth - mLeadingMargin - textWidth) / 2);
                } else {
                    expectedRight = (float) Math.ceil(mWidth
                            - (mWidth - mLeadingMargin - textWidth) / 2);
                }
            }
            assertEquals(expectedRight, mLayout.getLineRight(line), 0.0f);
        }
    }
}
