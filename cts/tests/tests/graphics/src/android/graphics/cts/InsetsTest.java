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

package android.graphics.cts;

import static org.junit.Assert.assertEquals;

import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Parcel;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InsetsTest {

    @Test
    public void testEmptyInsets() {
        assertEquals(Insets.NONE, Insets.of(0, 0, 0, 0));
    }

    @Test
    public void testInsetsAppliedInOrder() {
        Insets insets = Insets.of(1, 2, 3, 4);
        assertEquals(1, insets.left);
        assertEquals(2, insets.top);
        assertEquals(3, insets.right);
        assertEquals(4, insets.bottom);
    }

    @Test
    public void testInsetsFromNullRect() {
        assertEquals(Insets.NONE, Insets.of(null));
    }

    @Test
    public void testInsetsFromRect() {
        Rect rect = new Rect(1, 2, 3, 4);
        Insets insets = Insets.of(rect);
        assertEquals(1, insets.left);
        assertEquals(2, insets.top);
        assertEquals(3, insets.right);
        assertEquals(4, insets.bottom);
    }

    @Test
    public void testInsetsEquality() {
        Rect rect = new Rect(10, 20, 30, 40);
        Insets insets1 = Insets.of(rect);
        Insets insets2 = Insets.of(10, 20, 30, 40);
        assertEquals(insets1, insets2);
    }

    @Test
    public void testAdd() {
        Insets insets1 = Insets.of(10, 20, 30, 40);
        Insets insets2 = Insets.of(50, 60, 70, 80);
        assertEquals(Insets.add(insets1, insets2), Insets.of(60, 80, 100, 120));
    }

    @Test
    public void testSubtract() {
        Insets insets1 = Insets.of(10, 20, 30, 40);
        Insets insets2 = Insets.of(50, 70, 90, 110);
        assertEquals(Insets.subtract(insets2, insets1), Insets.of(40, 50, 60, 70));
    }

    @Test
    public void testMin() {
        Insets insets1 = Insets.of(1, 10, 100, 1000);
        Insets insets2 = Insets.of(1000, 100, 10, 1);
        assertEquals(Insets.min(insets2, insets1), Insets.of(1, 10, 10, 1));
    }

    @Test
    public void testMax() {
        Insets insets1 = Insets.of(1, 10, 100, 1000);
        Insets insets2 = Insets.of(1000, 100, 10, 1);
        assertEquals(Insets.max(insets2, insets1), Insets.of(1000, 100, 100, 1000));
    }

    @Test
    public void testParcel() {
        Insets inset = Insets.of(1, 3, 5, 7);
        final Parcel parcel = Parcel.obtain();
        inset.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        assertEquals(inset, Insets.CREATOR.createFromParcel(parcel));
        assertEquals(parcel.dataSize(), parcel.dataPosition());
        parcel.recycle();
    }
}
