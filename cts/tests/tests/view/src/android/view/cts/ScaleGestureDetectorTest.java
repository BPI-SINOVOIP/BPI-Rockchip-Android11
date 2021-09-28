/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ScaleGestureDetector.SimpleOnScaleGestureListener;
import android.view.ViewConfiguration;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ScaleGestureDetectorTest {

    private ScaleGestureDetector mScaleGestureDetector;
    private ScaleGestureDetectorCtsActivity mActivity;

    private boolean mGestureHasCrossedBackIntoSlopRadius;
    private long mFakeUptimeMs;
    private int mSpanSlop;

    @Rule
    public ActivityTestRule<ScaleGestureDetectorCtsActivity> mActivityRule =
            new ActivityTestRule<>(ScaleGestureDetectorCtsActivity.class);

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
        mScaleGestureDetector = mActivity.getScaleGestureDetector();
        mGestureHasCrossedBackIntoSlopRadius = false;
        mFakeUptimeMs = 0;
        // Value of mSpanSlop copied from ScaleGestureDetector.
        // Consider making it @VisibleForTesting.
        mSpanSlop = ViewConfiguration.get(mActivity).getScaledTouchSlop() * 2;
    }

    @UiThreadTest
    @Test
    public void testConstructor() {
        new ScaleGestureDetector(
                mActivity, new SimpleOnScaleGestureListener(), new Handler(Looper.getMainLooper()));
        new ScaleGestureDetector(mActivity, new SimpleOnScaleGestureListener());
    }

    @Test
    public void testAccessStylusScaleEnabled() {
        assertTrue(mScaleGestureDetector.isStylusScaleEnabled());
        mScaleGestureDetector.setStylusScaleEnabled(true);

        mScaleGestureDetector.setStylusScaleEnabled(false);
        assertFalse(mScaleGestureDetector.isStylusScaleEnabled());
    }

    @UiThreadTest
    @Test
    public void testGetScaleFactor_whenGestureCrossesBackInsideSlopRadius_returns1() {
        mScaleGestureDetector = new ScaleGestureDetector(mActivity,
                new ScaleGestureDetector.OnScaleGestureListener() {
                    @Override
                    public boolean onScale(ScaleGestureDetector detector) {
                        if (mGestureHasCrossedBackIntoSlopRadius) {
                            Assert.assertEquals(1.f, mScaleGestureDetector.getScaleFactor(),
                                    .0001F);
                        }
                        return true;
                    }

                    @Override
                    public boolean onScaleBegin(ScaleGestureDetector detector) {
                        return true;
                    }

                    @Override
                    public void onScaleEnd(ScaleGestureDetector detector) {}
                });
        float xValue = 500.f;
        float startY = 500.f;
        float slopRadius = mSpanSlop / 2.f;
        // Simulate a Double tap and drag outside the slop zone
        performTouch(MotionEvent.ACTION_DOWN, xValue, startY);
        performTouch(MotionEvent.ACTION_UP, xValue, startY);
        performTouch(MotionEvent.ACTION_DOWN, xValue, startY);
        performTouch(MotionEvent.ACTION_MOVE, xValue, startY + slopRadius + 1);
        // Continue drag with 2 subsequent events that cross back into slop zone
        performTouch(MotionEvent.ACTION_MOVE, xValue, startY + slopRadius - 1);
        mGestureHasCrossedBackIntoSlopRadius = true;
        performTouch(MotionEvent.ACTION_MOVE, xValue, startY + slopRadius - 2);
    }

    private void performTouch(int action, float x, float y) {
        mScaleGestureDetector.onTouchEvent(MotionEvent.obtain(0L, mFakeUptimeMs, action, x, y, 0));
        mFakeUptimeMs += 50L;
    }
}