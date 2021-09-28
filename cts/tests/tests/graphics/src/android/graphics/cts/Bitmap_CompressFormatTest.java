/*
 * Copyright (C) 2008 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Bitmap.Config;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.ByteArrayOutputStream;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class Bitmap_CompressFormatTest {
    @Test
    public void testValueOf(){
        assertEquals(CompressFormat.JPEG, CompressFormat.valueOf("JPEG"));
        assertEquals(CompressFormat.PNG, CompressFormat.valueOf("PNG"));
        assertEquals(CompressFormat.WEBP, CompressFormat.valueOf("WEBP"));
        assertEquals(CompressFormat.WEBP_LOSSY, CompressFormat.valueOf("WEBP_LOSSY"));
        assertEquals(CompressFormat.WEBP_LOSSLESS, CompressFormat.valueOf("WEBP_LOSSLESS"));
    }

    @Test
    public void testValues(){
        CompressFormat[] comFormat = CompressFormat.values();

        assertEquals(5, comFormat.length);
        assertEquals(CompressFormat.JPEG, comFormat[0]);
        assertEquals(CompressFormat.PNG, comFormat[1]);
        assertEquals(CompressFormat.WEBP, comFormat[2]);
        assertEquals(CompressFormat.WEBP_LOSSY, comFormat[3]);
        assertEquals(CompressFormat.WEBP_LOSSLESS, comFormat[4]);

        Bitmap b = Bitmap.createBitmap(10, 24, Config.ARGB_8888);
        for (CompressFormat format : comFormat) {
            assertTrue(b.compress(format, 24, new ByteArrayOutputStream()));
        }
    }
}
