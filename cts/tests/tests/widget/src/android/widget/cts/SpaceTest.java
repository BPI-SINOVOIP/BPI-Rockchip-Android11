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

package android.widget.cts;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;
import android.widget.Space;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.WidgetTestUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link Space}.
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class SpaceTest {
    private Activity mActivity;
    private Space mSpace;

    @Rule
    public ActivityTestRule<CtsActivity> mActivityRule =
            new ActivityTestRule<>(CtsActivity.class);

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
        mSpace = new Space(mActivity);
    }

    @UiThreadTest
    @Test
    public void testConstructor() {
        new Space(mActivity);

        new Space(mActivity, null);

        new Space(mActivity, null, 0);

        new Space(mActivity, null, 0, android.R.style.Widget);
    }

    @UiThreadTest
    @Test
    public void testInvisibleDefault() {
        assertEquals(View.INVISIBLE, mSpace.getVisibility());
    }

    @UiThreadTest
    @Test
    public void testDrawNothing() {
        Bitmap bitmap = Bitmap.createBitmap(200, 200, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Bitmap expected = bitmap.copy(Bitmap.Config.ARGB_8888, false);
        mSpace.measure(100, 100);
        mSpace.layout(0, 0, 100, 100);
        mSpace.draw(canvas);
        WidgetTestUtils.assertEquals(expected, bitmap);
    }

    @UiThreadTest
    @Test
    public void testMeasureSpecs() {
        mSpace.setMinimumWidth(100);
        mSpace.setMinimumHeight(100);

        mSpace.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
        assertEquals(100, mSpace.getMeasuredWidth());
        assertEquals(100, mSpace.getMeasuredHeight());

        mSpace.measure(200 | View.MeasureSpec.AT_MOST,
                200 | View.MeasureSpec.AT_MOST);
        assertEquals(100, mSpace.getMeasuredWidth());
        assertEquals(100, mSpace.getMeasuredHeight());

        mSpace.measure(200 | View.MeasureSpec.EXACTLY,
                200 | View.MeasureSpec.EXACTLY);
        assertEquals(200, mSpace.getMeasuredWidth());
        assertEquals(200, mSpace.getMeasuredHeight());
    }
}
