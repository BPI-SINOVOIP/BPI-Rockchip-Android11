/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

@RunWith(JUnit4.class)
public class PerformanceTestTest {

    private InputStream createImage(int color) throws IOException {
        int width = 10;
        int height = 10;
        BufferedImage img = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                img.setRGB(i, j, color);
            }
        }
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        ImageIO.write(img, "png", outputStream);
        return new ByteArrayInputStream(outputStream.toByteArray());
    }

    @Test
    public void testBlackImage() throws IOException {
        InputStream img = createImage(0xff000000);
        assertTrue(PerformanceTest.isImageBlack(img));
    }

    @Test
    public void testTransparentImage() throws IOException {
        InputStream img = createImage(0x00ffffff);
        assertTrue(PerformanceTest.isImageBlack(img));
    }

    @Test
    public void testAlmostBlackImage() throws IOException {
        InputStream img = createImage(0xff000001);
        assertFalse(PerformanceTest.isImageBlack(img));
    }
}