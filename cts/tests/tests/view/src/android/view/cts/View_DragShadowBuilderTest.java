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
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.graphics.Canvas;
import android.graphics.Point;
import android.view.View.DragShadowBuilder;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link DragShadowBuilder}.
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class View_DragShadowBuilderTest {
    private DragShadowBuilder mBuilder;
    private MockView mView;

    @Rule
    public ActivityTestRule<CtsActivity> mActivityRule =
            new ActivityTestRule<>(CtsActivity.class);

    @Before
    public void setup() {
        mView = new MockView(mActivityRule.getActivity());
        mBuilder = new DragShadowBuilder(mView);
    }

    @Test
    public void testConstructor() {
        new DragShadowBuilder(mView);

        new DragShadowBuilder();
    }

    @Test
    public void testGetView() {
        assertSame(mView, mBuilder.getView());
    }

    @Test
    public void testOnProvideShadowMetrics() {
        Point outShadowSize = new Point();
        Point outShadowTouchPoint = new Point();

        mView.setLeftTopRightBottom(0, 0, 50, 50);
        mBuilder.onProvideShadowMetrics(outShadowSize, outShadowTouchPoint);

        assertEquals(mView.getWidth(), outShadowSize.x);
        assertEquals(mView.getHeight(), outShadowSize.y);

        assertEquals(outShadowSize.x / 2, outShadowTouchPoint.x);
        assertEquals(outShadowSize.y / 2, outShadowTouchPoint.y);
    }

    @Test
    public void testOnDrawShadow() {
        Canvas canvas = new Canvas();
        mBuilder.onDrawShadow(canvas);

        assertTrue(mView.hasCalledOnDraw());
    }
}
