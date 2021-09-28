/**
 * Copyright (C) 2017 The Android Open Source Project
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

import static android.accessibilityservice.cts.utils.ActivityLaunchUtils.launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen;
import static android.accessibilityservice.cts.utils.DisplayUtils.VirtualDisplaySession;
import static android.accessibilityservice.cts.utils.GestureUtils.add;
import static android.accessibilityservice.cts.utils.GestureUtils.click;
import static android.accessibilityservice.cts.utils.GestureUtils.diff;
import static android.accessibilityservice.cts.utils.GestureUtils.endTimeOf;
import static android.accessibilityservice.cts.utils.GestureUtils.getGestureBuilder;
import static android.accessibilityservice.cts.utils.GestureUtils.longClick;
import static android.accessibilityservice.cts.utils.GestureUtils.startingAt;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.accessibilityservice.GestureDescription.StrokeDescription;
import android.accessibilityservice.cts.activities.AccessibilityWindowQueryActivity;
import android.accessibilityservice.cts.utils.GestureUtils;
import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Path;
import android.graphics.Point;
import android.graphics.PointF;
import android.platform.test.annotations.AppModeFull;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Verify that motion events are recognized as accessibility gestures. */
@RunWith(AndroidJUnit4.class)
public class AccessibilityGestureDetectorTest {

    // Constants
    private static final float GESTURE_LENGTH_INCHES = 1.0f;
    // The movement should exceed the threshold 1 cm in 150 ms defined in Swipe.java. It means the
    // swipe velocity in testing should be greater than 2.54 cm / 381 ms. Therefore the
    // duration should be smaller than 381.
    private static final long STROKE_MS = 380;
    private static final long GESTURE_DISPATCH_TIMEOUT_MS = 3000;
    private static final long EVENT_DISPATCH_TIMEOUT_MS = 3000;
    private static final PointF FINGER_OFFSET_PX = new PointF(100f, -50f);

    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private InstrumentedAccessibilityServiceTestRule<GestureDetectionStubAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    GestureDetectionStubAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    // Test AccessibilityService that collects gestures.
    GestureDetectionStubAccessibilityService mService; 
    boolean mHasTouchScreen;
    boolean mScreenBigEnough;
    int mStrokeLenPxX; // Gesture stroke size, in pixels
    int mStrokeLenPxY;
    Point mCenter; // Center of screen. Gestures all start from this point.
    PointF mTapLocation;
    @Mock AccessibilityService.GestureResultCallback mGestureDispatchCallback;

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation(FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
    }

    @AfterClass
    public static void finalTearDown() {
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        // Check that device has a touch screen.
        PackageManager pm = sInstrumentation.getContext().getPackageManager();
        mHasTouchScreen = pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
                || pm.hasSystemFeature(PackageManager.FEATURE_FAKETOUCH);
        if (!mHasTouchScreen) {
            return;
        }

        // Find screen size, check that it is big enough for gestures.
        // Gestures will start in the center of the screen, so we need enough horiz/vert space.
        WindowManager windowManager = (WindowManager) sInstrumentation.getContext()
                .getSystemService(Context.WINDOW_SERVICE);
        final DisplayMetrics metrics = new DisplayMetrics();
        windowManager.getDefaultDisplay().getRealMetrics(metrics);
        mCenter = new Point((int) metrics.widthPixels / 2, (int) metrics.heightPixels / 2);
        mTapLocation = new PointF(mCenter);
        mStrokeLenPxX = (int) (GESTURE_LENGTH_INCHES * metrics.xdpi);
        // The threshold is determined by xdpi.
        mStrokeLenPxY = mStrokeLenPxX;
        final boolean screenWideEnough = metrics.widthPixels / 2 > mStrokeLenPxX;
        final boolean screenHighEnough =  metrics.heightPixels / 2 > mStrokeLenPxY;
        mScreenBigEnough = screenWideEnough && screenHighEnough;
        if (!mScreenBigEnough) {
            return;
        }
        // Start stub accessibility service.
        mService = mServiceRule.enableService();
    }

    @Test
    @AppModeFull
    public void testRecognizeGesturePath() {
        if (!mHasTouchScreen || !mScreenBigEnough) {
            return;
        }

        runGestureDetectionTestOnDisplay(Display.DEFAULT_DISPLAY);
        runMultiFingerGestureDetectionTestOnDisplay(Display.DEFAULT_DISPLAY);
    }

