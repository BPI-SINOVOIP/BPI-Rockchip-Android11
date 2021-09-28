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
 * A test suite for the BipImageFormat class
 */
@RunWith(AndroidJUnit4.class)
public class BipImageFormatTest {
    @Test
    public void testParseNative_requiredOnly() {
        String expected = "<native encoding=\"JPEG\" pixel=\"1280*1024\" />";
        BipImageFormat format = BipImageFormat.parseNative("JPEG", "1280*1024", null);
        Assert.assertEquals(BipImageFormat.FORMAT_NATIVE, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testParseNative_withSize() {
        String expected = "<native encoding=\"JPEG\" pixel=\"1280*1024\" size=\"1048576\" />";
        BipImageFormat format = BipImageFormat.parseNative("JPEG", "1280*1024", "1048576");
        Assert.assertEquals(BipImageFormat.FORMAT_NATIVE, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(1048576, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testParseVariant_requiredOnly() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"1280*1024\" />";
        BipImageFormat format = BipImageFormat.parseVariant("JPEG", "1280*1024", null, null);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testParseVariant_withMaxSize() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"1280*1024\" maxsize=\"1048576\" />";
        BipImageFormat format = BipImageFormat.parseVariant("JPEG", "1280*1024", "1048576", null);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(1048576, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testParseVariant_withTransformation() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"1280*1024\""
                + " transformation=\"stretch fill crop\" />";
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.STRETCH);
        trans.addTransformation(BipTransformation.FILL);
        trans.addTransformation(BipTransformation.CROP);

        BipImageFormat format = null;

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", null, "stretch fill crop");
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(expected, format.toString());

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", null, "stretch crop fill");
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(expected, format.toString());

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", null, "crop stretch fill");
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testParseVariant_allFields() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"1280*1024\""
                + " transformation=\"stretch fill crop\" maxsize=\"1048576\" />";
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.STRETCH);
        trans.addTransformation(BipTransformation.FILL);
        trans.addTransformation(BipTransformation.CROP);

        BipImageFormat format = null;

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", "1048576", "stretch fill crop");
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(1048576, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", "1048576", "stretch crop fill");
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(1048576, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());

        format = BipImageFormat.parseVariant("JPEG", "1280*1024", "1048576", "crop stretch fill");
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(1048576, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateNative_requiredOnly() {
        String expected = "<native encoding=\"JPEG\" pixel=\"1280*1024\" />";
        BipImageFormat format = BipImageFormat.createNative(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(1280, 1024), -1);
        Assert.assertEquals(BipImageFormat.FORMAT_NATIVE, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateNative_withSize() {
        String expected = "<native encoding=\"JPEG\" pixel=\"1280*1024\" size=\"1048576\" />";
        BipImageFormat format = BipImageFormat.createNative(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(1280, 1024), 1048576);
        Assert.assertEquals(BipImageFormat.FORMAT_NATIVE, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(1280, 1024), format.getPixel());
        Assert.assertEquals(new BipTransformation(), format.getTransformation());
        Assert.assertEquals(1048576, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateVariant_requiredOnly() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"32*32\" />";
        BipImageFormat format = BipImageFormat.createVariant(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(32, 32), -1, null);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(32, 32), format.getPixel());
        Assert.assertEquals(null, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateVariant_withTransformations() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.STRETCH,
                BipTransformation.CROP, BipTransformation.FILL});
        String expected = "<variant encoding=\"JPEG\" pixel=\"32*32\" "
                + "transformation=\"stretch fill crop\" />";
        BipImageFormat format = BipImageFormat.createVariant(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(32, 32), -1, trans);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(32, 32), format.getPixel());
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(-1, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateVariant_withMaxsize() {
        String expected = "<variant encoding=\"JPEG\" pixel=\"32*32\" maxsize=\"123\" />";
        BipImageFormat format = BipImageFormat.createVariant(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(32, 32), 123, null);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(32, 32), format.getPixel());
        Assert.assertEquals(null, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(123, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test
    public void testCreateVariant_allFields() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.STRETCH,
                BipTransformation.CROP, BipTransformation.FILL});
        String expected = "<variant encoding=\"JPEG\" pixel=\"32*32\" "
                + "transformation=\"stretch fill crop\" maxsize=\"123\" />";
        BipImageFormat format = BipImageFormat.createVariant(
                new BipEncoding(BipEncoding.JPEG, null), BipPixel.createFixed(32, 32), 123, trans);
        Assert.assertEquals(BipImageFormat.FORMAT_VARIANT, format.getType());
        Assert.assertEquals(new BipEncoding(BipEncoding.JPEG, null), format.getEncoding());
        Assert.assertEquals(BipPixel.createFixed(32, 32), format.getPixel());
        Assert.assertEquals(trans, format.getTransformation());
        Assert.assertEquals(-1, format.getSize());
        Assert.assertEquals(123, format.getMaxSize());
        Assert.assertEquals(expected, format.toString());
    }

    @Test(expected = ParseException.class)
    public void testParseNative_noEncoding() {
        BipImageFormat format = BipImageFormat.parseNative(null, "1024*960", "1048576");
    }

    @Test(expected = ParseException.class)
    public void testParseNative_emptyEncoding() {
        BipImageFormat format = BipImageFormat.parseNative("", "1024*960", "1048576");
    }

    @Test(expected = ParseException.class)
    public void testParseNative_badEncoding() {
        BipImageFormat format = BipImageFormat.parseNative("JIF", "1024*960", "1048576");
    }

    @Test(expected = ParseException.class)
    public void testParseNative_noPixel() {
        BipImageFormat format = BipImageFormat.parseNative("JPEG", null, "1048576");
    }

    @Test(expected = ParseException.class)
    public void testParseNative_emptyPixel() {
        BipImageFormat format = BipImageFormat.parseNative("JPEG", "", "1048576");
    }

    @Test(expected = ParseException.class)
    public void testParseNative_badPixel() {
        BipImageFormat format = BipImageFormat.parseNative("JPEG", "abc*123", "1048576");
    }

    @Test(expected = NullPointerException.class)
    public void testCreateFormat_noEncoding() {
        BipImageFormat format = BipImageFormat.createNative(null, BipPixel.createFixed(1280, 1024),
                -1);
    }

    @Test(expected = NullPointerException.class)
    public void testCreateFormat_noPixel() {
        BipImageFormat format = BipImageFormat.createNative(new BipEncoding(BipEncoding.JPEG, null),
                null, -1);
    }
}
