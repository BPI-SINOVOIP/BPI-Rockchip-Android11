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

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.AsyncUtils.await;
import static android.accessibilityservice.cts.utils.GestureUtils.add;
import static android.accessibilityservice.cts.utils.GestureUtils.click;
import static android.accessibilityservice.cts.utils.GestureUtils.dispatchGesture;
import static android.accessibilityservice.cts.utils.GestureUtils.doubleTap;
import static android.accessibilityservice.cts.utils.GestureUtils.doubleTapAndHold;
import static android.accessibilityservice.cts.utils.GestureUtils.multiTap;
import static android.accessibilityservice.cts.utils.GestureUtils.secondFingerMultiTap;
import static android.accessibilityservice.cts.utils.GestureUtils.swipe;
import static android.view.MotionEvent.ACTION_DOWN;
import static android.view.MotionEvent.ACTION_HOVER_ENTER;
import static android.view.MotionEvent.ACTION_HOVER_EXIT;
import static android.view.MotionEvent.ACTION_HOVER_MOVE;
import static android.view.MotionEvent.ACTION_MOVE;
import static android.view.MotionEvent.ACTION_POINTER_DOWN;
import static android.view.MotionEvent.ACTION_POINTER_UP;
import static android.view.MotionEvent.ACTION_UP;
import static android.view.accessibility.AccessibilityEvent.TYPE_GESTURE_DETECTION_END;
import static android.view.accessibility.AccessibilityEvent.TYPE_GESTURE_DETECTION_START;
import static android.view.accessibility.AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END;
import static android.view.accessibility.AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START;
import static android.view.accessibility.AccessibilityEvent.TYPE_TOUCH_INTERACTION_END;
import static android.view.accessibility.AccessibilityEvent.TYPE_TOUCH_INTERACTION_START;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_CLICKED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_LONG_CLICKED;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.GestureDescription;
import android.accessibilityservice.GestureDescription.StrokeDescription;
import android.accessibilityservice.cts.AccessibilityGestureDispatchTest.GestureDispatchActivity;
import android.accessibilityservice.cts.utils.EventCapturingClickListener;
import android.accessibilityservice.cts.utils.EventCapturingHoverListener;
import android.accessibilityservice.cts.utils.EventCapturingLongClickListener;
import android.accessibilityservice.cts.utils.EventCapturingTouchListener;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Region;
import android.platform.test.annotations.AppModeFull;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Display;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * A set of tests for testing touch exploration. Each test dispatches a gesture and checks for the
 * appropriate hover and/or touch events followed by the appropriate accessibility events. Some
 * tests will then check for events from the view.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class TouchExplorerTest {
    // Constants
    private static final float GESTURE_LENGTH_MMS = 10.0f;
    private TouchExplorationStubAccessibilityService mService;
    private Instrumentation mInstrumentation;
    private UiAutomation mUiAutomation;
    private boolean mHasTouchscreen;
    private boolean mScreenBigEnough;
    private long mSwipeTimeMillis;
    private EventCapturingHoverListener mHoverListener = new EventCapturingHoverListener(false);
    private EventCapturingTouchListener mTouchListener = new EventCapturingTouchListener(false);
    private EventCapturingClickListener mClickListener = new EventCapturingClickListener();
    private EventCapturingLongClickListener mLongClickListener =
            new EventCapturingLongClickListener();

    private ActivityTestRule<GestureDispatchActivity> mActivityRule =
            new ActivityTestRule<>(GestureDispatchActivity.class, false);

    private InstrumentedAccessibilityServiceTestRule<TouchExplorationStubAccessibilityService>
            mServiceRule =
                    new InstrumentedAccessibilityServiceTestRule<>(
                            TouchExplorationStubAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityRule).around(mServiceRule).around(mDumpOnFailureRule);

    PointF mTapLocation; // Center of activity. Gestures all start from around this point.
    float mSwipeDistance;
    View mView;

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mUiAutomation =
                mInstrumentation.getUiAutomation(
                        UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        PackageManager pm = mInstrumentation.getContext().getPackageManager();
        mHasTouchscreen =
                pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
                        || pm.hasSystemFeature(PackageManager.FEATURE_FAKETOUCH);
        // Find window size, check that it is big enough for gestures.
        // Gestures will start in the center of the window, so we need enough horiz/vert space.
        mService = mServiceRule.enableService();
        mView = mActivityRule.getActivity().findViewById(R.id.full_screen_text_view);
        WindowManager windowManager =
                (WindowManager)
                        mInstrumentation.getContext().getSystemService(Context.WINDOW_SERVICE);
        final DisplayMetrics metrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getRealMetrics(metrics);
        mScreenBigEnough = mView.getWidth() / 2 >  TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_MM, GESTURE_LENGTH_MMS, metrics);
        if (!mHasTouchscreen || !mScreenBigEnough) return;

        mView.setOnHoverListener(mHoverListener);
        mView.setOnTouchListener(mTouchListener);
        mInstrumentation.runOnMainSync(
                () -> {
                    int[] viewLocation = new int[2];
                    mView = mActivityRule.getActivity().findViewById(R.id.full_screen_text_view);
                    final int midX = mView.getWidth() / 2;
                    final int midY = mView.getHeight() / 2;
                    mView.getLocationOnScreen(viewLocation);
                    mTapLocation = new PointF(viewLocation[0] + midX, viewLocation[1] + midY);
                    mSwipeDistance = mView.getWidth() / 4;
                    mSwipeTimeMillis = (long) mSwipeDistance * 4;
                    mView.setOnClickListener(mClickListener);
                    mView.setOnLongClickListener(mLongClickListener);
                });
    }

    /** Test a slow swipe which should initiate touch exploration. */
    @Test
    @AppModeFull
    public void testSlowSwipe_initiatesTouchExploration() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        dispatch(swipe(mTapLocation, add(mTapLocation, mSwipeDistance, 0), mSwipeTimeMillis));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_MOVE, ACTION_HOVER_EXIT);
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
    }

    /** Test a fast swipe which should not initiate touch exploration. */
    @Test
    @AppModeFull
    public void testFastSwipe_doesNotInitiateTouchExploration() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        dispatch(swipe(mTapLocation, add(mTapLocation, mSwipeDistance, 0)));
        mHoverListener.assertNonePropagated();
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_GESTURE_DETECTION_START,
                TYPE_GESTURE_DETECTION_END,
                TYPE_TOUCH_INTERACTION_END);
    }

    /**
     * Test a two finger drag. TouchExplorer would perform a drag gesture when two fingers moving in
     * the same direction.
     */
    @Test
    @AppModeFull
    public void testTwoFingerDrag_dispatchesEventsBetweenFingers() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        // A two point moving that are in the same direction can perform a drag gesture by
        // TouchExplorer while one point moving can not perform a drag gesture. We use two swipes
        // to emulate a two finger drag gesture.
        final int twoFingerOffset = (int) mSwipeDistance;
        final PointF dragStart = mTapLocation;
        final PointF dragEnd = add(dragStart, 0, mSwipeDistance);
        final PointF finger1Start = add(dragStart, twoFingerOffset, 0);
        final PointF finger1End = add(finger1Start, 0, mSwipeDistance);
        final PointF finger2Start = add(dragStart, -twoFingerOffset, 0);
        final PointF finger2End = add(finger2Start, 0, mSwipeDistance);
        dispatch(
                swipe(finger1Start, finger1End, mSwipeTimeMillis),
                swipe(finger2Start, finger2End, mSwipeTimeMillis));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_MOVE, ACTION_UP);
    }

    /** Test a basic single tap which should initiate touch exploration. */
    @Test
    @AppModeFull
    public void testSingleTap_initiatesTouchExploration() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        dispatch(click(mTapLocation));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_EXIT);
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
    }

    /**
     * Test the case where we execute a "sloppy" double tap, meaning that the second tap isn't
     * exactly in the same location as the first but still within tolerances. It should behave the
     * same as a standard double tap. Note that this test does not request that double tap be
     * dispatched to the accessibility service, meaning that it will be handled by the framework and
     * the view will be clicked.
     */
    @Test
    @AppModeFull
    public void testSloppyDoubleTapAccessibilityFocus_performsClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        syncAccessibilityFocusToInputFocus();
        int slop = ViewConfiguration.get(mInstrumentation.getContext()).getScaledDoubleTapSlop();
        dispatch(multiTap(mTapLocation, 2, slop));
        mHoverListener.assertNonePropagated();
        // The click should not be delivered via touch events in this case.
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_INTERACTION_END,
                TYPE_VIEW_CLICKED);
        mClickListener.assertClicked(mView);
    }

    /**
     * Test the case where we want to click on the item that has accessibility focus by using
     * AccessibilityNodeInfo.performAction. Note that this test does not request that double tap be
     * dispatched to the accessibility service, meaning that it will be handled by the framework and
     * the view will be clicked.
     */
    @Test
    @AppModeFull
    public void testDoubleTapAccessibilityFocus_performsClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        syncAccessibilityFocusToInputFocus();
        dispatch(doubleTap(mTapLocation));
        mHoverListener.assertNonePropagated();
        // The click should not be delivered via touch events in this case.
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_INTERACTION_END,
                TYPE_VIEW_CLICKED);
        mClickListener.assertClicked(mView);
    }

    /**
     * Test the case where we double tap but there is no accessibility focus. Nothing should happen.
     */
    @Test
    @AppModeFull
    public void testDoubleTapNoFocus_doesNotPerformClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        dispatch(doubleTap(mTapLocation));
        mHoverListener.assertNonePropagated();
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(TYPE_TOUCH_INTERACTION_START, TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        mClickListener.assertNoneClicked();
    }

    /**
     * Test the case where we double tap and hold but there is no accessibility focus. Nothing
     * should happen.
     */
    @Test
    @AppModeFull
    public void testDoubleTapAndHoldNoFocus_doesNotPerformLongClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        dispatch(doubleTap(mTapLocation));
        mHoverListener.assertNonePropagated();
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(TYPE_TOUCH_INTERACTION_START, TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        mLongClickListener.assertNoneLongClicked();
    }

    /**
     * Test the case where we want to double tap using a second finger while the first finger is
     * touch exploring.
     */
    @Test
    @AppModeFull
    public void testSecondFingerDoubleTapTouchExploring_performsClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        syncAccessibilityFocusToInputFocus();
        // hold the first finger for long enough to trigger touch exploration before double-tapping.
        // Touch exploration is triggered after the double tap timeout.
        dispatch(
                secondFingerMultiTap(
                        mTapLocation,
                        add(mTapLocation, mSwipeDistance, 0),
                        2,
                        ViewConfiguration.getDoubleTapTimeout() + 50));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_EXIT);
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END,
                TYPE_VIEW_CLICKED);
        mClickListener.assertClicked(mView);
    }

    /**
     * Test the case where we double tap and no item has accessibility focus, so TouchExplorer sends
     * touch events to the last touch-explored coordinates to simulate a click.
     */
    @Test
    @AppModeFull
    public void testDoubleTapNoAccessibilityFocus_sendsTouchEvents() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        // Do a single tap so there is a valid last touch-explored location.
        dispatch(click(mTapLocation));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_EXIT);
        // We don't really care about these events but we need to make sure all the events we want
        // to clear have arrived before we clear them.
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        dispatch(doubleTap(mTapLocation));
        mHoverListener.assertNonePropagated();
        // The click gets delivered as a series of touch events.
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_UP);
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START, TYPE_TOUCH_INTERACTION_END, TYPE_VIEW_CLICKED);
        mClickListener.assertClicked(mView);
    }

    /**
     * Test the case where we double tap and hold and no item has accessibility focus, so
     * TouchExplorer sends touch events to the last touch-explored coordinates to simulate a long
     * click.
     */
    @Test
    @AppModeFull
    public void testDoubleTapAndHoldNoAccessibilityFocus_sendsTouchEvents() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        // Do a single tap so there is a valid last touch-explored location.
        dispatch(click(mTapLocation));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_EXIT);
        // We don't really care about these events but we need to make sure all the events we want
        // to clear have arrived before we clear them.
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        dispatch(doubleTapAndHold(mTapLocation));
        mHoverListener.assertNonePropagated();
        // The click gets delivered as a series of touch events.
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_UP);
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START, TYPE_VIEW_LONG_CLICKED, TYPE_TOUCH_INTERACTION_END);
        mLongClickListener.assertLongClicked(mView);
    }

    /**
     * Test the case where we want to double tap using a second finger without triggering touch
     * exploration.
     */
    @Test
    @AppModeFull
    public void testSecondFingerDoubleTapNotTouchExploring_performsClick() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        syncAccessibilityFocusToInputFocus();
        // Hold the first finger for less than the double tap timeout which will not trigger touch
        // exploration.
        // Touch exploration is triggered after the double tap timeout.
        dispatch(
                secondFingerMultiTap(
                        mTapLocation,
                        add(mTapLocation, mSwipeDistance, 0),
                        2,
                        ViewConfiguration.getDoubleTapTimeout() / 3));
        mHoverListener.assertNonePropagated();
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_INTERACTION_END,
                TYPE_VIEW_CLICKED);
        mClickListener.assertClicked(mView);
    }

    /**
     * This method tests a three-finger swipe. The gesture will be delegated to the view as-is. This
     * is distinct from dragging, where two fingers are delegated to the view as one finger. Note
     * that because multi-finger gestures are disabled this gesture will not be handled by the
     * gesture detector.
     */
    @Test
    @AppModeFull
    public void testThreeFingerMovement_shouldDelegate() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        // Move three fingers down the screen slowly.
        PointF finger1Start = add(mTapLocation, -mSwipeDistance, 0);
        PointF finger1End = add(mTapLocation, -mSwipeDistance, mSwipeDistance);
        PointF finger2Start = mTapLocation;
        PointF finger2End = add(mTapLocation, 0, mSwipeDistance);
        PointF finger3Start = add(mTapLocation, mSwipeDistance, 0);
        PointF finger3End = add(mTapLocation, mSwipeDistance, mSwipeDistance);
        StrokeDescription swipe1 = swipe(finger1Start, finger1End, mSwipeTimeMillis);
        StrokeDescription swipe2 = swipe(finger2Start, finger2End, mSwipeTimeMillis);
        StrokeDescription swipe3 = swipe(finger3Start, finger3End, mSwipeTimeMillis);
        dispatch(swipe1, swipe2, swipe3);
        mHoverListener.assertNonePropagated();
        mTouchListener.assertPropagated(
                ACTION_DOWN,
                ACTION_POINTER_DOWN,
                ACTION_POINTER_DOWN,
                ACTION_MOVE,
                ACTION_POINTER_UP,
                ACTION_POINTER_UP,
                ACTION_UP);
    }

    /**
     * This method tests the case where two fingers are moving independently. The gesture will be
     * delegated to the view as-is. This is distinct from dragging, where two fingers are delegated
     * to the view as one finger.
     */
    @Test
    @AppModeFull
    public void testTwoFingersMovingIndependently_shouldDelegate() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        // Move two fingers towards eacher slowly.
        PointF finger1Start = add(mTapLocation, -mSwipeDistance, 0);
        PointF finger1End = add(mTapLocation, -10, 0);
        StrokeDescription swipe1 = swipe(finger1Start, finger1End, mSwipeTimeMillis);
        PointF finger2Start = add(mTapLocation, mSwipeDistance, 0);
        PointF finger2End = add(mTapLocation, 10, 0);
        StrokeDescription swipe2 = swipe(finger2Start, finger2End, mSwipeTimeMillis);
        dispatch(swipe1, swipe2);
        mHoverListener.assertNonePropagated();
        mTouchListener.assertPropagated(
                ACTION_DOWN, ACTION_POINTER_DOWN, ACTION_MOVE, ACTION_POINTER_UP, ACTION_UP);
    }

    /**
     * Test the gesture detection passthrough by performing a fast swipe in the passthrough region.
     * It should bypass the gesture detector entirely.
     */
    @Test
    @AppModeFull
    public void testGestureDetectionPassthrough_initiatesTouchExploration() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        setRightSideOfActivityWindowGestureDetectionPassthrough();
        // Swipe in the passthrough region. This should generate hover events.
        dispatch(swipe(mTapLocation, add(mTapLocation, mSwipeDistance, 0)));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_MOVE, ACTION_HOVER_EXIT);
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        // Swipe starting inside the passthrough region but ending outside of it. This should still
        // behave as a passthrough interaction.
        dispatch(swipe(mTapLocation, add(mTapLocation, -mSwipeDistance, 0)));
        mHoverListener.assertPropagated(ACTION_HOVER_ENTER, ACTION_HOVER_MOVE, ACTION_HOVER_EXIT);
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_START,
                TYPE_TOUCH_EXPLORATION_GESTURE_END,
                TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        // Swipe outside the passthrough region. This should not generate hover events.
        dispatch(swipe(add(mTapLocation, -1, 0), add(mTapLocation, -mSwipeDistance, 0)));
        mHoverListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_GESTURE_DETECTION_START,
                TYPE_GESTURE_DETECTION_END,
                TYPE_TOUCH_INTERACTION_END);
        mService.clearEvents();
        // There should be no touch events in this test.
        mTouchListener.assertNonePropagated();
        clearPassthroughRegions();
    }

    /**
     * Test the touch exploration passthrough by performing a fast swipe in the passthrough region.
     * It should generate touch events.
     */
    @Test
    @AppModeFull
    public void testTouchExplorationPassthrough_sendsTouchEvents() {
        if (!mHasTouchscreen || !mScreenBigEnough) return;
        setRightSideOfActivityWindowTouchExplorationPassthrough();
        // Swipe in the passthrough region. This should generate  touch events.
        dispatch(swipe(mTapLocation, add(mTapLocation, mSwipeDistance, 0)));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_MOVE, ACTION_UP);
        // We still want accessibility events to tell us when the gesture starts and ends.
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START, TYPE_TOUCH_INTERACTION_END, TYPE_VIEW_CLICKED);
        mService.clearEvents();
        // Swipe starting inside the passthrough region but ending outside of it. This should still
        // behave as a passthrough interaction.
        dispatch(swipe(mTapLocation, add(mTapLocation, -mSwipeDistance, 0)));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_MOVE, ACTION_UP);
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START, TYPE_TOUCH_INTERACTION_END, TYPE_VIEW_CLICKED);
        mService.clearEvents();
        // Swipe outside the passthrough region. This should not generate touch events.
        dispatch(swipe(add(mTapLocation, -1, 0), add(mTapLocation, -mSwipeDistance, 0)));
        mTouchListener.assertNonePropagated();
        mService.assertPropagated(
                TYPE_TOUCH_INTERACTION_START,
                TYPE_GESTURE_DETECTION_START,
                TYPE_GESTURE_DETECTION_END,
                TYPE_TOUCH_INTERACTION_END);
        // There should be no hover events in this test.
        mHoverListener.assertNonePropagated();
        clearPassthroughRegions();
    }

    public void dispatch(StrokeDescription firstStroke, StrokeDescription... rest) {
        GestureDescription.Builder builder =
                new GestureDescription.Builder().addStroke(firstStroke);
        for (StrokeDescription stroke : rest) {
            builder.addStroke(stroke);
        }
        dispatch(builder.build());
    }

    public void dispatch(GestureDescription gesture) {
        await(dispatchGesture(mService, gesture));
    }

    /** Set the accessibility focus to the element that has input focus. */
    private void syncAccessibilityFocusToInputFocus() {
        mService.runOnServiceSync(
                () -> {
                    mUiAutomation
                            .getRootInActiveWindow()
                            .findFocus(AccessibilityNodeInfo.FOCUS_INPUT)
                            .performAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
                });
    }

    private void setRightSideOfActivityWindowGestureDetectionPassthrough() {
        Region region = getRightSideOfActivityWindowRegion();
        mService.runOnServiceSync(
                () -> {
                    mService.setGestureDetectionPassthroughRegion(Display.DEFAULT_DISPLAY, region);
                });
    }

    private void setRightSideOfActivityWindowTouchExplorationPassthrough() {
        Region region = getRightSideOfActivityWindowRegion();
        mService.runOnServiceSync(
                () -> {
                    mService.setTouchExplorationPassthroughRegion(Display.DEFAULT_DISPLAY, region);
                });
    }

    private void clearPassthroughRegions() {
        mService.runOnServiceSync(
                () -> {
                    mService.setGestureDetectionPassthroughRegion(
                            Display.DEFAULT_DISPLAY, new Region());
                    mService.setTouchExplorationPassthroughRegion(
                            Display.DEFAULT_DISPLAY, new Region());
                });
    }

    private Region getRightSideOfActivityWindowRegion() {
        int[] viewLocation = new int[2];
        mView.getLocationOnScreen(viewLocation);

        int top = viewLocation[1];
        int left = viewLocation[0]  + mView.getWidth() / 2;
        int right = viewLocation[0]  + mView.getWidth();
        int bottom = viewLocation[1] + mView.getHeight();
        Region region = new Region(left, top, right, bottom);
        return region;
    }
}
