/*
 * Copyright (C) 2017 The Android Open Source Project
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
import static android.accessibilityservice.cts.utils.AsyncUtils.waitOn;
import static android.accessibilityservice.cts.utils.GestureUtils.add;
import static android.accessibilityservice.cts.utils.GestureUtils.click;
import static android.accessibilityservice.cts.utils.GestureUtils.dispatchGesture;
import static android.accessibilityservice.cts.utils.GestureUtils.distance;
import static android.accessibilityservice.cts.utils.GestureUtils.doubleTap;
import static android.accessibilityservice.cts.utils.GestureUtils.drag;
import static android.accessibilityservice.cts.utils.GestureUtils.endTimeOf;
import static android.accessibilityservice.cts.utils.GestureUtils.lastPointOf;
import static android.accessibilityservice.cts.utils.GestureUtils.longClick;
import static android.accessibilityservice.cts.utils.GestureUtils.path;
import static android.accessibilityservice.cts.utils.GestureUtils.pointerDown;
import static android.accessibilityservice.cts.utils.GestureUtils.pointerUp;
import static android.accessibilityservice.cts.utils.GestureUtils.startingAt;
import static android.accessibilityservice.cts.utils.GestureUtils.swipe;
import static android.accessibilityservice.cts.utils.GestureUtils.tripleTap;
import static android.view.MotionEvent.ACTION_DOWN;
import static android.view.MotionEvent.ACTION_MOVE;
import static android.view.MotionEvent.ACTION_UP;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.GestureDescription;
import android.accessibilityservice.GestureDescription.StrokeDescription;
import android.accessibilityservice.cts.AccessibilityGestureDispatchTest.GestureDispatchActivity;
import android.accessibilityservice.cts.utils.EventCapturingTouchListener;
import android.app.Instrumentation;
import android.content.pm.PackageManager;
import android.graphics.PointF;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.view.ViewConfiguration;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

/**
 * Class for testing magnification.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class MagnificationGestureHandlerTest {

    private static final double MIN_SCALE = 1.2;

    private InstrumentedAccessibilityService mService;
    private Instrumentation mInstrumentation;
    private EventCapturingTouchListener mTouchListener = new EventCapturingTouchListener();
    float mCurrentScale = 1f;
    PointF mCurrentZoomCenter = null;
    PointF mTapLocation;
    PointF mTapLocation2;
    float mPan;
    private boolean mHasTouchscreen;
    private boolean mOriginalIsMagnificationEnabled;

    private final Object mZoomLock = new Object();

    private ActivityTestRule<GestureDispatchActivity> mActivityRule =
            new ActivityTestRule<>(GestureDispatchActivity.class);

    private InstrumentedAccessibilityServiceTestRule<StubMagnificationAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    StubMagnificationAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mServiceRule)
            .around(mDumpOnFailureRule);

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        PackageManager pm = mInstrumentation.getContext().getPackageManager();
        mHasTouchscreen = pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
                || pm.hasSystemFeature(PackageManager.FEATURE_FAKETOUCH);
        if (!mHasTouchscreen) return;

        mOriginalIsMagnificationEnabled =
                Settings.Secure.getInt(mInstrumentation.getContext().getContentResolver(),
                        Settings.Secure.ACCESSIBILITY_DISPLAY_MAGNIFICATION_ENABLED, 0) == 1;
        setMagnificationEnabled(true);

        mService = mServiceRule.enableService();
        mService.getMagnificationController().addListener(
                (controller, region, scale, centerX, centerY) -> {
                    mCurrentScale = scale;
                    mCurrentZoomCenter = isZoomed() ? new PointF(centerX, centerY) : null;

                    synchronized (mZoomLock) {
                        mZoomLock.notifyAll();
                    }
                });

        TextView view = mActivityRule.getActivity().findViewById(R.id.full_screen_text_view);
        mInstrumentation.runOnMainSync(() -> {
            view.setOnTouchListener(mTouchListener);
            int[] xy = new int[2];
            view.getLocationOnScreen(xy);
            mTapLocation = new PointF(xy[0] + view.getWidth() / 2, xy[1] + view.getHeight() / 2);
            mTapLocation2 = add(mTapLocation, 31, 29);
            mPan = view.getWidth() / 4;
        });
    }

    @After
    public void tearDown() throws Exception {
        if (!mHasTouchscreen) return;

        setMagnificationEnabled(mOriginalIsMagnificationEnabled);
    }

    @Test
    public void testZoomOnOff() {
        if (!mHasTouchscreen) return;

        assertFalse(isZoomed());

        assertGesturesPropagateToView();
        assertFalse(isZoomed());

        setZoomByTripleTapping(true);

        assertGesturesPropagateToView();
        assertTrue(isZoomed());

        setZoomByTripleTapping(false);
    }

    @Test
    public void testViewportDragging() {
        if (!mHasTouchscreen) return;

        assertFalse(isZoomed());
        tripleTapAndDragViewport();
        waitOn(mZoomLock, () -> !isZoomed());

        setZoomByTripleTapping(true);
        tripleTapAndDragViewport();
        assertTrue(isZoomed());

        setZoomByTripleTapping(false);
    }

    @Test
    public void testPanning() {
        //The minimum movement to transit to panningState.
        final float minSwipeDistance = ViewConfiguration.get(
                mInstrumentation.getContext()).getScaledTouchSlop() + 1;
        final boolean screenBigEnough = mPan > minSwipeDistance;
        if (!mHasTouchscreen || !screenBigEnough) return;
        assertFalse(isZoomed());

        setZoomByTripleTapping(true);
        final PointF oldCenter = mCurrentZoomCenter;

        // Dispatch a swipe gesture composed of two consecutive gestures; the first one to transit
        // to panningState, and the second one to moves the window.
        final GestureDescription.Builder builder1 = new GestureDescription.Builder();
        final GestureDescription.Builder builder2 = new GestureDescription.Builder();

        final long totalDuration = ViewConfiguration.getTapTimeout();
        final long firstDuration = (long)(totalDuration * (minSwipeDistance / mPan));

        for (final PointF startPoint : new PointF[]{mTapLocation, mTapLocation2}) {
            final PointF midPoint = add(startPoint, -minSwipeDistance, 0);
            final PointF endPoint = add(startPoint, -mPan, 0);
            final StrokeDescription firstStroke = new StrokeDescription(path(startPoint, midPoint),
                    0, firstDuration, true);
            final StrokeDescription secondStroke = firstStroke.continueStroke(
                    path(midPoint, endPoint), 0, totalDuration - firstDuration, false);
            builder1.addStroke(firstStroke);
            builder2.addStroke(secondStroke);
        }

        dispatch(builder1.build());
        dispatch(builder2.build());

        waitOn(mZoomLock,
                () -> (mCurrentZoomCenter.x - oldCenter.x
                        >= (mPan - minSwipeDistance) / mCurrentScale * 0.9));

        setZoomByTripleTapping(false);
    }

    private void setZoomByTripleTapping(boolean desiredZoomState) {
        if (isZoomed() == desiredZoomState) return;
        dispatch(tripleTap(mTapLocation));
        waitOn(mZoomLock, () -> isZoomed() == desiredZoomState);
        mTouchListener.assertNonePropagated();
    }

    private void tripleTapAndDragViewport() {
        StrokeDescription down = tripleTapAndHold();

        PointF oldCenter = mCurrentZoomCenter;

        StrokeDescription drag = drag(down, add(lastPointOf(down), mPan, 0f));
        dispatch(drag);
        waitOn(mZoomLock, () -> distance(mCurrentZoomCenter, oldCenter) >= mPan / 5);
        assertTrue(isZoomed());
        mTouchListener.assertNonePropagated();

        dispatch(pointerUp(drag));
        mTouchListener.assertNonePropagated();
    }

    private StrokeDescription tripleTapAndHold() {
        StrokeDescription tap1 = click(mTapLocation);
        StrokeDescription tap2 = startingAt(endTimeOf(tap1) + 20, click(mTapLocation2));
        StrokeDescription down = startingAt(endTimeOf(tap2) + 20, pointerDown(mTapLocation));
        dispatch(tap1, tap2, down);
        waitOn(mZoomLock, () -> isZoomed());
        return down;
    }

    private void assertGesturesPropagateToView() {
        dispatch(click(mTapLocation));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_UP);

        dispatch(longClick(mTapLocation));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_UP);

        dispatch(doubleTap(mTapLocation));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_UP, ACTION_DOWN, ACTION_UP);

        dispatch(swipe(
                mTapLocation,
                add(mTapLocation, 0, 29)));
        mTouchListener.assertPropagated(ACTION_DOWN, ACTION_MOVE, ACTION_UP);
    }

    private void setMagnificationEnabled(boolean enabled) {
        Settings.Secure.putInt(mInstrumentation.getContext().getContentResolver(),
                Settings.Secure.ACCESSIBILITY_DISPLAY_MAGNIFICATION_ENABLED, enabled ? 1 : 0);
    }

    private boolean isZoomed() {
        return mCurrentScale >= MIN_SCALE;
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
}
