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
 * A test suite for the BipEncoding class
 */
@RunWith(AndroidJUnit4.class)
public class BipEncodingTest {

    private void testParse(String input, int encodingType, String encodingStr, String propId,
            boolean isAndroidSupported) {
        BipEncoding encoding = new BipEncoding(input);
        Assert.assertEquals(encodingType, encoding.getType());
        Assert.assertEquals(encodingStr, encoding.toString());
        Assert.assertEquals(propId, encoding.getProprietaryEncodingId());
        Assert.assertEquals(isAndroidSupported, encoding.isAndroidSupported());
    }

    private void testParseMany(String[] inputs, int encodingType, String encodingStr, String propId,
            boolean isAndroidSupported) {
        for (String input : inputs) {
            testParse(input, encodingType, encodingStr, propId, isAndroidSupported);
        }
    }

    @Test
    public void testParseJpeg() {
        String[] inputs = new String[]{"JPEG", "jpeg", "Jpeg", "JpEg"};
        testParseMany(inputs, BipEncoding.JPEG, "JPEG", null, true);
    }

    @Test
    public void testParseGif() {
        String[] inputs = new String[]{"GIF", "gif", "Gif", "gIf"};
        testParseMany(inputs, BipEncoding.GIF, "GIF", null, true);
    }

    @Test
    public void testParseWbmp() {
        String[] inputs = new String[]{"WBMP", "wbmp", "Wbmp", "WbMp"};
        testParseMany(inputs, BipEncoding.WBMP, "WBMP", null, false);
    }

    @Test
    public void testParsePng() {
        String[] inputs = new String[]{"PNG", "png", "Png", "PnG"};
        testParseMany(inputs, BipEncoding.PNG, "PNG", null, true);
    }

    @Test
    public void testParseJpeg2000() {
        String[] inputs = new String[]{"JPEG2000", "jpeg2000", "Jpeg2000", "JpEg2000"};
        testParseMany(inputs, BipEncoding.JPEG2000, "JPEG2000", null, false);
    }

    @Test
    public void testParseBmp() {
        String[] inputs = new String[]{"BMP", "bmp", "Bmp", "BmP"};
        testParseMany(inputs, BipEncoding.BMP, "BMP", null, true);
    }

    @Test
    public void testParseUsrProprietary() {
        String[] inputs = new String[]{"USR-test", "usr-test", "Usr-Test", "UsR-TeSt"};
        testParseMany(inputs, BipEncoding.USR_XXX, "USR-TEST", "TEST", false);

        // Example used in the spec so not a bad choice here
        inputs = new String[]{"USR-NOKIA-FORMAT1", "usr-nokia-format1"};
        testParseMany(inputs, BipEncoding.USR_XXX, "USR-NOKIA-FORMAT1", "NOKIA-FORMAT1", false);
    }

    @Test
    public void testCreateBasicEncoding() {
        int[] inputs = new int[]{BipEncoding.JPEG, BipEncoding.PNG, BipEncoding.BMP,
                BipEncoding.GIF, BipEncoding.JPEG2000, BipEncoding.WBMP};
        for (int encodingType : inputs) {
            BipEncoding encoding = new BipEncoding(encodingType, null);
            Assert.assertEquals(encodingType, encoding.getType());
            Assert.assertEquals(null, encoding.getProprietaryEncodingId());
        }
    }

    @Test
    public void testCreateProprietaryEncoding() {
        BipEncoding encoding = new BipEncoding(BipEncoding.USR_XXX, "test-encoding");
        Assert.assertEquals(BipEncoding.USR_XXX, encoding.getType());
        Assert.assertEquals("TEST-ENCODING", encoding.getProprietaryEncodingId());
        Assert.assertEquals("USR-TEST-ENCODING", encoding.toString());
        Assert.assertFalse(encoding.isAndroidSupported());
    }

    @Test
    public void testCreateProprietaryEncoding_emptyId() {
        BipEncoding encoding = new BipEncoding(BipEncoding.USR_XXX, "");
        Assert.assertEquals(BipEncoding.USR_XXX, encoding.getType());
        Assert.assertEquals("", encoding.getProprietaryEncodingId());
        Assert.assertEquals("USR-", encoding.toString());
        Assert.assertFalse(encoding.isAndroidSupported());
    }

    @Test(expected = ParseException.class)
    public void testParseInvalidName_throwsException() {
        testParse("JIF", BipEncoding.UNKNOWN, "UNKNOWN", null, false);
    }

    @Test(expected = ParseException.class)
    public void testParseInvalidUsrEncoding_throwsException() {
        testParse("USR", BipEncoding.UNKNOWN, "UNKNOWN", null, false);
    }

    @Test(expected = ParseException.class)
    public void testParseNullString() {
        testParse(null, BipEncoding.UNKNOWN, "UNKNOWN", null, false);
    }

    @Test(expected = ParseException.class)
    public void testParseEmptyString() {
        testParse("", BipEncoding.UNKNOWN, "UNKNOWN", null, false);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreateInvalidEncoding() {
        BipEncoding encoding = new BipEncoding(-5, null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreateProprietary_nullId() {
        BipEncoding encoding = new BipEncoding(BipEncoding.USR_XXX, null);
    }
}
