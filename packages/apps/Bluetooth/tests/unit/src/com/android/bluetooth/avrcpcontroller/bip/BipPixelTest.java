/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test suite for the BipPixel class
 */
@RunWith(AndroidJUnit4.class)
public class BipPixelTest {

    private void testParse(String input, int pixelType, int minWidth, int minHeight, int maxWidth,
            int maxHeight, String pixelStr) {
        BipPixel pixel = new BipPixel(input);
        Assert.assertEquals(pixelType, pixel.getType());
        Assert.assertEquals(minWidth, pixel.getMinWidth());
        Assert.assertEquals(minHeight, pixel.getMinHeight());
        Assert.assertEquals(maxWidth, pixel.getMaxWidth());
        Assert.assertEquals(maxHeight, pixel.getMaxHeight());
        Assert.assertEquals(pixelStr, pixel.toString());
    }

    private void testFixed(int width, int height, String pixelStr) {
        BipPixel pixel = BipPixel.createFixed(width, height);
        Assert.assertEquals(BipPixel.TYPE_FIXED, pixel.getType());
        Assert.assertEquals(width, pixel.getMinWidth());
        Assert.assertEquals(height, pixel.getMinHeight());
        Assert.assertEquals(width, pixel.getMaxWidth());
        Assert.assertEquals(height, pixel.getMaxHeight());
        Assert.assertEquals(pixelStr, pixel.toString());
    }

    private void testResizableModified(int minWidth, int minHeight, int maxWidth, int maxHeight,
            String pixelStr) {
        BipPixel pixel = BipPixel.createResizableModified(minWidth, minHeight, maxWidth, maxHeight);
        Assert.assertEquals(BipPixel.TYPE_RESIZE_MODIFIED_ASPECT_RATIO, pixel.getType());
        Assert.assertEquals(minWidth, pixel.getMinWidth());
        Assert.assertEquals(minHeight, pixel.getMinHeight());
        Assert.assertEquals(maxWidth, pixel.getMaxWidth());
        Assert.assertEquals(maxHeight, pixel.getMaxHeight());
        Assert.assertEquals(pixelStr, pixel.toString());
    }

    private void testResizableFixed(int minWidth, int maxWidth, int maxHeight,
            String pixelStr) {
        int minHeight = (minWidth * maxHeight) / maxWidth; // spec defined
        BipPixel pixel = BipPixel.createResizableFixed(minWidth, maxWidth, maxHeight);
        Assert.assertEquals(BipPixel.TYPE_RESIZE_FIXED_ASPECT_RATIO, pixel.getType());
        Assert.assertEquals(minWidth, pixel.getMinWidth());
        Assert.assertEquals(minHeight, pixel.getMinHeight());
        Assert.assertEquals(maxWidth, pixel.getMaxWidth());
        Assert.assertEquals(maxHeight, pixel.getMaxHeight());
        Assert.assertEquals(pixelStr, pixel.toString());
    }

    @Test
    public void testParseFixed() {
        testParse("0*0", BipPixel.TYPE_FIXED, 0, 0, 0, 0, "0*0");
        testParse("200*200", BipPixel.TYPE_FIXED, 200, 200, 200, 200, "200*200");
        testParse("12*67", BipPixel.TYPE_FIXED, 12, 67, 12, 67, "12*67");
        testParse("1000*1000", BipPixel.TYPE_FIXED, 1000, 1000, 1000, 1000, "1000*1000");
    }

    @Test
    public void testParseResizableModified() {
        testParse("0*0-200*200", BipPixel.TYPE_RESIZE_MODIFIED_ASPECT_RATIO, 0, 0, 200, 200,
                "0*0-200*200");
        testParse("200*200-600*600", BipPixel.TYPE_RESIZE_MODIFIED_ASPECT_RATIO, 200, 200, 600, 600,
                "200*200-600*600");
        testParse("12*67-34*89", BipPixel.TYPE_RESIZE_MODIFIED_ASPECT_RATIO, 12, 67, 34, 89,
                "12*67-34*89");
        testParse("123*456-1000*1000", BipPixel.TYPE_RESIZE_MODIFIED_ASPECT_RATIO, 123, 456, 1000,
                1000, "123*456-1000*1000");
    }

