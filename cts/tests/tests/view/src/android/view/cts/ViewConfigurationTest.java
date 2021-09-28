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

package android.view.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Point;
import android.util.TypedValue;
import android.view.Display;
import android.view.ViewConfiguration;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link ViewConfiguration}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class ViewConfigurationTest {
    @Test
    public void testStaticValues() {
        ViewConfiguration.getScrollBarSize();
        ViewConfiguration.getFadingEdgeLength();
        ViewConfiguration.getPressedStateDuration();
        ViewConfiguration.getLongPressTimeout();
        ViewConfiguration.getTapTimeout();
        ViewConfiguration.getJumpTapTimeout();
        ViewConfiguration.getEdgeSlop();
        ViewConfiguration.getTouchSlop();
        ViewConfiguration.getWindowTouchSlop();
        ViewConfiguration.getMinimumFlingVelocity();
        ViewConfiguration.getMaximumFlingVelocity();
        ViewConfiguration.getMaximumDrawingCacheSize();
        ViewConfiguration.getZoomControlsTimeout();
        ViewConfiguration.getGlobalActionKeyTimeout();
        ViewConfiguration.getScrollFriction();
        ViewConfiguration.getScrollBarFadeDuration();
        ViewConfiguration.getScrollDefaultDelay();
        ViewConfiguration.getDoubleTapTimeout();
        ViewConfiguration.getKeyRepeatTimeout();
        ViewConfiguration.getKeyRepeatDelay();
        ViewConfiguration.getDefaultActionModeHideDuration();
    }

    @Test
    public void testConstructor() {
        new ViewConfiguration();
    }

    @Test
    public void testInstanceValues() {
        Context context = InstrumentationRegistry.getTargetContext();
        ViewConfiguration vc = ViewConfiguration.get(context);
        assertNotNull(vc);

        vc.getScaledDoubleTapSlop();
        vc.getScaledEdgeSlop();
        vc.getScaledFadingEdgeLength();
        vc.getScaledMaximumDrawingCacheSize();
        vc.getScaledMaximumFlingVelocity();
        vc.getScaledMinimumFlingVelocity();
        vc.getScaledOverflingDistance();
        vc.getScaledOverscrollDistance();
        vc.getScaledPagingTouchSlop();
        vc.getScaledScrollBarSize();
        vc.getScaledHorizontalScrollFactor();
        vc.getScaledVerticalScrollFactor();
        vc.getScaledTouchSlop();
        vc.getScaledWindowTouchSlop();
        vc.hasPermanentMenuKey();

        float pixelsToMmRatio = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_MM, 1,
                context.getResources().getDisplayMetrics());

        // Verify that the min scaling span size is reasonable.
        float scaledMinScalingSpanMm = vc.getScaledMinimumScalingSpan() / pixelsToMmRatio;
        assertTrue(scaledMinScalingSpanMm > 0);
        assertTrue(scaledMinScalingSpanMm < 40.5); // 1.5 times the recommended size of 27mm
    }

    @Test
    public void testExceptionsThrown() {
        ViewConfiguration vc = new ViewConfiguration();
        boolean correctExceptionThrown = false;
        try {
            vc.getScaledMinimumScalingSpan();
        } catch (IllegalStateException e) {
            if (e.getMessage().equals("Min scaling span cannot be determined when this "
                    + "method is called on a ViewConfiguration that was instantiated using a "
                    + "constructor with no Context parameter")) {
                correctExceptionThrown = true;
            }
        }
        assertTrue(correctExceptionThrown);
    }

    /**
     * The purpose of the ambiguous gesture multiplier is to potentially increase the touch slop
     * and the long press timeout to allow the gesture classifier an additional window to
     * make the classification. Therefore, this multiplier should be always greater or equal to 1.
     */
    @Test
    public void testGetAmbiguousGestureMultiplier() {
        final float staticMultiplier = ViewConfiguration.getAmbiguousGestureMultiplier();
        assertTrue(staticMultiplier >= 1);

        ViewConfiguration vc = ViewConfiguration.get(InstrumentationRegistry.getTargetContext());
        final float instanceMultiplier = vc.getAmbiguousGestureMultiplier();
        assertTrue(instanceMultiplier >= 1);
    }

    @Test
    public void testGetScaledAmbiguousGestureMultiplier() {
        ViewConfiguration vc = ViewConfiguration.get(InstrumentationRegistry.getTargetContext());
        final float instanceMultiplier = vc.getScaledAmbiguousGestureMultiplier();
        assertTrue(instanceMultiplier >= 1);
    }

    @Test
    public void testGetMaximumDrawingCacheSize() {
        Context context = InstrumentationRegistry.getTargetContext();
        ViewConfiguration vc = ViewConfiguration.get(context);
        assertNotNull(vc);

        // Should be at least the size of the screen we're supposed to draw into.
        final WindowManager win = context.getSystemService(WindowManager.class);
        final Display display = win.getDefaultDisplay();
        final Point size = new Point();
        display.getSize(size);
        assertTrue(vc.getScaledMaximumDrawingCacheSize() >= size.x * size.y * 4);

        // This deprecated value should just be what it's historically hardcoded to be.
        assertEquals(480 * 800 * 4, vc.getMaximumDrawingCacheSize());
    }
}
