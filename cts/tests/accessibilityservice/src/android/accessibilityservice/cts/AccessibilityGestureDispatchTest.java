/**
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts;

import static android.accessibility.cts.common.InstrumentedAccessibilityService.enableService;
import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.AsyncUtils.await;
import static android.accessibilityservice.cts.utils.AsyncUtils.awaitCancellation;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_CANCEL;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_DOWN;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_MOVE;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_POINTER_DOWN;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_POINTER_UP;
import static android.accessibilityservice.cts.utils.GestureUtils.IS_ACTION_UP;
import static android.accessibilityservice.cts.utils.GestureUtils.add;
import static android.accessibilityservice.cts.utils.GestureUtils.ceil;
import static android.accessibilityservice.cts.utils.GestureUtils.click;
import static android.accessibilityservice.cts.utils.GestureUtils.diff;
import static android.accessibilityservice.cts.utils.GestureUtils.dispatchGesture;
import static android.accessibilityservice.cts.utils.GestureUtils.isAtPoint;
import static android.accessibilityservice.cts.utils.GestureUtils.longClick;
import static android.accessibilityservice.cts.utils.GestureUtils.path;
import static android.accessibilityservice.cts.utils.GestureUtils.times;
import static android.view.KeyCharacterMap.VIRTUAL_KEYBOARD;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.any;
import static org.hamcrest.CoreMatchers.both;
import static org.hamcrest.CoreMatchers.everyItem;
import static org.hamcrest.CoreMatchers.hasItem;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.accessibilityservice.GestureDescription.StrokeDescription;
import android.accessibilityservice.cts.activities.AccessibilityTestActivity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Matrix;
import android.graphics.Path;
import android.graphics.PointF;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Verify that gestures dispatched from an accessibility service show up in the current UI
 */
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class AccessibilityGestureDispatchTest {
    private static final String TAG = AccessibilityGestureDispatchTest.class.getSimpleName();

    private static final int GESTURE_COMPLETION_TIMEOUT = 5000; // millis
    private static final int MOTION_EVENT_TIMEOUT = 1000; // millis

    private ActivityTestRule<GestureDispatchActivity> mActivityRule =
            new ActivityTestRule<>(GestureDispatchActivity.class, false, false);

    private InstrumentedAccessibilityServiceTestRule<StubGestureAccessibilityService> mServiceRule =
            new InstrumentedAccessibilityServiceTestRule<>(
                    StubGestureAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mServiceRule)
            .around(mDumpOnFailureRule);

    final List<MotionEvent> mMotionEvents = new ArrayList<>();
    StubGestureAccessibilityService mService;
    MyTouchListener mMyTouchListener = new MyTouchListener();
    TextView mFullScreenTextView;
    int[] mViewLocation = new int[2];  // The location of TextView on the screen.
    PointF mStartPoint = new PointF(); // The relative location from mViewLocation.
    boolean mGotUpEvent;
    // Without a touch screen, there's no point in testing this feature
    boolean mHasTouchScreen;
    boolean mHasMultiTouch;

    private GestureDispatchActivity mActivity;

    @Before
    public void setUp() throws Exception {
        Instrumentation instrumentation = getInstrumentation();
        PackageManager pm = instrumentation.getContext().getPackageManager();
        mHasTouchScreen = pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
                || pm.hasSystemFeature(PackageManager.FEATURE_FAKETOUCH);
        if (!mHasTouchScreen) {
            return;
        }

        mActivity = launchActivityAndWaitForItToBeOnscreen(instrumentation,
                instrumentation.getUiAutomation(), mActivityRule);

        mHasMultiTouch = pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH)
                || pm.hasSystemFeature(PackageManager.FEATURE_FAKETOUCH_MULTITOUCH_DISTINCT);

        mFullScreenTextView = mActivity.findViewById(R.id.full_screen_text_view);
        getInstrumentation().runOnMainSync(() -> {
            final int midX = mFullScreenTextView.getWidth() / 2;
            final int midY = mFullScreenTextView.getHeight() / 2;
            mFullScreenTextView.getLocationOnScreen(mViewLocation);
            mFullScreenTextView.setOnTouchListener(mMyTouchListener);
            mStartPoint.set(midX, midY);
        });

        mService = mServiceRule.enableService();

        mMotionEvents.clear();
        mGotUpEvent = false;
    }

    @Test
    public void testClickAt_producesDownThenUp() throws InterruptedException {
        if (!mHasTouchScreen) {
            return;
        }

        PointF clickPoint = new PointF(mStartPoint.x, mStartPoint.y);
        dispatch(clickWithinView(clickPoint), GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(any(MotionEvent.class), 2);

        assertEquals(2, mMotionEvents.size());
        MotionEvent clickDown = mMotionEvents.get(0);
        MotionEvent clickUp = mMotionEvents.get(1);
        assertThat(clickDown, both(IS_ACTION_DOWN).and(isAtPoint(clickPoint)));
        assertThat(clickUp, both(IS_ACTION_UP).and(isAtPoint(clickPoint)));

        // Verify other MotionEvent fields in this test to make sure they get initialized.
        assertEquals(0, clickDown.getActionIndex());
        assertEquals(VIRTUAL_KEYBOARD, clickDown.getDeviceId());
        assertEquals(0, clickDown.getEdgeFlags());
        assertEquals(1F, clickDown.getXPrecision(), 0F);
        assertEquals(1F, clickDown.getYPrecision(), 0F);
        assertEquals(1, clickDown.getPointerCount());
        assertEquals(1F, clickDown.getPressure(), 0F);

        // Verify timing matches click
        assertEquals(clickDown.getDownTime(), clickDown.getEventTime());
        assertEquals(clickDown.getDownTime(), clickUp.getDownTime());
        assertEquals(ViewConfiguration.getTapTimeout(),
                clickUp.getEventTime() - clickUp.getDownTime());
        assertTrue(clickDown.getEventTime() + ViewConfiguration.getLongPressTimeout()
                > clickUp.getEventTime());
    }

    @Test
    public void testLongClickAt_producesEventsWithLongClickTiming() throws InterruptedException {
        if (!mHasTouchScreen) {
            return;
        }

        PointF clickPoint = new PointF(mStartPoint.x, mStartPoint.y);
        dispatch(longClickWithinView(clickPoint),
                ViewConfiguration.getLongPressTimeout() + GESTURE_COMPLETION_TIMEOUT);

        waitForMotionEvents(any(MotionEvent.class), 2);
        MotionEvent clickDown = mMotionEvents.get(0);
        MotionEvent clickUp = mMotionEvents.get(1);
        assertThat(clickDown, both(IS_ACTION_DOWN).and(isAtPoint(clickPoint)));
        assertThat(clickUp, both(IS_ACTION_UP).and(isAtPoint(clickPoint)));

        assertTrue(clickDown.getEventTime() + ViewConfiguration.getLongPressTimeout()
                <= clickUp.getEventTime());
        assertEquals(clickDown.getDownTime(), clickUp.getDownTime());
    }

    @Test
    public void testSwipe_shouldContainPointsInALine() throws InterruptedException {
        if (!mHasTouchScreen) {
            return;
        }

        float pointTolerance = 1f;
        PointF startPoint = new PointF(mStartPoint.x, mStartPoint.y);
        PointF endPoint = new PointF(mStartPoint.x + 10, mStartPoint.y + 20);
        int gestureTime = 500;

        dispatch(swipeWithinView(startPoint, endPoint, gestureTime),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(IS_ACTION_UP, 1);

        int numEvents = mMotionEvents.size();

        MotionEvent downEvent = mMotionEvents.get(0);
        MotionEvent upEvent = mMotionEvents.get(numEvents - 1);
        assertThat(downEvent, both(IS_ACTION_DOWN).and(isAtPoint(startPoint,
                    pointTolerance)));
        assertThat(upEvent, both(IS_ACTION_UP).and(isAtPoint(endPoint, pointTolerance)));
        assertEquals(gestureTime, upEvent.getEventTime() - downEvent.getEventTime());

        long lastEventTime = downEvent.getEventTime();
        for (int i = 1; i < numEvents - 1; i++) {
            MotionEvent moveEvent = mMotionEvents.get(i);
            assertTrue(moveEvent.getEventTime() >= lastEventTime);
            float fractionOfSwipe =
                    ((float) (moveEvent.getEventTime() - downEvent.getEventTime())) / gestureTime;
            PointF intermediatePoint = add(startPoint,
                    times(fractionOfSwipe, diff(endPoint, startPoint)));
            assertThat(moveEvent, both(IS_ACTION_MOVE).and(
                isAtPoint(intermediatePoint, pointTolerance)));
            lastEventTime = moveEvent.getEventTime();
        }
    }

    public void dispatch(GestureDescription gesture, int timeoutMs) {
        await(dispatchGesture(mService, gesture), timeoutMs, MILLISECONDS);
    }

    @Test
    public void testSlowSwipe_shouldNotContainMovesForTinyMovement() throws InterruptedException {
        if (!mHasTouchScreen) {
            return;
        }

        PointF startPoint = new PointF(mStartPoint.x, mStartPoint.y);
        PointF intermediatePoint1 = new PointF(mStartPoint.x, mStartPoint.y + 1);
        PointF intermediatePoint2 = new PointF(mStartPoint.x + 1, mStartPoint.y + 1);
        PointF intermediatePoint3 = new PointF(mStartPoint.x + 1, mStartPoint.y + 2);
        PointF endPoint = new PointF(mStartPoint.x + 1, mStartPoint.y + 2);
        int gestureTime = 1000;

        dispatch(swipeWithinView(startPoint, endPoint, gestureTime),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(IS_ACTION_UP, 1);

        assertEquals(5, mMotionEvents.size());
        assertThat(mMotionEvents.get(0), both(IS_ACTION_DOWN).and(isAtPoint(startPoint)));
        assertThat(mMotionEvents.get(1), both(IS_ACTION_MOVE).and(isAtPoint(intermediatePoint1)));
        assertThat(mMotionEvents.get(2), both(IS_ACTION_MOVE).and(isAtPoint(intermediatePoint2)));
        assertThat(mMotionEvents.get(3), both(IS_ACTION_MOVE).and(isAtPoint(intermediatePoint3)));
        assertThat(mMotionEvents.get(4), both(IS_ACTION_UP).and(isAtPoint(endPoint)));
    }

    @Test
    public void testAngledPinch_looksReasonable() throws InterruptedException {
        if (!(mHasTouchScreen && mHasMultiTouch)) {
            return;
        }

        PointF centerPoint = new PointF(mStartPoint.x, mStartPoint.y);
        int startSpacing = 100;
        int endSpacing = 50;
        int gestureTime = 500;
        float pinchTolerance = 2.0f;

        dispatch(pinchWithinView(centerPoint, startSpacing, endSpacing, 45.0F, gestureTime),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(IS_ACTION_UP, 1);
        int numEvents = mMotionEvents.size();

        // First and last two events are the pointers going down and up
        assertThat(mMotionEvents.get(0), IS_ACTION_DOWN);
        assertThat(mMotionEvents.get(1), IS_ACTION_POINTER_DOWN);
        assertThat(mMotionEvents.get(numEvents - 2), IS_ACTION_POINTER_UP);
        assertThat(mMotionEvents.get(numEvents - 1), IS_ACTION_UP);
        // The rest of the events are all moves
        assertEquals(numEvents - 4, getEventsMatching(IS_ACTION_MOVE).size());

        // All but the first and last events have two pointers
        float lastSpacing = startSpacing;
        for (int i = 1; i < numEvents - 1; i++) {
            MotionEvent.PointerCoords coords0 = new MotionEvent.PointerCoords();
            MotionEvent.PointerCoords coords1 = new MotionEvent.PointerCoords();
            MotionEvent event = mMotionEvents.get(i);
            event.getPointerCoords(0, coords0);
            event.getPointerCoords(1, coords1);
            // Verify center point
            assertEquals((float) centerPoint.x, (coords0.x + coords1.x) / 2, pinchTolerance);
            assertEquals((float) centerPoint.y, (coords0.y + coords1.y) / 2, pinchTolerance);
            // Verify angle
            assertEquals(coords0.x - centerPoint.x, coords0.y - centerPoint.y,
                    pinchTolerance);
            assertEquals(coords1.x - centerPoint.x, coords1.y - centerPoint.y,
                    pinchTolerance);
            float spacing = distance(coords0, coords1);
            assertTrue(spacing <= lastSpacing + pinchTolerance);
            assertTrue(spacing >= endSpacing - pinchTolerance);
            lastSpacing = spacing;
        }
    }

    // This test assumes device's screen contains its center (W/2, H/2) with some surroundings
    // and should work for rectangular, round and round with chin screens.
    @Test
    public void testClickWhenMagnified_matchesActualTouch() throws InterruptedException {
        final float POINT_TOL = 2.0f;
        final float CLICK_OFFSET_X = 10;
        final float CLICK_OFFSET_Y = 20;
        final float MAGNIFICATION_FACTOR = 2;
        if (!mHasTouchScreen) {
            return;
        }

        int displayId = mActivity.getWindow().getDecorView().getDisplay().getDisplayId();
        if (displayId != Display.DEFAULT_DISPLAY) {
            Log.i(TAG, "Magnification is not supported on virtual displays.");
            return;
        }

        final WindowManager wm = (WindowManager) getInstrumentation().getContext().getSystemService(
                Context.WINDOW_SERVICE);
        final StubMagnificationAccessibilityService magnificationService =
                enableService(StubMagnificationAccessibilityService.class);
        final AccessibilityService.MagnificationController
                magnificationController = magnificationService.getMagnificationController();

        final PointF magRegionCenterPoint = new PointF();
        magnificationService.runOnServiceSync(() -> {
            magnificationController.reset(false);
            magRegionCenterPoint.set(magnificationController.getCenterX(),
                    magnificationController.getCenterY());
        });
        final PointF magRegionOffsetPoint
                = add(magRegionCenterPoint, CLICK_OFFSET_X, CLICK_OFFSET_Y);

        final PointF magRegionOffsetClickPoint = add(magRegionCenterPoint,
                CLICK_OFFSET_X * MAGNIFICATION_FACTOR, CLICK_OFFSET_Y * MAGNIFICATION_FACTOR);

        try {
            // Zoom in
            final AtomicBoolean setScale = new AtomicBoolean();
            magnificationService.runOnServiceSync(() -> {
                setScale.set(magnificationController.setScale(MAGNIFICATION_FACTOR, false));
            });
            assertTrue("Failed to set scale", setScale.get());

            // Click in the center of the magnification region
            dispatch(new GestureDescription.Builder()
                    .addStroke(click(magRegionCenterPoint))
                    .build(),
                    GESTURE_COMPLETION_TIMEOUT);

            // Click at a slightly offset point
            dispatch(new GestureDescription.Builder()
                    .addStroke(click(magRegionOffsetClickPoint))
                    .build(),
                    GESTURE_COMPLETION_TIMEOUT);
            waitForMotionEvents(any(MotionEvent.class), 4);
        } finally {
            // Reset magnification
            final AtomicBoolean result = new AtomicBoolean();
            magnificationService.runOnServiceSync(() ->
                    result.set(magnificationController.reset(false)));
            magnificationService.runOnServiceSync(() -> magnificationService.disableSelf());
            assertTrue("Failed to reset", result.get());
        }

        assertEquals(4, mMotionEvents.size());
        // Because the MotionEvents have been captures by the view, the coordinates will
        // be in the View's coordinate system.
        magRegionCenterPoint.offset(-mViewLocation[0], -mViewLocation[1]);
        magRegionOffsetPoint.offset(-mViewLocation[0], -mViewLocation[1]);

        // The first click should be at the magnification center, as that point is invariant
        // for zoom only
        assertThat(mMotionEvents.get(0),
                both(IS_ACTION_DOWN).and(isAtPoint(magRegionCenterPoint, POINT_TOL)));
        assertThat(mMotionEvents.get(1),
                both(IS_ACTION_UP).and(isAtPoint(magRegionCenterPoint, POINT_TOL)));

        // The second point should be at the offset point
        assertThat(mMotionEvents.get(2),
                both(IS_ACTION_DOWN).and(isAtPoint(magRegionOffsetPoint, POINT_TOL)));
        assertThat(mMotionEvents.get(3),
                both(IS_ACTION_UP).and(isAtPoint(magRegionOffsetPoint, POINT_TOL)));
    }

    @Test
    public void testContinuedGestures_motionEventsContinue() throws Exception {
        if (!mHasTouchScreen) {
            return;
        }

        PointF start = new PointF(mStartPoint.x, mStartPoint.y);
        PointF mid1 = new PointF(mStartPoint.x + 10, mStartPoint.y);
        PointF mid2 = new PointF(mStartPoint.x + 10, mStartPoint.y + 5);
        PointF end = new PointF(mStartPoint.x + 10, mStartPoint.y + 10);
        int gestureTime = 500;

        StrokeDescription s1 = new StrokeDescription(
                lineWithinView(start, mid1), 0, gestureTime, true);
        StrokeDescription s2 = s1.continueStroke(
                lineWithinView(mid1, mid2), 0, gestureTime, true);
        StrokeDescription s3 = s2.continueStroke(
                lineWithinView(mid2, end), 0, gestureTime, false);

        GestureDescription gesture1 = new GestureDescription.Builder().addStroke(s1).build();
        GestureDescription gesture2 = new GestureDescription.Builder().addStroke(s2).build();
        GestureDescription gesture3 = new GestureDescription.Builder().addStroke(s3).build();
        dispatch(gesture1, gestureTime + GESTURE_COMPLETION_TIMEOUT);
        dispatch(gesture2, gestureTime + GESTURE_COMPLETION_TIMEOUT);
        dispatch(gesture3, gestureTime + GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(IS_ACTION_UP, 1);

        assertThat(mMotionEvents.get(0), allOf(IS_ACTION_DOWN, isAtPoint(start)));
        assertThat(mMotionEvents.subList(1, mMotionEvents.size() - 1), everyItem(IS_ACTION_MOVE));
        assertThat(mMotionEvents, hasItem(isAtPoint(mid1)));
        assertThat(mMotionEvents, hasItem(isAtPoint(mid2)));
        assertThat(mMotionEvents.get(mMotionEvents.size() - 1),
                allOf(IS_ACTION_UP, isAtPoint(end)));
    }

    @Test
    public void testContinuedGesture_withLineDisconnect_isCancelled() throws Exception {
        if (!mHasTouchScreen) {
            return;
        }

        PointF startPoint = new PointF(mStartPoint.x, mStartPoint.y);
        PointF midPoint = new PointF(mStartPoint.x + 10, mStartPoint.y);
        PointF endPoint = new PointF(mStartPoint.x + 10, mStartPoint.y + 10);
        int gestureTime = 500;

        StrokeDescription stroke1 =
                new StrokeDescription(lineWithinView(startPoint, midPoint), 0, gestureTime, true);
        dispatch(new GestureDescription.Builder().addStroke(stroke1).build(),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);
        waitForMotionEvents(both(IS_ACTION_MOVE).and(isAtPoint(midPoint)), 1);

        StrokeDescription stroke2 =
                stroke1.continueStroke(lineWithinView(endPoint, midPoint), 0, gestureTime, false);
        mMotionEvents.clear();
        awaitCancellation(
                dispatchGesture(mService,
                        new GestureDescription.Builder().addStroke(stroke2).build()),
                gestureTime + GESTURE_COMPLETION_TIMEOUT, MILLISECONDS);

        waitForMotionEvents(IS_ACTION_CANCEL, 1);
        assertEquals(1, mMotionEvents.size());
    }

    @Test
    public void testContinuedGesture_nextGestureDoesntContinue_isCancelled() throws Exception {
        if (!mHasTouchScreen) {
            return;
        }

        PointF startPoint = new PointF(mStartPoint.x, mStartPoint.y);
        PointF midPoint = new PointF(mStartPoint.x + 10, mStartPoint.y);
        PointF endPoint = new PointF(mStartPoint.x + 10, mStartPoint.y + 10);
        int gestureTime = 500;

        StrokeDescription stroke1 =
                new StrokeDescription(lineWithinView(startPoint, midPoint), 0, gestureTime, true);
        dispatch(new GestureDescription.Builder().addStroke(stroke1).build(),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);

        StrokeDescription stroke2 =
                new StrokeDescription(lineWithinView(midPoint, endPoint), 0, gestureTime, false);
        dispatch(new GestureDescription.Builder().addStroke(stroke2).build(),
                gestureTime + GESTURE_COMPLETION_TIMEOUT);

        waitForMotionEvents(IS_ACTION_UP, 1);

        List<MotionEvent> cancelEvent = getEventsMatching(IS_ACTION_CANCEL);
        assertEquals(1, cancelEvent.size());
        // Confirm that a down follows the cancel
        assertThat(mMotionEvents.get(mMotionEvents.indexOf(cancelEvent.get(0)) + 1),
                both(IS_ACTION_DOWN).and(isAtPoint(midPoint)));
        // Confirm that the last point is an up
        assertThat(mMotionEvents.get(mMotionEvents.size() - 1),
                both(IS_ACTION_UP).and(isAtPoint(endPoint)));
    }

    @Test
    public void testContinuingGesture_withNothingToContinue_isCancelled() {
        if (!mHasTouchScreen) {
            return;
        }

        PointF startPoint = new PointF(mStartPoint.x, mStartPoint.y);
        PointF midPoint = new PointF(mStartPoint.x + 10, mStartPoint.y);
        PointF endPoint = new PointF(mStartPoint.x + 10, mStartPoint.y + 10);
        int gestureTime = 500;

        StrokeDescription stroke1 =
                new StrokeDescription(lineWithinView(startPoint, midPoint), 0, gestureTime, true);

        StrokeDescription stroke2 =
                stroke1.continueStroke(lineWithinView(midPoint, endPoint), 0, gestureTime, false);
        awaitCancellation(
                dispatchGesture(mService,
                        new GestureDescription.Builder().addStroke(stroke2).build()),
                gestureTime + GESTURE_COMPLETION_TIMEOUT, MILLISECONDS);
    }

    public static class GestureDispatchActivity extends AccessibilityTestActivity {
        public GestureDispatchActivity() {
            super();
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.full_screen_frame_layout);
        }
    }

    private void waitForMotionEvents(Matcher<MotionEvent> matcher, int numEventsExpected)
            throws InterruptedException {
        synchronized (mMotionEvents) {
            long endMillis = SystemClock.uptimeMillis() + MOTION_EVENT_TIMEOUT;
            boolean gotEvents = getEventsMatching(matcher).size() >= numEventsExpected;
            while (!gotEvents && (SystemClock.uptimeMillis() < endMillis)) {
                mMotionEvents.wait(endMillis - SystemClock.uptimeMillis());
                gotEvents = getEventsMatching(matcher).size() >= numEventsExpected;
            }
            assertTrue("Did not receive required events. Got:\n" + mMotionEvents + "\n filtered:\n"
                    + getEventsMatching(matcher), gotEvents);
        }
    }

    private List<MotionEvent> getEventsMatching(Matcher<MotionEvent> matcher) {
        List<MotionEvent> events = new ArrayList<>();
        synchronized (mMotionEvents) {
            for (MotionEvent event : mMotionEvents) {
                if (matcher.matches(event)) {
                    events.add(event);
                }
            }
        }
        return events;
    }

    private float distance(MotionEvent.PointerCoords point1, MotionEvent.PointerCoords point2) {
        return (float) Math.hypot((double) (point1.x - point2.x), (double) (point1.y - point2.y));
    }

    private class MyTouchListener implements View.OnTouchListener {
        @Override
        public boolean onTouch(View view, MotionEvent motionEvent) {
            synchronized (mMotionEvents) {
                if (motionEvent.getActionMasked() == MotionEvent.ACTION_UP) {
                    mGotUpEvent = true;
                }
                mMotionEvents.add(MotionEvent.obtain(motionEvent));
                mMotionEvents.notifyAll();
                return true;
            }
        }
    }

    private GestureDescription clickWithinView(PointF clickPoint) {
        return new GestureDescription.Builder()
                .addStroke(click(withinView(clickPoint)))
                .build();
    }

    private GestureDescription longClickWithinView(PointF clickPoint) {
        return new GestureDescription.Builder()
                .addStroke(longClick(withinView(clickPoint)))
                .build();
    }

    private PointF withinView(PointF clickPoint) {
        return add(clickPoint, mViewLocation[0], mViewLocation[1]);
    }

    private GestureDescription swipeWithinView(PointF start, PointF end, long duration) {
        return new GestureDescription.Builder()
                .addStroke(new StrokeDescription(lineWithinView(start, end), 0, duration))
                .build();
    }

    private Path lineWithinView(PointF startPoint, PointF endPoint) {
        return path(withinView(startPoint), withinView(endPoint));
    }

    private GestureDescription pinchWithinView(PointF centerPoint, int startSpacing,
            int endSpacing, float orientation, long duration) {
        if ((startSpacing < 0) || (endSpacing < 0)) {
            throw new IllegalArgumentException("Pinch spacing cannot be negative");
        }
        PointF offsetCenter = withinView(centerPoint);
        float[] startPoint1 = new float[2];
        float[] endPoint1 = new float[2];
        float[] startPoint2 = new float[2];
        float[] endPoint2 = new float[2];

        /* Build points for a horizontal gesture centered at the origin */
        startPoint1[0] = startSpacing / 2;
        startPoint1[1] = 0;
        endPoint1[0] = endSpacing / 2;
        endPoint1[1] = 0;
        startPoint2[0] = -startSpacing / 2;
        startPoint2[1] = 0;
        endPoint2[0] = -endSpacing / 2;
        endPoint2[1] = 0;

        /* Rotate and translate the points */
        Matrix matrix = new Matrix();
        matrix.setRotate(orientation);
        matrix.postTranslate(offsetCenter.x, offsetCenter.y);
        matrix.mapPoints(startPoint1);
        matrix.mapPoints(endPoint1);
        matrix.mapPoints(startPoint2);
        matrix.mapPoints(endPoint2);

        Path path1 = new Path();
        path1.moveTo(startPoint1[0], startPoint1[1]);
        path1.lineTo(endPoint1[0], endPoint1[1]);
        Path path2 = new Path();
        path2.moveTo(startPoint2[0], startPoint2[1]);
        path2.lineTo(endPoint2[0], endPoint2[1]);

        return new GestureDescription.Builder()
                .addStroke(new StrokeDescription(path1, 0, duration))
                .addStroke(new StrokeDescription(path2, 0, duration))
                .build();
    }
}
