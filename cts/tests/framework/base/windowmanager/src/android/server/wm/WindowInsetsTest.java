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

package android.server.wm;

import static android.view.WindowInsets.Type.captionBar;
import static android.view.WindowInsets.Type.displayCutout;
import static android.view.WindowInsets.Type.ime;
import static android.view.WindowInsets.Type.mandatorySystemGestures;
import static android.view.WindowInsets.Type.navigationBars;
import static android.view.WindowInsets.Type.statusBars;
import static android.view.WindowInsets.Type.systemGestures;
import static android.view.WindowInsets.Type.tappableElement;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.graphics.Insets;
import android.graphics.Rect;
import android.platform.test.annotations.Presubmit;
import android.view.DisplayCutout;
import android.view.WindowInsets;
import android.view.WindowInsets.Type;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Collections;

/**
 * Test {@link WindowInsets}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
@Presubmit
public class WindowInsetsTest {

    private static final DisplayCutout CUTOUT = new DisplayCutout(new Rect(0, 10, 0, 0),
            Collections.singletonList(new Rect(5, 0, 15, 10)));
    private static final DisplayCutout CUTOUT2 = new DisplayCutout(new Rect(0, 15, 0, 0),
            Collections.singletonList(new Rect(5, 0, 15, 15)));
    private static final int INSET_LEFT = 1;
    private static final int INSET_TOP = 2;
    private static final int INSET_RIGHT = 3;
    private static final int INSET_BOTTOM = 4;

    @Test
    public void testBuilder() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT)
                .build();

        assertEquals(Insets.of(1, 2, 3, 4), insets.getSystemWindowInsets());
        assertEquals(Insets.of(5, 6, 7, 8), insets.getStableInsets());
        assertEquals(Insets.of(9, 10, 11, 12), insets.getSystemGestureInsets());
        assertEquals(Insets.of(13, 14, 15, 16), insets.getMandatorySystemGestureInsets());
        assertEquals(Insets.of(17, 18, 19, 20), insets.getTappableElementInsets());
        assertSame(CUTOUT, insets.getDisplayCutout());
        assertEquals(getCutoutSafeInsets(insets), insets.getInsets(Type.displayCutout()));
    }

    @Test
    public void testBuilder_copy() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT)
                .build();
        final WindowInsets copy = new WindowInsets.Builder(insets).build();

        assertEquals(insets, copy);
    }

    @Test
    public void testBuilder_consumed() {
        final WindowInsets insets = new WindowInsets.Builder()
                .build();

        assertFalse(insets.hasSystemWindowInsets());
        assertFalse(insets.hasStableInsets());
        assertEquals(Insets.NONE, insets.getSystemGestureInsets());
        assertNull(insets.getDisplayCutout());
        assertTrue(insets.isConsumed());
    }

    @Test
    public void testBuilder_emptyCutout() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setDisplayCutout(null)
                .build();

        assertFalse(insets.hasSystemWindowInsets());
        assertFalse(insets.hasStableInsets());
        assertEquals(Insets.NONE, insets.getSystemGestureInsets());

        assertNull(insets.getDisplayCutout());
        assertFalse(insets.isConsumed());
        assertTrue(insets.consumeDisplayCutout().isConsumed());
        assertEquals(Insets.NONE, insets.getInsets(Type.displayCutout()));
    }

    @Test
    public void testBuilder_producesImmutableWindowInsets() {
        final WindowInsets.Builder builder = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT);
        final WindowInsets insets = builder.build();

        builder.setSystemWindowInsets(Insets.NONE);
        builder.setStableInsets(Insets.NONE);
        builder.setDisplayCutout(null);
        builder.setSystemGestureInsets(Insets.NONE);
        builder.setMandatorySystemGestureInsets(Insets.NONE);
        builder.setTappableElementInsets(Insets.NONE);

        assertEquals(Insets.of(1, 2, 3, 4), insets.getSystemWindowInsets());
        assertEquals(Insets.of(5, 6, 7, 8), insets.getStableInsets());
        assertSame(CUTOUT, insets.getDisplayCutout());
        assertEquals(getCutoutSafeInsets(insets), insets.getInsets(Type.displayCutout()));
    }

    @Test
    public void testBuilder_typeMap() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setInsets(statusBars(), Insets.of(0, 50, 0, 0))
                .setInsets(navigationBars(), Insets.of(0, 0, 0, 100))
                .setInsets(tappableElement(), Insets.of(0, 10, 0, 10))
                .setInsets(mandatorySystemGestures(), Insets.of(0, 20, 0, 20))
                .setInsets(systemGestures(), Insets.of(0, 30, 0, 30))
                .setInsets(displayCutout(), Insets.of(0, 5, 0, 0))
                .setInsets(captionBar(), Insets.of(0, 50, 0, 0))
                .setInsets(ime(), Insets.of(0, 0, 0, 300))
                .build();
        assertEquals(Insets.of(0, 50, 0, 0), insets.getInsets(statusBars()));
        assertEquals(Insets.of(0, 0, 0, 100), insets.getInsets(navigationBars()));
        assertEquals(Insets.of(0, 10, 0, 10), insets.getInsets(tappableElement()));
        assertEquals(Insets.of(0, 20, 0, 20), insets.getInsets(mandatorySystemGestures()));
        assertEquals(Insets.of(0, 30, 0, 30), insets.getInsets(systemGestures()));
        assertEquals(Insets.of(0, 5, 0, 0), insets.getInsets(displayCutout()));
        assertEquals(Insets.of(0, 50, 0, 0), insets.getInsets(captionBar()));
        assertEquals(Insets.of(0, 50, 0, 100), insets.getSystemWindowInsets());
        assertEquals(Insets.of(0, 0, 0, 300), insets.getInsets(ime()));
    }

    @Test
    public void testBuilder_typeMapIgnoringVisibility() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setInsetsIgnoringVisibility(statusBars(), Insets.of(0, 50, 0, 0))
                .setInsetsIgnoringVisibility(navigationBars(), Insets.of(0, 0, 0, 100))
                .setInsetsIgnoringVisibility(tappableElement(), Insets.of(0, 10, 0, 10))
                .setInsetsIgnoringVisibility(mandatorySystemGestures(), Insets.of(0, 20, 0, 20))
                .setInsetsIgnoringVisibility(systemGestures(), Insets.of(0, 30, 0, 30))
                .setInsetsIgnoringVisibility(displayCutout(), Insets.of(0, 5, 0, 0))
                .setInsetsIgnoringVisibility(captionBar(), Insets.of(0, 50, 0, 0))
                .build();
        assertEquals(Insets.of(0, 50, 0, 0),
                insets.getInsetsIgnoringVisibility(statusBars()));
        assertEquals(Insets.of(0, 0, 0, 100),
                insets.getInsetsIgnoringVisibility(navigationBars()));
        assertEquals(Insets.of(0, 10, 0, 10),
                insets.getInsetsIgnoringVisibility(tappableElement()));
        assertEquals(Insets.of(0, 20, 0, 20),
                insets.getInsetsIgnoringVisibility(mandatorySystemGestures()));
        assertEquals(Insets.of(0, 30, 0, 30),
                insets.getInsetsIgnoringVisibility(systemGestures()));
        assertEquals(Insets.of(0, 5, 0, 0),
                insets.getInsetsIgnoringVisibility(displayCutout()));
        assertEquals(Insets.of(0, 50, 0, 0),
                insets.getInsetsIgnoringVisibility(captionBar()));
        assertEquals(Insets.of(0, 50, 0, 100), insets.getStableInsets());

        Exception exception = null;
        try {
            new WindowInsets.Builder().setInsetsIgnoringVisibility(ime(), Insets.NONE);
        } catch (Exception e) {
            exception = e;
        }
        assertNotNull(exception);

        exception = null;
        try {
            insets.getInsetsIgnoringVisibility(ime());
        } catch (Exception e) {
            exception = e;
        }
        assertNotNull(exception);
    }

    @Test
    public void testBuilder_compatInsets() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(0, 50, 30, 10))
                .build();
        assertEquals(Insets.of(0, 50, 0, 0), insets.getInsets(statusBars()));
        assertEquals(Insets.of(0, 0, 30, 10), insets.getInsets(navigationBars()));
    }

    @Test
    public void testBuilder_visibility() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setInsets(navigationBars(), Insets.of(0, 0, 0, 100))
                .setInsets(ime(), Insets.of(0, 0, 0, 300))
                .setVisible(navigationBars(), true)
                .setVisible(ime(), true)
                .build();
        assertTrue(insets.isVisible(navigationBars()));
        assertTrue(insets.isVisible(navigationBars() | ime()));
        assertFalse(insets.isVisible(navigationBars() | statusBars()));
    }

    @Test
    public void testEquality() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insets2 = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        assertNotSame("Test setup failed, insets and insets2 should not be identical",
                insets, insets2);

        assertEquals(insets, insets2);
        assertEquals(insets.hashCode(), insets2.hashCode());
    }

    @Test
    public void testInEquality_consuming() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        assertNotEquals(insets, insets.consumeSystemWindowInsets());
        assertNotEquals(insets, insets.consumeDisplayCutout());
    }

    @Test
    public void testConsume_doesntChangeVisibility() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setInsets(ime(), Insets.of(0, 0, 0, 300))
                .setVisible(ime(), true)
                .build();

        final WindowInsets consumed = insets.consumeSystemWindowInsets();

        assertTrue(consumed.isVisible(ime()));
    }

    @Test
    public void testConsume_systemWindowInsets() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets consumed = insets.consumeSystemWindowInsets();

        assertEquals(Insets.NONE, consumed.getSystemWindowInsets());
        assertEquals(Insets.NONE, consumed.getStableInsets());
        assertEquals(insets.getDisplayCutout(), consumed.getDisplayCutout());
        assertEquals(Insets.NONE, consumed.getSystemGestureInsets());
        assertEquals(Insets.NONE, consumed.getMandatorySystemGestureInsets());
        assertEquals(Insets.NONE, consumed.getTappableElementInsets());
        assertEquals(Insets.NONE, consumed.getInsets(Type.displayCutout()));
    }

    @Test
    public void testConsume_stableInsets() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets consumed = insets.consumeStableInsets();
        assertSame(insets, consumed);
    }

    @Test
    public void testConsume_displayCutout() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets consumed = insets.consumeDisplayCutout();

        assertEquals(insets.getSystemWindowInsets(), consumed.getSystemWindowInsets());
        assertEquals(insets.getStableInsets(), consumed.getStableInsets());
        assertNull(consumed.getDisplayCutout());
        assertEquals(insets.getSystemGestureInsets(), consumed.getSystemGestureInsets());
        assertEquals(insets.getMandatorySystemGestureInsets(), consumed.getMandatorySystemGestureInsets());
        assertEquals(insets.getTappableElementInsets(), consumed.getTappableElementInsets());
        assertEquals(
                insets.getInsets(Type.displayCutout()), consumed.getInsets(Type.displayCutout()));
    }

    @Test
    public void testConsistency_individualSides() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        assertEquals(insets.getSystemWindowInsets(), Insets.of(
                insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom()));
        assertEquals(insets.getStableInsets(), Insets.of(
                insets.getStableInsetLeft(), insets.getStableInsetTop(),
                insets.getStableInsetRight(), insets.getStableInsetBottom()));
    }

    @Test
    @SuppressWarnings("deprecation")
    public void testReplacingConsumedSystemWindowInset_staysZeroAndConsumed() {
        final WindowInsets consumed = new WindowInsets.Builder().build();
        final WindowInsets replaced = consumed.replaceSystemWindowInsets(new Rect(1, 2, 3, 4));

        assertEquals(Insets.NONE, replaced.getSystemWindowInsets());
        assertTrue(replaced.isConsumed());
    }

    @Test
    @SuppressWarnings("deprecation")
    public void testReplacingSystemWindowInsets_works() {
        final WindowInsets replaced = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(100, 200, 300, 400))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build()
                .replaceSystemWindowInsets(new Rect(1, 2, 3, 4));
        final WindowInsets expected = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        assertEquals(expected, replaced);
    }

    @Test
    @SuppressWarnings("deprecation")
    public void testReplacingSystemWindowInsets_consistencyAcrossOverloads() {
        final Rect newInsets = new Rect(100, 200, 300, 400);
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        assertEquals(insets.replaceSystemWindowInsets(newInsets),
                insets.replaceSystemWindowInsets(newInsets.left, newInsets.top, newInsets.right,
                        newInsets.bottom));
    }

    @Test
    public void testInEquality_difference() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetsChangedSysWindowInsets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(10, 20, 30, 40))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetsChangedStableInsets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(50, 60, 70, 80))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetsChangedCutout = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT2).build();

        final WindowInsets insetsChangedGesture = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(90, 100, 110, 120))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetsChangedMandatoryGesture = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(130, 140, 150, 160))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetsChangedTappableElement = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(1, 2, 3, 4))
                .setStableInsets(Insets.of(5, 6, 7, 8))
                .setSystemGestureInsets(Insets.of(9, 10, 11, 12))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(170, 180, 190, 200))
                .setDisplayCutout(CUTOUT).build();

        assertNotEquals(insets, insetsChangedSysWindowInsets);
        assertNotEquals(insets, insetsChangedStableInsets);
        assertNotEquals(insets, insetsChangedCutout);
        assertNotEquals(insets, insetsChangedGesture);
        assertNotEquals(insets, insetsChangedMandatoryGesture);
        assertNotEquals(insets, insetsChangedTappableElement);
    }

    @Test
    public void testInset() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(10, 20, 30, 40))
                .setStableInsets(Insets.of(50, 60, 70, 80))
                .setSystemGestureInsets(Insets.of(90, 100, 110, 120))
                .setMandatorySystemGestureInsets(Insets.of(130, 140, 150, 160))
                .setTappableElementInsets(Insets.of(170, 180, 190, 200))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetInsets = insets.inset(
                INSET_LEFT, INSET_TOP, INSET_RIGHT, INSET_BOTTOM);

        assertEquals(applyInset(insets.getSystemWindowInsets()),
                insetInsets.getSystemWindowInsets());
        assertEquals(applyInset(insets.getStableInsets()), insetInsets.getStableInsets());
        assertEquals(applyInset(insets.getSystemGestureInsets()),
                insetInsets.getSystemGestureInsets());
        assertEquals(applyInset(insets.getMandatorySystemGestureInsets()),
                insetInsets.getMandatorySystemGestureInsets());
        assertEquals(applyInset(insets.getTappableElementInsets()),
                insetInsets.getTappableElementInsets());
        assertEquals(applyInset(getCutoutSafeInsets(insets)), getCutoutSafeInsets(insetInsets));
        assertEquals(applyInset(getCutoutSafeInsets(insets)),
                insetInsets.getInsets(Type.displayCutout()));
    }

    @Test
    public void testInset_clipsToZero() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(10, 20, 30, 40))
                .setStableInsets(Insets.of(50, 60, 70, 80))
                .setSystemGestureInsets(Insets.of(90, 100, 110, 120))
                .setMandatorySystemGestureInsets(Insets.of(13, 14, 15, 16))
                .setTappableElementInsets(Insets.of(17, 18, 19, 20))
                .setDisplayCutout(CUTOUT).build();

        final WindowInsets insetInsets = insets.inset(1000, 1000, 1000, 1000);

        assertEquals(Insets.NONE, insetInsets.getSystemWindowInsets());
        assertEquals(Insets.NONE, insetInsets.getStableInsets());
        assertEquals(Insets.NONE, insetInsets.getSystemGestureInsets());
        assertEquals(Insets.NONE, insetInsets.getMandatorySystemGestureInsets());
        assertEquals(Insets.NONE, insetInsets.getTappableElementInsets());
        assertNull(insetInsets.getDisplayCutout());
        assertEquals(Insets.NONE, insetInsets.getInsets(Type.displayCutout()));
    }

    @Test
    public void testConsumed_copy() {
        final WindowInsets insets = new WindowInsets.Builder()
                .setSystemWindowInsets(Insets.of(10, 20, 30, 40))
                .setStableInsets(Insets.of(50, 60, 70, 80))
                .build();

        final WindowInsets consumed = insets.consumeSystemWindowInsets().consumeStableInsets();
        final WindowInsets copy = new WindowInsets(consumed);
        assertTrue(copy.isConsumed());
    }

    @Test
    public void testConsumedInstance() {
        assertTrue(WindowInsets.CONSUMED.isConsumed());
    }

    private static Insets applyInset(Insets res) {
        return Insets.of(Math.max(0, res.left - INSET_LEFT),
                Math.max(0, res.top - INSET_TOP),
                Math.max(0, res.right - INSET_RIGHT),
                Math.max(0, res.bottom - INSET_BOTTOM));
    }

    private static Insets getCutoutSafeInsets(WindowInsets insets) {
        final DisplayCutout dc = insets.getDisplayCutout();
        return Insets.of(dc.getSafeInsetLeft(), dc.getSafeInsetTop(), dc.getSafeInsetRight(),
                dc.getSafeInsetBottom());
    }
}
