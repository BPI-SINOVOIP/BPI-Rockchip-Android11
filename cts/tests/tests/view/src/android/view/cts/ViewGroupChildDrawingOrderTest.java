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

package android.view.cts;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ViewGroupChildDrawingOrderTest {

    private Context mContext;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testPublicGetChildDrawingOrderWithDefaultOrder() {
        LinearLayout layout = new LinearLayout(mContext);
        layout.addView(new View(mContext));
        layout.addView(new View(mContext));
        layout.addView(new View(mContext));

        assertEquals(0, layout.getChildDrawingOrder(0));
        assertEquals(1, layout.getChildDrawingOrder(1));
        assertEquals(2, layout.getChildDrawingOrder(2));
    }

    @Test
    public void testPublicGetChildDrawingOrderWithCustomOrder() {
        LinearLayoutWithDrawingOrder layout = new LinearLayoutWithDrawingOrder(mContext);
        layout.addView(new View(mContext));
        layout.addView(new View(mContext));
        layout.addView(new View(mContext));
        layout.mChildDrawingOrder = new int[] {2, 0, 1};

        assertEquals(2, layout.getChildDrawingOrder(0));
        assertEquals(0, layout.getChildDrawingOrder(1));
        assertEquals(1, layout.getChildDrawingOrder(2));
    }

    private class LinearLayoutWithDrawingOrder extends LinearLayout {
        private int[] mChildDrawingOrder;
        LinearLayoutWithDrawingOrder(Context context) {
            super(context);
            setChildrenDrawingOrderEnabled(true);
        }

        @Override
        protected int getChildDrawingOrder(int childCount, int i) {
            return mChildDrawingOrder[i];
        }
    }

}

