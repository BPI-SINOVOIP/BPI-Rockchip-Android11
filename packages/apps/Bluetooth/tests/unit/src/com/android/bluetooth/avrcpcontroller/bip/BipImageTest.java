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

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.InputStream;

/**
 * A test suite for the BipImage class
 */
@RunWith(AndroidJUnit4.class)
public class BipImageTest {
    private static String sImageHandle = "123456789";
    private Context mTargetContext;
    private Resources mTestResources;

    @Before
    public void setUp() {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        try {
            mTestResources = mTargetContext.getPackageManager()
                    .getResourcesForApplication("com.android.bluetooth.tests");
        } catch (PackageManager.NameNotFoundException e) {
            Assert.fail("Setup Failure Unable to get resources" + e.toString());
        }
    }

    @Test
    public void testParseImage_200by200() {
        InputStream imageInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_200_200);
        BipImage image = new BipImage(sImageHandle, imageInputStream);

        InputStream expectedInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_200_200);
        Bitmap bitmap = BitmapFactory.decodeStream(expectedInputStream);

        Assert.assertEquals(sImageHandle, image.getImageHandle());
        Assert.assertTrue(bitmap.sameAs(image.getImage()));
    }

    @Test
    public void testParseImage_600by600() {
        InputStream imageInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_600_600);
        BipImage image = new BipImage(sImageHandle, imageInputStream);

        InputStream expectedInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_600_600);
        Bitmap bitmap = BitmapFactory.decodeStream(expectedInputStream);

        Assert.assertEquals(sImageHandle, image.getImageHandle());
        Assert.assertTrue(bitmap.sameAs(image.getImage()));
    }

    @Test
    public void testMakeFromImage_200by200() {
        InputStream imageInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_200_200);
        Bitmap bitmap = BitmapFactory.decodeStream(imageInputStream);
        BipImage image = new BipImage(sImageHandle, bitmap);
        Assert.assertEquals(sImageHandle, image.getImageHandle());
        Assert.assertTrue(bitmap.sameAs(image.getImage()));
    }

    @Test
    public void testMakeFromImage_600by600() {
        InputStream imageInputStream = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_600_600);
        Bitmap bitmap = BitmapFactory.decodeStream(imageInputStream);
        BipImage image = new BipImage(sImageHandle, bitmap);
        Assert.assertEquals(sImageHandle, image.getImageHandle());
        Assert.assertTrue(bitmap.sameAs(image.getImage()));
    }
}