    @Test
    public void testParseResizableFixed() {
        testParse("0**-200*200", BipPixel.TYPE_RESIZE_FIXED_ASPECT_RATIO, 0, 0, 200, 200,
                "0**-200*200");
        testParse("200**-600*600", BipPixel.TYPE_RESIZE_FIXED_ASPECT_RATIO, 200, 200, 600, 600,
                "200**-600*600");
        testParse("123**-1000*1000", BipPixel.TYPE_RESIZE_FIXED_ASPECT_RATIO, 123, 123, 1000, 1000,
                "123**-1000*1000");
        testParse("12**-35*89", BipPixel.TYPE_RESIZE_FIXED_ASPECT_RATIO, 12, (12 * 89) / 35, 35, 89,
                "12**-35*89");
    }

    @Test
    public void testParseDimensionAtMax() {
        testParse("65535*65535", BipPixel.TYPE_FIXED, 65535, 65535, 65535, 65535, "65535*65535");
    }

    @Test
    public void testCreateFixed() {
        testFixed(0, 0, "0*0");
        testFixed(200, 200, "200*200");
        testFixed(17, 43, "17*43");
        testFixed(20000, 23456, "20000*23456");
    }

    @Test
    public void testCreateResizableModified() {
        testResizableModified(0, 0, 200, 200, "0*0-200*200");
        testResizableModified(200, 200, 600, 600, "200*200-600*600");
        testResizableModified(12, 67, 34, 89, "12*67-34*89");
        testResizableModified(123, 456, 1000, 1000, "123*456-1000*1000");
    }

    @Test
    public void testCreateResizableFixed() {
        testResizableFixed(0, 200, 200, "0**-200*200");
        testResizableFixed(200, 600, 600, "200**-600*600");
        testResizableFixed(123, 1000, 1000, "123**-1000*1000");
        testResizableFixed(12, 35, 89, "12**-35*89");
    }

    @Test
    public void testCreateDimensionsAtMax() {
        testFixed(65535, 65535, "65535*65535");
    }

    @Test(expected = ParseException.class)
    public void testParseNull_throwsException() {
        testParse(null, BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseEmpty_throwsException() {
        testParse("", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseWhitespace_throwsException() {
        testParse("\n\t ", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadCharacters_throwsException() {
        testParse("this*has-characters*init", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseTooManyAsterisks_throwsException() {
        testParse("123*****456", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseWithSymbols_throwsException() {
        testParse("!@#*342", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseEscapeCharacters_throwsException() {
        testParse("\\\\*\\\\-\\\\*\\\\", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseWidthTooLargeFixed_throwsException() {
        testParse("123456*123", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseHeightTooLargeFixed_throwsException() {
        testParse("123*123456", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMinWidthTooLargeResize_throwsException() {
        testParse("123456*1-12*1234", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMaxWidthTooLargeResize_throwsException() {
        testParse("1*1-123456*123", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMinHeightTooLargeResize_throwsException() {
        testParse("1*123456-12*1234", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMaxHeightTooLargeResize_throwsException() {
        testParse("1*1-12*123456", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMinWidthTooLargeResizeFixed_throwsException() {
        testParse("123456**-123*1234", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMaxWidthTooLargeResizeFixed_throwsException() {
        testParse("123**-123456*1234", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }

    @Test(expected = ParseException.class)
    public void testParseMaxHeightTooLargeResizeFixed_throwsException() {
        testParse("123**-1234*123456", BipPixel.TYPE_UNKNOWN, -1, -1, -1, -1, null);
    }
}