    @Test
    @AppModeFull
    public void testRecognizeGesturePathOnVirtualDisplay() throws Exception {
        if (!mHasTouchScreen || !mScreenBigEnough) {
            return;
        }

        try (final VirtualDisplaySession displaySession = new VirtualDisplaySession()) {
            final int displayId = displaySession.createDisplayWithDefaultDisplayMetricsAndWait(
                    sInstrumentation.getTargetContext(), false).getDisplayId();
            // Launches an activity on virtual display to meet a real situation.
            final Activity activity = launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
                    sInstrumentation, sUiAutomation, AccessibilityWindowQueryActivity.class,
                    displayId);

            try {
                runGestureDetectionTestOnDisplay(displayId);
                runMultiFingerGestureDetectionTestOnDisplay(displayId);
            } finally {
                sInstrumentation.runOnMainSync(() -> {
                    activity.finish();
                });
                sInstrumentation.waitForIdleSync();
            }
        }
    }

    private void runGestureDetectionTestOnDisplay(int displayId) {
        // Compute gesture stroke lengths, in pixels.
        final int dx = mStrokeLenPxX;
        final int dy = mStrokeLenPxY;

        // Test recognizing various gestures.
        testGesture(
                doubleTap(displayId),
                AccessibilityService.GESTURE_DOUBLE_TAP,
                displayId);
        testGesture(
                doubleTapAndHold(displayId),
                AccessibilityService.GESTURE_DOUBLE_TAP_AND_HOLD,
                displayId);
        testPath(p(-dx, +0), AccessibilityService.GESTURE_SWIPE_LEFT, displayId);
        testPath(p(+dx, +0), AccessibilityService.GESTURE_SWIPE_RIGHT, displayId);
        testPath(p(+0, -dy), AccessibilityService.GESTURE_SWIPE_UP, displayId);
        testPath(p(+0, +dy), AccessibilityService.GESTURE_SWIPE_DOWN, displayId);

        testPath(p(-dx, +0), p(+0, +0), AccessibilityService.GESTURE_SWIPE_LEFT_AND_RIGHT,
                displayId);
        testPath(p(-dx, +0), p(-dx, -dy), AccessibilityService.GESTURE_SWIPE_LEFT_AND_UP,
                displayId);
        testPath(p(-dx, +0), p(-dx, +dy), AccessibilityService.GESTURE_SWIPE_LEFT_AND_DOWN,
                displayId);

        testPath(p(+dx, +0), p(+0, +0), AccessibilityService.GESTURE_SWIPE_RIGHT_AND_LEFT,
                displayId);
        testPath(p(+dx, +0), p(+dx, -dy), AccessibilityService.GESTURE_SWIPE_RIGHT_AND_UP,
                displayId);
        testPath(p(+dx, +0), p(+dx, +dy), AccessibilityService.GESTURE_SWIPE_RIGHT_AND_DOWN,
                displayId);

        testPath(p(+0, -dy), p(-dx, -dy), AccessibilityService.GESTURE_SWIPE_UP_AND_LEFT,
                displayId);
        testPath(p(+0, -dy), p(+dx, -dy), AccessibilityService.GESTURE_SWIPE_UP_AND_RIGHT,
                displayId);
        testPath(p(+0, -dy), p(+0, +0), AccessibilityService.GESTURE_SWIPE_UP_AND_DOWN,
                displayId);

        testPath(p(+0, +dy), p(-dx, +dy), AccessibilityService.GESTURE_SWIPE_DOWN_AND_LEFT,
                displayId);
        testPath(p(+0, +dy), p(+dx, +dy), AccessibilityService.GESTURE_SWIPE_DOWN_AND_RIGHT,
                displayId);
        testPath(p(+0, +dy), p(+0, +0), AccessibilityService.GESTURE_SWIPE_DOWN_AND_UP,
                displayId);
    }

    private void runMultiFingerGestureDetectionTestOnDisplay(int displayId) {
        // Compute gesture stroke lengths, in pixels.
        final int dx = mStrokeLenPxX;
        final int dy = mStrokeLenPxY;
        testGesture(
                twoFingerSingleTap(displayId),
                AccessibilityService.GESTURE_2_FINGER_SINGLE_TAP,
                displayId);
        testGesture(
                twoFingerDoubleTap(displayId),
                AccessibilityService.GESTURE_2_FINGER_DOUBLE_TAP,
                displayId);
                testGesture(
                twoFingerDoubleTapAndHold(displayId),
                AccessibilityService.GESTURE_2_FINGER_DOUBLE_TAP_AND_HOLD,
                displayId);
        testGesture(
                twoFingerTripleTap(displayId),
                AccessibilityService.GESTURE_2_FINGER_TRIPLE_TAP,
                displayId);

        testGesture(
                threeFingerSingleTap(displayId),
                AccessibilityService.GESTURE_3_FINGER_SINGLE_TAP,
                displayId);
        testGesture(
                threeFingerDoubleTap(displayId),
                AccessibilityService.GESTURE_3_FINGER_DOUBLE_TAP,
                displayId);
                testGesture(
                threeFingerDoubleTapAndHold(displayId),
                AccessibilityService.GESTURE_3_FINGER_DOUBLE_TAP_AND_HOLD,
                displayId);
        testGesture(
                threeFingerTripleTap(displayId),
                AccessibilityService.GESTURE_3_FINGER_TRIPLE_TAP,
                displayId);

        testGesture(
                fourFingerSingleTap(displayId),
                AccessibilityService.GESTURE_4_FINGER_SINGLE_TAP,
                displayId);
        testGesture(
                fourFingerDoubleTap(displayId),
                AccessibilityService.GESTURE_4_FINGER_DOUBLE_TAP,
                displayId);
                testGesture(
                fourFingerDoubleTapAndHold(displayId),
                AccessibilityService.GESTURE_4_FINGER_DOUBLE_TAP_AND_HOLD,
                displayId);
        testGesture(
                fourFingerTripleTap(displayId),
                AccessibilityService.GESTURE_4_FINGER_TRIPLE_TAP,
                displayId);

        testGesture(
                MultiFingerSwipe(displayId, 3, 0, dy),
                AccessibilityService.GESTURE_3_FINGER_SWIPE_DOWN,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 3, -dx, 0),
                AccessibilityService.GESTURE_3_FINGER_SWIPE_LEFT,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 3, dx, 0),
                AccessibilityService.GESTURE_3_FINGER_SWIPE_RIGHT,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 3, 0, -dy),
                AccessibilityService.GESTURE_3_FINGER_SWIPE_UP,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 4, 0, dy),
                AccessibilityService.GESTURE_4_FINGER_SWIPE_DOWN,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 4, -dx, 0),
                AccessibilityService.GESTURE_4_FINGER_SWIPE_LEFT,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 4, dx, 0),
                AccessibilityService.GESTURE_4_FINGER_SWIPE_RIGHT,
                displayId);
        testGesture(
                MultiFingerSwipe(displayId, 4, 0, -dy),
                AccessibilityService.GESTURE_4_FINGER_SWIPE_UP,
                displayId);
    }

    /** Convenient short alias to make a Point. */
    private static Point p(int x, int y) {
        return new Point(x, y);
    }

    /** Test recognizing path from PATH_START to PATH_START+delta on default display. */
    private void testPath(Point delta, int gestureId) {
        testPath(delta, null, gestureId, Display.DEFAULT_DISPLAY);
    }

    /** Test recognizing path from PATH_START to PATH_START+delta on specified display. */
    private void testPath(Point delta, int gestureId, int displayId) {
        testPath(delta, null, gestureId, displayId);
    }
    /** Test recognizing path from PATH_START to PATH_START+delta on default display. */
    private void testPath(Point delta1, Point delta2, int gestureId) {
        testPath(delta1, delta2, gestureId, Display.DEFAULT_DISPLAY);
    }

    /**
     * Test recognizing path from PATH_START to PATH_START+delta1 to PATH_START+delta2. on specified
     * display.
     */
    private void testPath(Point delta1, Point delta2, int gestureId, int displayId) {
        // Create gesture motions.
        int numPathSegments = (delta2 == null) ? 1 : 2;
        long pathDurationMs = numPathSegments * STROKE_MS;
        GestureDescription gesture = new GestureDescription.Builder()
                .addStroke(new StrokeDescription(
                linePath(mCenter, delta1, delta2), 0, pathDurationMs, false))
                .setDisplayId(displayId)
                .build();

        testGesture(gesture, gestureId, displayId);
    }

    /** Dispatch a gesture and make sure it is detected as the specified gesture id. */
    private void testGesture(GestureDescription gesture, int gestureId, int displayId) {
        // Dispatch gesture motions to specified  display with GestureDescription..
        // Use AccessibilityService.dispatchGesture() instead of Instrumentation.sendPointerSync()
        // because accessibility services read gesture events upstream from the point where
        // sendPointerSync() injects events.
        mService.clearGestures();
        mService.runOnServiceSync(() ->
        mService.dispatchGesture(gesture, mGestureDispatchCallback, null));
        verify(mGestureDispatchCallback, timeout(GESTURE_DISPATCH_TIMEOUT_MS).atLeastOnce())
                .onCompleted(any());

        // Wait for gesture recognizer, and check recognized gesture.
        mService.assertGestureReceived(gestureId, displayId);
    }
    /** Create a path from startPoint, moving by delta1, then delta2. (delta2 may be null.) */
    Path linePath(Point startPoint, Point delta1, Point delta2) {
        Path path = new Path();
        path.moveTo(startPoint.x, startPoint.y);
        path.lineTo(startPoint.x + delta1.x, startPoint.y + delta1.y);
        if (delta2 != null) {
            path.lineTo(startPoint.x + delta2.x, startPoint.y + delta2.y);
        }
        return path;
    }

    @Test
    @AppModeFull
    public void testVerifyGestureTouchEvent() {
        if (!mHasTouchScreen || !mScreenBigEnough) {
            return;
        }

        verifyGestureTouchEventOnDisplay(Display.DEFAULT_DISPLAY);
        verifyMultiFingerGestureTouchEventOnDisplay(Display.DEFAULT_DISPLAY);
    }

    @Test
    @AppModeFull
    public void testVerifyGestureTouchEventOnVirtualDisplay() throws Exception {
        if (!mHasTouchScreen || !mScreenBigEnough) {
            return;
        }

        try (final VirtualDisplaySession displaySession = new VirtualDisplaySession()) {
            final int displayId = displaySession.createDisplayWithDefaultDisplayMetricsAndWait(
                    sInstrumentation.getTargetContext(),
                    false).getDisplayId();

            // Launches an activity on virtual display to meet a real situation.
            final Activity activity = launchActivityOnSpecifiedDisplayAndWaitForItToBeOnscreen(
                    sInstrumentation, sUiAutomation, AccessibilityWindowQueryActivity.class,
                    displayId);
            try {
                verifyGestureTouchEventOnDisplay(displayId);
                verifyMultiFingerGestureTouchEventOnDisplay(displayId);
            } finally {
                sInstrumentation.runOnMainSync(() -> {
                    activity.finish();
                });
                sInstrumentation.waitForIdleSync();
            }
        }
    }

    private void verifyGestureTouchEventOnDisplay(int displayId) {
        assertEventAfterGesture(swipe(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);

        assertEventAfterGesture(tap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START,
                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);

        assertEventAfterGesture(doubleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);

        assertEventAfterGesture(doubleTapAndHold(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
    }

    private void verifyMultiFingerGestureTouchEventOnDisplay(int displayId) {
        assertEventAfterGesture(twoFingerSingleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
        assertEventAfterGesture(twoFingerDoubleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
        assertEventAfterGesture(twoFingerTripleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);

        assertEventAfterGesture(threeFingerSingleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
        assertEventAfterGesture(threeFingerDoubleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
        assertEventAfterGesture(threeFingerTripleTap(displayId),
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_START,
                AccessibilityEvent.TYPE_TOUCH_INTERACTION_END);
    }

    @Test
    @AppModeFull
    public void testDispatchGesture_privateDisplay_gestureCancelled() throws Exception{
        if (!mHasTouchScreen || !mScreenBigEnough) {
            return;
        }

        try (final VirtualDisplaySession displaySession = new VirtualDisplaySession()) {
            final int displayId = displaySession.createDisplayWithDefaultDisplayMetricsAndWait
                    (sInstrumentation.getTargetContext(),
                            true).getDisplayId();
            GestureDescription gesture = swipe(displayId);
            mService.clearGestures();
            mService.runOnServiceSync(() ->
                    mService.dispatchGesture(gesture, mGestureDispatchCallback, null));
            verify(mGestureDispatchCallback, timeout(GESTURE_DISPATCH_TIMEOUT_MS).atLeastOnce())
                    .onCancelled(any());
        }
    }

    /** Test touch for accessibility events */
    private void assertEventAfterGesture(GestureDescription gesture, int... events) {
        mService.clearEvents();
        mService.runOnServiceSync(
                () -> mService.dispatchGesture(gesture, mGestureDispatchCallback, null));
        verify(mGestureDispatchCallback, timeout(EVENT_DISPATCH_TIMEOUT_MS).atLeastOnce())
                .onCompleted(any());

        mService.assertPropagated(events);
    }

    private GestureDescription swipe(int displayId) {
        StrokeDescription swipe = new StrokeDescription(
                linePath(mCenter, p(0, mStrokeLenPxY), null), 0, STROKE_MS, false);
        return getGestureBuilder(displayId, swipe).build();
    }

    private GestureDescription tap(int displayId) {
        StrokeDescription tap = click(mTapLocation);
        return getGestureBuilder(displayId, tap).build();
    }

    private GestureDescription doubleTap(int displayId) {
        StrokeDescription tap1 = click(mTapLocation);
        StrokeDescription tap2 = startingAt(endTimeOf(tap1) + 20, click(mTapLocation));
        return getGestureBuilder(displayId, tap1, tap2).build();
    }

    private GestureDescription doubleTapAndHold(int displayId) {
        StrokeDescription tap1 = click(mTapLocation);
        StrokeDescription tap2 = startingAt(endTimeOf(tap1) + 20, longClick(mTapLocation));
        return getGestureBuilder(displayId, tap1, tap2).build();
    }

    private GestureDescription twoFingerSingleTap(int displayId) {
        return multiFingerMultiTap(2, 1, displayId);
    }

    private GestureDescription twoFingerDoubleTap(int displayId) {
        return multiFingerMultiTap(2, 2, displayId);
    }

    private GestureDescription twoFingerDoubleTapAndHold(int displayId) {
        return multiFingerMultiTapAndHold(2, 2, displayId);
    }
 
    private GestureDescription twoFingerTripleTap(int displayId) {
        return multiFingerMultiTap(2, 3, displayId);
    }

    private GestureDescription threeFingerSingleTap(int displayId) {
        return multiFingerMultiTap(3, 1, displayId);
    }

    private GestureDescription threeFingerDoubleTap(int displayId) {
        return multiFingerMultiTap(3, 2, displayId);
    }

    private GestureDescription threeFingerDoubleTapAndHold(int displayId) {
        return multiFingerMultiTapAndHold(3, 2, displayId);
    }

    private GestureDescription threeFingerTripleTap(int displayId) {
        return multiFingerMultiTap(3, 3, displayId);
    }

    private GestureDescription fourFingerSingleTap(int displayId) {
        return multiFingerMultiTap(4, 1, displayId);
    }

    private GestureDescription fourFingerDoubleTap(int displayId) {
        return multiFingerMultiTap(4, 2, displayId);
    }

    private GestureDescription fourFingerDoubleTapAndHold(int displayId) {
        return multiFingerMultiTapAndHold(4, 2, displayId);
    }

    private GestureDescription fourFingerTripleTap(int displayId) {
        return multiFingerMultiTap(4, 3, displayId);
    }

    private GestureDescription multiFingerMultiTap(int fingerCount, int tapCount, int displayId) {
        // We dispatch the first finger, base, placed at left down side by an offset
        // from the center of the display and the rest ones at right up side by delta
        // from the base.
        final PointF base = diff(mTapLocation, FINGER_OFFSET_PX);
        return GestureUtils.multiFingerMultiTap(
                base, FINGER_OFFSET_PX, fingerCount, tapCount, /* slop= */ 0, displayId);
    }

    private GestureDescription multiFingerMultiTapAndHold(
            int fingerCount, int tapCount, int displayId) {
        // We dispatch the first finger, base, placed at left down side by an offset
        // from the center of the display and the rest ones at right up side by delta
        // from the base.
        final PointF base = diff(mTapLocation, FINGER_OFFSET_PX);
        return GestureUtils.multiFingerMultiTapAndHold(
                base, FINGER_OFFSET_PX, fingerCount, tapCount, /* slop= */ 0, displayId);
    }

    private GestureDescription MultiFingerSwipe(
            int displayId, int fingerCount, float dx, float dy) {
        float fingerOffset = 10f;
        GestureDescription.Builder builder = new GestureDescription.Builder();
        builder.setDisplayId(displayId);
        for (int currentFinger = 0; currentFinger < fingerCount; ++currentFinger) {
            builder.addStroke(
                    GestureUtils.swipe(
                            add(mTapLocation, fingerOffset * currentFinger, 0),
                            add(mTapLocation, dx + (fingerOffset * currentFinger), dy),
                            STROKE_MS));
        }
        return builder.build();
    }
}
