/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.graphics.cts;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorSpace;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class CanvasTest {
    Canvas mCanvas;

    @Before
    public void setup() {
        Bitmap bm = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mCanvas = new Canvas(bm);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawColorXYZ() {
        mCanvas.drawColor(Color.convert(Color.BLUE, ColorSpace.get(ColorSpace.Named.CIE_XYZ)));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawColorLAB() {
        mCanvas.drawColor(Color.convert(Color.BLUE, ColorSpace.get(ColorSpace.Named.CIE_LAB)));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testDrawColorUnknown() {
        mCanvas.drawColor(-1L);
    }
}
