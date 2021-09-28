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
 * A test suite for the BipTransformation class
 */
@RunWith(AndroidJUnit4.class)
public class BipTransformationTest {

    @Test
    public void testCreateEmpty() {
        BipTransformation trans = new BipTransformation();
        Assert.assertFalse(trans.supportsAny());
        Assert.assertEquals(null, trans.toString());
    }

    @Test
    public void testAddTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("crop", trans.toString());

        trans.addTransformation(BipTransformation.STRETCH);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch crop", trans.toString());
    }

    @Test
    public void testAddExistingTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("crop", trans.toString());

        trans.addTransformation(BipTransformation.CROP);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("crop", trans.toString());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testAddOnlyInvalidTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.UNKNOWN);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testAddNewInvalidTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        trans.addTransformation(BipTransformation.UNKNOWN);
    }

    @Test
    public void testRemoveOnlyTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("crop", trans.toString());

        trans.removeTransformation(BipTransformation.CROP);
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.supportsAny());
        Assert.assertEquals(null, trans.toString());
    }

    @Test
    public void testRemoveOneTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        trans.addTransformation(BipTransformation.STRETCH);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch crop", trans.toString());

        trans.removeTransformation(BipTransformation.CROP);
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch", trans.toString());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testRemoveInvalidTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        trans.addTransformation(BipTransformation.STRETCH);
        trans.removeTransformation(BipTransformation.UNKNOWN);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch crop", trans.toString());
    }

    @Test
    public void testRemoveUnsupportedTransformation() {
        BipTransformation trans = new BipTransformation();
        trans.addTransformation(BipTransformation.CROP);
        trans.addTransformation(BipTransformation.STRETCH);
        trans.removeTransformation(BipTransformation.FILL);
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch crop", trans.toString());
    }

    @Test
    public void testParse_Stretch() {
        BipTransformation trans = new BipTransformation("stretch");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch", trans.toString());
    }

    @Test
    public void testParse_Crop() {
        BipTransformation trans = new BipTransformation("crop");
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("crop", trans.toString());
    }

    @Test
    public void testParse_Fill() {
        BipTransformation trans = new BipTransformation("Fill");
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("fill", trans.toString());
    }

    @Test
    public void testParse_StretchFill() {
        BipTransformation trans = new BipTransformation("stretch fill");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill", trans.toString());
    }

    @Test
    public void testParse_StretchCrop() {
        BipTransformation trans = new BipTransformation("stretch crop");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertEquals("stretch crop", trans.toString());
    }

    @Test
    public void testParse_FillCrop() {
        BipTransformation trans = new BipTransformation("fill crop");
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertEquals("fill crop", trans.toString());
    }

    @Test
    public void testParse_StretchFillCrop() {
        BipTransformation trans = new BipTransformation("stretch fill crop");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test
    public void testParse_CropFill() {
        BipTransformation trans = new BipTransformation("crop fill");
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertEquals("fill crop", trans.toString());
    }

    @Test
    public void testParse_CropFillStretch() {
        BipTransformation trans = new BipTransformation("crop fill stretch");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test
    public void testParse_CropFillStretchWithDuplicates() {
        BipTransformation trans = new BipTransformation("stretch crop fill fill crop stretch");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test
    public void testCreate_stretch() {
        BipTransformation trans = new BipTransformation(BipTransformation.STRETCH);
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch", trans.toString());
    }

    @Test
    public void testCreate_fill() {
        BipTransformation trans = new BipTransformation(BipTransformation.FILL);
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("fill", trans.toString());
    }

    @Test
    public void testCreate_crop() {
        BipTransformation trans = new BipTransformation(BipTransformation.CROP);
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("crop", trans.toString());
    }

    @Test
    public void testCreate_cropArray() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.CROP});
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("crop", trans.toString());
    }

    @Test
    public void testCreate_stretchFill() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.STRETCH,
                BipTransformation.FILL});
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill", trans.toString());
    }

    @Test
    public void testCreate_stretchFillCrop() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.STRETCH,
                BipTransformation.FILL, BipTransformation.CROP});
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test
    public void testCreate_stretchFillCropOrderChanged() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.CROP,
                BipTransformation.FILL, BipTransformation.STRETCH});
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreate_badTransformationOnly() {
        BipTransformation trans = new BipTransformation(-7);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testCreate_badTransformation() {
        BipTransformation trans = new BipTransformation(new int[]{BipTransformation.CROP, -7});
    }

    @Test
    public void testParse_badTransformationOnly() {
        BipTransformation trans = new BipTransformation("bad");
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals(null, trans.toString());
    }

    @Test
    public void testParse_badTransformationMixedIn() {
        BipTransformation trans = new BipTransformation("crop fill bad stretch");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch fill crop", trans.toString());
    }

    @Test
    public void testParse_badTransformationStart() {
        BipTransformation trans = new BipTransformation("bad crop fill");
        Assert.assertFalse(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertTrue(trans.isSupported(BipTransformation.FILL));
        Assert.assertTrue(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("fill crop", trans.toString());
    }

    @Test
    public void testParse_badTransformationEnd() {
        BipTransformation trans = new BipTransformation("stretch bad");
        Assert.assertTrue(trans.isSupported(BipTransformation.STRETCH));
        Assert.assertFalse(trans.isSupported(BipTransformation.FILL));
        Assert.assertFalse(trans.isSupported(BipTransformation.CROP));
        Assert.assertEquals("stretch", trans.toString());
    }
}
