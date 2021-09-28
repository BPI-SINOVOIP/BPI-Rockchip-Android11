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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;

import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.google.common.collect.Lists;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/**
 * Test {@link View#setSystemGestureExclusionRects} and the full-tree reporting of transformed
 * changes by {@link ViewTreeObserver#addOnSystemGestureExclusionRectsChangedListener(Consumer)}.
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class SystemGestureExclusionRectsTest {
    @Rule
    public ActivityTestRule<SystemGestureExclusionActivity> mActivityRule =
            new ActivityTestRule<>(SystemGestureExclusionActivity.class);

    @Test
    public void staticView() throws Throwable {
        final Activity activity = mActivityRule.getActivity();

        final CountDownLatch received = new CountDownLatch(1);
        final List<Rect>[] holder = new List[1];
        final Rect expected = new Rect();

        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            final ViewTreeObserver vto = v.getViewTreeObserver();
            vto.addOnSystemGestureExclusionRectsChangedListener(rects -> {
                int[] point = new int[2];
                v.getLocationInWindow(point);
                expected.left = point[0];
                expected.top = point[1];
                expected.right = expected.left + v.getWidth();
                expected.bottom = expected.top + v.getHeight();

                holder[0] = rects;
                received.countDown();
            });
            v.setSystemGestureExclusionRects(
                    Lists.newArrayList(new Rect(0, 0, 5, 5)));
        });

        assertTrue("didn't receive exclusion rects", received.await(3, SECONDS));

        assertEquals("view position reference width " + expected, 5, expected.width());
        assertEquals("view position reference height", 5, expected.height());
        assertNotNull("exclusion rects reported", holder[0]);
        assertEquals("one exclusion rect", 1, holder[0].size());
        assertEquals("view bounds equals exclusion rect", expected, holder[0].get(0));
    }

    /**
     * Animate a view from X=0 to X=30px and verify that the static exclusion rect follows.
     */
    @Test
    public void animatingView() throws Throwable {
        final Activity activity = mActivityRule.getActivity();

        final List<List<Rect>> results = new ArrayList<>();
        final CountDownLatch doneAnimating = new CountDownLatch(1);

        final Consumer<List<Rect>> vtoListener = results::add;

        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            final ViewTreeObserver vto = v.getViewTreeObserver();
            vto.addOnSystemGestureExclusionRectsChangedListener(vtoListener);

            v.setSystemGestureExclusionRects(
                    Lists.newArrayList(new Rect(0, 0, 5, 5)));
            v.animate().x(30).setListener(new AnimationDoneListener(doneAnimating));
        });

        assertTrue("didn't finish animating", doneAnimating.await(3, SECONDS));

        // Sloppy size range since these rects are transformed and may straddle pixel boundaries
        List<Integer> sizeRange = Lists.newArrayList(5, 6);
        Rect prev = null;
        assertFalse("results empty", results.isEmpty());
        for (List<Rect> list : results) {
            assertEquals("one result rect", 1, list.size());
            final Rect first = list.get(0);
            if (prev != null) {
                assertTrue("left edge " + first.left + " >= previous " + prev.left,
                        first.left >= prev.left);
            }
            assertTrue("rect had expected width", sizeRange.contains(first.width()));
            assertTrue("rect had expected height", sizeRange.contains(first.height()));
            prev = first;
        }

        assertEquals("reached expected animated destination", prev.right, 35);

        // Make sure we don't get any more callbacks after removing the VTO listener.
        // Capture values on the UI thread to avoid races.
        final List<List<Rect>> oldResults = new ArrayList<>();
        final CountDownLatch secondDoneAnimating = new CountDownLatch(1);
        mActivityRule.runOnUiThread(() -> {
            oldResults.addAll(results);
            final View v = activity.findViewById(R.id.animating_view);
            final ViewTreeObserver vto = v.getViewTreeObserver();
            vto.removeOnSystemGestureExclusionRectsChangedListener(vtoListener);
            v.animate().x(0).setListener(new AnimationDoneListener(secondDoneAnimating));
        });

        assertTrue("didn't finish second animation", secondDoneAnimating.await(3, SECONDS));

        assertEquals("got unexpected exclusion rects", oldResults, results);
    }

    /**
     * Test that the system internals correctly handle cycling between exclusion rects present
     * and rects absent.
     */
    @Test
    public void removingRects() throws Throwable {
        final Activity activity = mActivityRule.getActivity();
        for (int i = 0; i < 3; i++) {
            final GestureExclusionLatcher[] setter = new GestureExclusionLatcher[1];
            mActivityRule.runOnUiThread(() -> {
                final View v = activity.findViewById(R.id.animating_view);
                setter[0] = GestureExclusionLatcher.watching(v.getViewTreeObserver());
                v.setSystemGestureExclusionRects(Lists.newArrayList(new Rect(0, 0, 5, 5)));
            });
            assertTrue("set rects timeout", setter[0].await(3, SECONDS));

            final GestureExclusionLatcher[] unsetter = new GestureExclusionLatcher[1];
            mActivityRule.runOnUiThread(() -> {
                final View v = activity.findViewById(R.id.animating_view);
                unsetter[0] = GestureExclusionLatcher.watching(v.getViewTreeObserver());
                v.setSystemGestureExclusionRects(Collections.emptyList());
            });
            assertTrue("unset rects timeout", unsetter[0].await(3, SECONDS));
        }
    }

    /**
     * Test round-trip of setting the window's exclusion rects
     */
    @Test
    public void rootExclusionRects() throws Throwable {
        final Activity activity = mActivityRule.getActivity();
        final GestureExclusionLatcher[] setter = new GestureExclusionLatcher[1];
        mActivityRule.runOnUiThread(() -> {
            final Window w = activity.getWindow();

            final List<Rect> initialRects = w.getSystemGestureExclusionRects();
            assertTrue("initial rects empty", initialRects.isEmpty());

            setter[0] = GestureExclusionLatcher.watching(w.getDecorView().getViewTreeObserver());
            w.setSystemGestureExclusionRects(Lists.newArrayList(new Rect(0, 0, 5, 5)));
            assertEquals("returned rects as expected",
                    Lists.newArrayList(new Rect(0, 0, 5, 5)),
                    w.getSystemGestureExclusionRects());
        });
        assertTrue("set rects timeout", setter[0].await(3, SECONDS));
    }

    @Test
    public void ignoreHiddenViewRects() throws Throwable {
        final Activity activity = mActivityRule.getActivity();
        final View contentView = activity.findViewById(R.id.abslistview_root);
        final List<Rect> dummyLocalExclusionRects = Lists.newArrayList(new Rect(0, 0, 5, 5));
        final List<Rect> dummyWindowExclusionRects = new ArrayList<>();

        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            int[] point = new int[2];
            v.getLocationInWindow(point);
            for (Rect r : dummyLocalExclusionRects) {
                Rect offsetR = new Rect(r);
                offsetR.offsetTo(point[0], point[1]);
                dummyWindowExclusionRects.add(offsetR);
            }
        });

        // Set an exclusion rect on the animating view, ensure it's reported
        final GestureExclusionLatcher[] setLatch = new GestureExclusionLatcher[1];
        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            setLatch[0] = GestureExclusionLatcher.watching(v.getViewTreeObserver());
            v.setSystemGestureExclusionRects(dummyLocalExclusionRects);
        });
        assertTrue("set rects timeout", setLatch[0].await(3, SECONDS));
        assertEquals("returned rects as expected", dummyWindowExclusionRects,
                setLatch[0].getLastReportedRects());

        // Hide the content view, ensure that the reported rects are null for the child view
        final GestureExclusionLatcher[] updateHideLatch = new GestureExclusionLatcher[1];
        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            updateHideLatch[0] = GestureExclusionLatcher.watching(v.getViewTreeObserver());
            contentView.setVisibility(View.INVISIBLE);
        });
        assertTrue("set rects timeout", updateHideLatch[0].await(3, SECONDS));
        assertEquals("returned rects as expected", Collections.EMPTY_LIST,
                updateHideLatch[0].getLastReportedRects());

        // Show the content view again, ensure that the reported rects are valid for the child view
        final GestureExclusionLatcher[] updateShowLatch = new GestureExclusionLatcher[1];
        mActivityRule.runOnUiThread(() -> {
            final View v = activity.findViewById(R.id.animating_view);
            updateShowLatch[0] = GestureExclusionLatcher.watching(v.getViewTreeObserver());
            contentView.setVisibility(View.VISIBLE);
        });
        assertTrue("set rects timeout", updateShowLatch[0].await(3, SECONDS));
        assertEquals("returned rects as expected", dummyWindowExclusionRects,
                updateShowLatch[0].getLastReportedRects());
    }

    private static class GestureExclusionLatcher implements Consumer<List<Rect>> {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private final ViewTreeObserver mVto;
        private List<Rect> mLastReportedRects = Collections.EMPTY_LIST;

        public static GestureExclusionLatcher watching(ViewTreeObserver vto) {
            final GestureExclusionLatcher latcher = new GestureExclusionLatcher(vto);
            vto.addOnSystemGestureExclusionRectsChangedListener(latcher);
            return latcher;
        }

        private GestureExclusionLatcher(ViewTreeObserver vto) {
            mVto = vto;
        }

        public boolean await(long time, TimeUnit unit) throws InterruptedException {
            return mLatch.await(time, unit);
        }

        public List<Rect> getLastReportedRects() {
            return mLastReportedRects;
        }

        @Override
        public void accept(List<Rect> rects) {
            mLastReportedRects = rects;
            mLatch.countDown();
            mVto.removeOnSystemGestureExclusionRectsChangedListener(this);
        }
    }

    private static class AnimationDoneListener extends AnimatorListenerAdapter {
        private final CountDownLatch mLatch;

        AnimationDoneListener(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mLatch.countDown();
        }
    }
}
