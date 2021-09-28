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

package android.widget.cts;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Instrumentation;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.widget.SeekBar;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.WidgetTestUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Test {@link SeekBar}.
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class SeekBarTest {
    private Instrumentation mInstrumentation;
    private Activity mActivity;
    private SeekBar mSeekBar;

    @Rule
    public ActivityTestRule<SeekBarCtsActivity> mActivityRule =
            new ActivityTestRule<>(SeekBarCtsActivity.class);

    @Before
    public void setup() throws Throwable {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mActivity = mActivityRule.getActivity();
        mSeekBar = mActivity.findViewById(R.id.seekBar);
        if (mSeekBar.isAttachedToWindow()) {
            updateExclusionRects();
        } else {
            mSeekBar.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View view) {
                    mSeekBar.removeOnAttachStateChangeListener(this);
                    updateExclusionRects();
                }

                @Override
                public void onViewDetachedFromWindow(View view) {
                }
            });
        }
    }

    private void updateExclusionRects() {
        // "Mark" the left edge of our seek bar to be excluded from system gestures.
        // This does not need to be RTL-aware since the logic in the change listener
        // always injects the events from left to right.
        WindowInsets rootWindowInsets = mSeekBar.getRootWindowInsets();
        List<Rect> exclusion = new ArrayList<>();
        exclusion.add(new Rect(0, 0,
                rootWindowInsets.getSystemGestureInsets().left,
                mSeekBar.getHeight()));
        mSeekBar.setSystemGestureExclusionRects(exclusion);
    }

    @Test
    public void testConstructor() {
        new SeekBar(mActivity);

        new SeekBar(mActivity, null);

        new SeekBar(mActivity, null, android.R.attr.seekBarStyle);

        new SeekBar(mActivity, null, 0, android.R.style.Widget_DeviceDefault_SeekBar);

        new SeekBar(mActivity, null, 0, android.R.style.Widget_DeviceDefault_Light_SeekBar);

        new SeekBar(mActivity, null, 0, android.R.style.Widget_Material_SeekBar);

        new SeekBar(mActivity, null, 0, android.R.style.Widget_Material_Light_SeekBar);
    }

    @Test
    public void testSetOnSeekBarChangeListener() {
        SeekBar.OnSeekBarChangeListener mockChangeListener =
                mock(SeekBar.OnSeekBarChangeListener.class);

        mSeekBar.setOnSeekBarChangeListener(mockChangeListener);
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();
        int seekBarXY[] = new int[2];
        mSeekBar.getLocationOnScreen(seekBarXY);
        MotionEvent event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_DOWN,
                seekBarXY[0], seekBarXY[1], 0);
        mInstrumentation.sendPointerSync(event);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mSeekBar, null);
        verify(mockChangeListener, times(1)).onStartTrackingTouch(mSeekBar);
        // while starting to track, the progress is changed also
        verify(mockChangeListener, atLeastOnce()).onProgressChanged(eq(mSeekBar), anyInt(),
                eq(true));

        reset(mockChangeListener);
        downTime = SystemClock.uptimeMillis();
        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_MOVE,
                seekBarXY[0] + (mSeekBar.getWidth() >> 1), seekBarXY[1], 0);
        mInstrumentation.sendPointerSync(event);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mSeekBar, null);
        verify(mockChangeListener, atLeastOnce()).onProgressChanged(eq(mSeekBar), anyInt(),
                eq(true));

        reset(mockChangeListener);
        downTime = SystemClock.uptimeMillis();
        eventTime = SystemClock.uptimeMillis();
        event = MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP,
                seekBarXY[0] + (mSeekBar.getWidth() >> 1), seekBarXY[1], 0);
        mInstrumentation.sendPointerSync(event);
        WidgetTestUtils.runOnMainAndDrawSync(mActivityRule, mSeekBar, null);
        verify(mockChangeListener, times(1)).onStopTrackingTouch(mSeekBar);
    }
}
