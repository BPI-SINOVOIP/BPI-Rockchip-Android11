/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.uirendering.cts.testclasses;

import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.ColorVerifier;
import android.uirendering.cts.bitmapverifiers.RectVerifier;
import android.uirendering.cts.bitmapverifiers.SamplePointVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class LayoutTests extends ActivityTestBase {
    @Test
    public void testSimpleRedLayout() {
        createTest().addLayout(R.layout.simple_red_layout, null, false).runWithVerifier(
                new ColorVerifier(Color.RED));
    }

    @Test
    public void testSimpleRectLayout() {
        createTest().addLayout(R.layout.simple_rect_layout, null, false).runWithVerifier(
                new RectVerifier(Color.WHITE, Color.BLUE, new Rect(5, 5, 85, 85)));
    }

    @Test
    public void testTransformChildViewLayout() {
        createTest()
                // Verify skew tranform matrix is applied for child view.
                .addLayout(R.layout.skew_layout, null)
                .runWithVerifier(new SamplePointVerifier(
                        new Point[] {
                                new Point(20, 10),
                                new Point(0, TEST_HEIGHT - 1)
                        },
                        new int[] {
                                Color.RED,
                                Color.WHITE
                        }));
    }
}

