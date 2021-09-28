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

import static android.graphics.text.LineBreaker.BREAK_STRATEGY_BALANCED;
import static android.graphics.text.LineBreaker.BREAK_STRATEGY_HIGH_QUALITY;
import static android.graphics.text.LineBreaker.BREAK_STRATEGY_SIMPLE;
import static android.graphics.text.LineBreaker.HYPHENATION_FREQUENCY_FULL;
import static android.graphics.text.LineBreaker.HYPHENATION_FREQUENCY_NONE;
import static android.graphics.text.LineBreaker.HYPHENATION_FREQUENCY_NORMAL;
import static android.graphics.text.LineBreaker.JUSTIFICATION_MODE_INTER_WORD;
import static android.graphics.text.LineBreaker.JUSTIFICATION_MODE_NONE;
import static android.graphics.text.LineBreaker.ParagraphConstraints;
import static android.graphics.text.LineBreaker.Result;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.text.LineBreaker;
import android.graphics.text.MeasuredText;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LineBreakerTest {
    private static final String TAG = "LineBreakerTest";

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
    public void testLineBreak_construct() {
        assertNotNull(new LineBreaker.Builder().build());
    }

    @Test
    public void testSetBreakStrategy_shoulNotThrowExceptions() {
        assertNotNull(new LineBreaker.Builder().setBreakStrategy(BREAK_STRATEGY_SIMPLE).build());
        assertNotNull(new LineBreaker.Builder().setBreakStrategy(BREAK_STRATEGY_HIGH_QUALITY)
                .build());
        assertNotNull(new LineBreaker.Builder().setBreakStrategy(BREAK_STRATEGY_BALANCED).build());
    }

    @Test
    public void testSetHyphenationFrequency_shouldNotThrowExceptions() {
        assertNotNull(new LineBreaker.Builder()
                .setHyphenationFrequency(HYPHENATION_FREQUENCY_NORMAL).build());
        assertNotNull(new LineBreaker.Builder()
                .setHyphenationFrequency(HYPHENATION_FREQUENCY_FULL).build());
        assertNotNull(new LineBreaker.Builder()
                .setHyphenationFrequency(HYPHENATION_FREQUENCY_NONE).build());
    }

    @Test
    public void testSetJustification_shouldNotThrowExceptions() {
        assertNotNull(new LineBreaker.Builder()
                .setJustificationMode(JUSTIFICATION_MODE_NONE).build());
        assertNotNull(new LineBreaker.Builder()
                .setJustificationMode(JUSTIFICATION_MODE_INTER_WORD).build());
    }

    @Test
    public void testSetIntent_shouldNotThrowExceptions() {
        assertNotNull(new LineBreaker.Builder().setIndents(null).build());
        assertNotNull(new LineBreaker.Builder().setIndents(new int[] {}).build());
        assertNotNull(new LineBreaker.Builder().setIndents(new int[] { 100 }).build());
    }

    @Test
    public void testSetGetWidth() {
        ParagraphConstraints c = new ParagraphConstraints();
        assertEquals(0, c.getWidth(), 0.0f);  // 0 by default
        c.setWidth(100);
        assertEquals(100, c.getWidth(), 0.0f);
        c.setWidth(200);
        assertEquals(200, c.getWidth(), 0.0f);
    }

    @Test
    public void testSetGetIndent() {
        ParagraphConstraints c = new ParagraphConstraints();
        assertEquals(0.0f, c.getFirstWidth(), 0.0f);  // 0 by default
        assertEquals(0, c.getFirstWidthLineCount());  // 0 by default
        c.setIndent(100.0f, 1);
        assertEquals(100.0f, c.getFirstWidth(), 0.0f);
        assertEquals(1, c.getFirstWidthLineCount());
        c.setIndent(200.0f, 5);
        assertEquals(200.0f, c.getFirstWidth(), 0.0f);
        assertEquals(5, c.getFirstWidthLineCount());
    }

    @Test
    public void testSetGetTabStops() {
        ParagraphConstraints c = new ParagraphConstraints();
        assertNull(c.getTabStops());  // null by default
        assertEquals(0, c.getDefaultTabStop(), 0.0);  // 0 by default
        c.setTabStops(new float[] { 120 }, 240);
        assertEquals(1, c.getTabStops().length);
        assertEquals(120, c.getTabStops()[0], 0.0);
        assertEquals(240, c.getDefaultTabStop(), 0.0);
    }

    @Test
    public void testLineBreak_Simple() {
        final String text = "Hello, World.";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(Float.MAX_VALUE);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false).build(), c, 0);
        assertEquals(1, r.getLineCount());
        assertEquals(13, r.getLineBreakOffset(0));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertFalse(r.hasLineTab(0));
        assertEquals(130.0f, r.getLineWidth(0), 0.0f);
    }

    @Test
    public void testLineBreak_Simple2() {
        // The visual line break output is like
        // |abc defg|
        // |hijkl   |
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(80.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false).build(), c, 0);
        assertEquals(2, r.getLineCount());
        assertEquals(9, r.getLineBreakOffset(0));
        assertEquals(14, r.getLineBreakOffset(1));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertEquals(80.0f, r.getLineWidth(0), 0.0f);
        assertEquals(50.0f, r.getLineWidth(1), 0.0f);
    }

    @Test
    public void testLineBreak_Simple3() {
        // The visual line break output is like
        // |abc |
        // |defg|
        // |hijk|
        // |l   |
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(40.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false).build(), c, 0);
        assertEquals(4, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(9, r.getLineBreakOffset(1));
        assertEquals(13, r.getLineBreakOffset(2));
        assertEquals(14, r.getLineBreakOffset(3));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(3), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(3), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(3));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(3));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertFalse(r.hasLineTab(3));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(40.0f, r.getLineWidth(1), 0.0f);
        assertEquals(40.0f, r.getLineWidth(2), 0.0f);
        assertEquals(10.0f, r.getLineWidth(3), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_NotRectangle() {
        // The visual line break output is like
        // |abc  |
        // |defg hijkl|
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(100.0f);
        c.setIndent(50.0f, 1);  // Make the first line width 50 px.
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false).build(), c, 0);
        assertEquals(2, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(14, r.getLineBreakOffset(1));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(100.0f, r.getLineWidth(1), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_Hyphenation() {
        // The visual line break output is like
        // |abc |
        // |defg|
        // |hi- |
        // |jkl |
        final String text = "ab\u00ADc de\u00ADfg hi\u00ADjkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .setHyphenationFrequency(HYPHENATION_FREQUENCY_NORMAL)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(40.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false).build(), c, 0);
        assertEquals(4, r.getLineCount());
        assertEquals(5, r.getLineBreakOffset(0));
        assertEquals(11, r.getLineBreakOffset(1));
        assertEquals(14, r.getLineBreakOffset(2));
        assertEquals(17, r.getLineBreakOffset(3));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(3), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(3), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_INSERT_HYPHEN, r.getEndLineHyphenEdit(2));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(3));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(3));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertFalse(r.hasLineTab(3));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(40.0f, r.getLineWidth(1), 0.0f);
        assertEquals(30.0f, r.getLineWidth(2), 0.0f);
        assertEquals(30.0f, r.getLineWidth(3), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_Styled() {
        // The visual line break output is like
        // |abc      |
        // |ddeeffgg | (Make text size of "defg" doubled)
        // |hijkl    |
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(90.0f);
        final Paint biggerPaint = new Paint(sPaint);
        biggerPaint.setTextSize(sPaint.getTextSize() * 2.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, 4, false)
                .appendStyleRun(biggerPaint, 5, false)
                .appendStyleRun(sPaint, 5, false).build(), c, 0);
        assertEquals(3, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(9, r.getLineBreakOffset(1));
        assertEquals(14, r.getLineBreakOffset(2));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-20.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(4.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(80.0f, r.getLineWidth(1), 0.0f);
        assertEquals(50.0f, r.getLineWidth(2), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_Styled2() {
        // The visual line break output is like
        // |abc deffg| (Make text size of "f" doubled)
        // |hijkl    |
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(90.0f);
        final Paint biggerPaint = new Paint(sPaint);
        biggerPaint.setTextSize(sPaint.getTextSize() * 2.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, 6, false)
                .appendStyleRun(biggerPaint, 1, false)
                .appendStyleRun(sPaint, 7, false)
                .build(), c, 0);
        assertEquals(2, r.getLineCount());
        assertEquals(9, r.getLineBreakOffset(0));
        assertEquals(14, r.getLineBreakOffset(1));
        assertEquals(-20.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(4.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertEquals(90.0f, r.getLineWidth(0), 0.0f);
        assertEquals(50.0f, r.getLineWidth(1), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_indents() {
        // The visual line break output is like
        // |abc  |
        // |defg hijkl|
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .setIndents(new int[] { 50, 0 })  // The first line indent is 50 and 0 for others.
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(100.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false)
                .build(), c, 0);
        assertEquals(2, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(14, r.getLineBreakOffset(1));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(100.0f, r.getLineWidth(1), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_indents2() {
        // The visual line break output is like
        // |abc |
        // |defg|
        // |hijkl     |
        final String text = "abc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .setIndents(new int[] { 60, 60, 0 })
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(100.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false)
                .build(), c, 0);
        assertEquals(3, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(9, r.getLineBreakOffset(1));
        assertEquals(14, r.getLineBreakOffset(2));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(40.0f, r.getLineWidth(1), 0.0f);
        assertEquals(50.0f, r.getLineWidth(2), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_tabStop() {
        // The visual line break output is like
        // |abc    |
        // |de  fg |
        // |hijkl  |
        final String text = "abc de\tfg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(70.0f);
        c.setTabStops(null, 40);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false)
                .build(), c, 0);
        assertEquals(3, r.getLineCount());
        assertEquals(4, r.getLineBreakOffset(0));
        assertEquals(10, r.getLineBreakOffset(1));
        assertEquals(15, r.getLineBreakOffset(2));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertFalse(r.hasLineTab(0));
        assertTrue(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertEquals(30.0f, r.getLineWidth(0), 0.0f);
        assertEquals(60.0f, r.getLineWidth(1), 0.0f);
        assertEquals(50.0f, r.getLineWidth(2), 0.0f);
    }

    @Test
    public void testLineBreak_Simple_tabStop2() {
        // The visual line break output is like
        // |a b  c |
        // |defg   |
        // |hijkl  |
        final String text = "a\tb\tc defg hijkl";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_SIMPLE)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(70.0f);
        c.setTabStops(new float[] { 20 }, 50);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false)
                .build(), c, 0);
        assertEquals(3, r.getLineCount());
        assertEquals(6, r.getLineBreakOffset(0));
        assertEquals(11, r.getLineBreakOffset(1));
        assertEquals(16, r.getLineBreakOffset(2));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertTrue(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertEquals(60.0f, r.getLineWidth(0), 0.0f);
        assertEquals(40.0f, r.getLineWidth(1), 0.0f);
        assertEquals(50.0f, r.getLineWidth(2), 0.0f);
    }

    @Test
    public void testLineBreak_Balanced() {
        // The visual BALANCED line break output is like
        // |This   |
        // |is an  |
        // |example|
        //
        // FYI, SIMPLE line breaker breaks this string to
        // |This is|
        // |an     |
        // |example|
        final String text = "This is an example";
        final LineBreaker lb = new LineBreaker.Builder()
                .setBreakStrategy(BREAK_STRATEGY_BALANCED)
                .build();
        final ParagraphConstraints c = new ParagraphConstraints();
        c.setWidth(70.0f);
        final Result r = lb.computeLineBreaks(new MeasuredText.Builder(text.toCharArray())
                .appendStyleRun(sPaint, text.length(), false)
                .build(), c, 0);
        assertEquals(3, r.getLineCount());
        assertEquals(5, r.getLineBreakOffset(0));
        assertEquals(11, r.getLineBreakOffset(1));
        assertEquals(18, r.getLineBreakOffset(2));
        assertEquals(-10.0f, r.getLineAscent(0), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(1), 0.0f);
        assertEquals(-10.0f, r.getLineAscent(2), 0.0f);
        assertEquals(2.0f, r.getLineDescent(0), 0.0f);
        assertEquals(2.0f, r.getLineDescent(1), 0.0f);
        assertEquals(2.0f, r.getLineDescent(2), 0.0f);
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(0));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(0));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(1));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(1));
        assertEquals(Paint.START_HYPHEN_EDIT_NO_EDIT, r.getStartLineHyphenEdit(2));
        assertEquals(Paint.END_HYPHEN_EDIT_NO_EDIT, r.getEndLineHyphenEdit(2));
        assertFalse(r.hasLineTab(0));
        assertFalse(r.hasLineTab(1));
        assertFalse(r.hasLineTab(2));
        assertEquals(40.0f, r.getLineWidth(0), 0.0f);
        assertEquals(50.0f, r.getLineWidth(1), 0.0f);
        assertEquals(70.0f, r.getLineWidth(2), 0.0f);
    }
}
