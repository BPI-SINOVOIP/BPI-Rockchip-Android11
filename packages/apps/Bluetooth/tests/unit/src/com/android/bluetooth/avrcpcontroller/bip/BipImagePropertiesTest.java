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

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;

/**
 * A test suite for the BipImageProperties class
 */
@RunWith(AndroidJUnit4.class)
public class BipImagePropertiesTest {
    private static String sImageHandle = "123456789";
    private static final String sXmlDocDecl =
            "<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n";
    private static String sXmlString = sXmlDocDecl
            + "<image-properties version=\"1.0\" handle=\"123456789\">\n"
            + "    <native encoding=\"JPEG\" pixel=\"1280*1024\" size=\"1048576\" />\n"
            + "    <variant encoding=\"JPEG\" pixel=\"640*480\" />\n"
            + "    <variant encoding=\"GIF\" pixel=\"80*60-640*480\" "
            + "transformation=\"stretch fill crop\" />\n"
            + "    <variant encoding=\"JPEG\" pixel=\"150**-600*120\" />\n"
            + "    <attachment content-type=\"text/plain\" name=\"ABCD1234.txt\" size=\"5120\" />\n"
            + "    <attachment content-type=\"audio/basic\" name=\"ABCD1234.wav\" size=\"102400\" "
            + "/>\n"
            + "</image-properties>\n";

    private InputStream toUtf8Stream(String s) {
        try {
            return new ByteArrayInputStream(s.getBytes("UTF-8"));
        } catch (UnsupportedEncodingException e) {
            return null;
        }
    }

    @Test
    public void testParseProperties() {
        InputStream stream = toUtf8Stream(sXmlString);
        BipImageProperties properties = new BipImageProperties(stream);
        Assert.assertEquals(sImageHandle, properties.getImageHandle());
        Assert.assertEquals(sXmlString, properties.toString());
    }

    @Test
    public void testCreateProperties() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.STRETCH);
        trans.addTransformation(BipTransformation.CROP);
        trans.addTransformation(BipTransformation.FILL);

        BipImageProperties.Builder builder = new BipImageProperties.Builder();
        builder.setImageHandle(sImageHandle);
        builder.addNativeFormat(BipImageFormat.createNative(new BipEncoding(BipEncoding.JPEG, null),
                BipPixel.createFixed(1280, 1024), 1048576));

        builder.addVariantFormat(
                BipImageFormat.createVariant(
                    new BipEncoding(BipEncoding.JPEG, null),
                    BipPixel.createFixed(640, 480), -1, null));
        builder.addVariantFormat(
                BipImageFormat.createVariant(
                    new BipEncoding(BipEncoding.GIF, null),
                    BipPixel.createResizableModified(80, 60, 640, 480), -1, trans));
        builder.addVariantFormat(
                BipImageFormat.createVariant(
                    new BipEncoding(BipEncoding.JPEG, null),
                    BipPixel.createResizableFixed(150, 600, 120), -1, null));

        builder.addAttachment(
                new BipAttachmentFormat("text/plain", null, "ABCD1234.txt", 5120, null, null));
        builder.addAttachment(
                new BipAttachmentFormat("audio/basic", null, "ABCD1234.wav", 102400, null, null));

        BipImageProperties properties = builder.build();
        Assert.assertEquals(sImageHandle, properties.getImageHandle());
        Assert.assertEquals(sXmlString, properties.toString());
    }
}
