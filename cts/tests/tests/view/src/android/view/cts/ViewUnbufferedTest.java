/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static android.view.InputDevice.SOURCE_CLASS_JOYSTICK;
import static android.view.InputDevice.SOURCE_CLASS_NONE;
import static android.view.InputDevice.SOURCE_CLASS_POINTER;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.graphics.Point;
import android.os.SystemClock;
import android.view.Choreographer;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicInteger;

@MediumTest
@RunWith(AndroidJUnit4.class)
// Test View.requestUnbufferedDispatch API.
//
// In this test, we try to synchronously inject input events to the app, and look at how many events
// we are able to inject during a single VSYNC period.
//
// During normal (buffered) dispatch, input event would remain pending until the VSYNC callback.
// In that scenario, we can send at most 1 input event per VSYNC.
//
// During unbuffered dispatch, input event would be received and handled by the app right away.
// We should be able to send multiple input events to the app within a single VSYNC period.
//
// For this test, we count the max number of synchronously injected events in a single VSYNC period.
// If we can achieve at most 1 event per VSYNC, we must be in normal (batched) mode.
// If we are receiving more than 1 event per VSYNC, we must be in unbuffered mode.
// This test assumes that the time that it takes to inject an event to receiver is less than one
// frame.
//
// This test is executed for joystick, touch, and hover events.
public class ViewUnbufferedTest {
    private static final int SAMPLE_COUNT = 20;
    private static final int POS_STEP = 5;

    @Rule
    public ActivityTestRule<ViewUnbufferedTestActivity> mActivityRule =
            new ActivityTestRule<>(ViewUnbufferedTestActivity.class);

    private Instrumentation mInstrumentation;
    private UiAutomation mAutomation;
    private Activity mActivity;
    private View mView;
    private Choreographer mChoreographer;

    private final BlockingQueue<ReceivedEvent> mReceivedEvents = new LinkedBlockingQueue<>();
    private final BlockingQueue<MotionEvent> mSentEvents = new LinkedBlockingQueue<>();

    private AtomicInteger mReceivedCountPerFrame = new AtomicInteger(0);
    private int mMaxReceivedCountPerFrame;
    private boolean mIsRunning;
    private final Choreographer.FrameCallback mFrameCallback = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long l) {
            mReceivedCountPerFrame.set(0);
            if (mIsRunning) {
                mChoreographer.postFrameCallback(this);
            }
        }
    };

    private final class ReceivedEvent {
        final long mEventTime;
        final int mAction;
        final int mX;
        final int mY;
        final int mSource;

        ReceivedEvent(long eventTime, int action, int x, int y, int source) {
            mEventTime = eventTime;
            mAction = action;
            mX = x;
            mY = y;
            mSource = source;
        }
    }

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mAutomation = mInstrumentation.getUiAutomation();
        mActivity = mActivityRule.getActivity();
        mInstrumentation.runOnMainSync(() -> mChoreographer = Choreographer.getInstance());
        mReceivedEvents.clear();
        mSentEvents.clear();
        mReceivedCountPerFrame.set(0);
        mMaxReceivedCountPerFrame = 0;
        PollingCheck.waitFor(mActivity::hasWindowFocus);
        mView = mActivity.findViewById(R.id.test_view);
    }

    @After
    public void tearDown() {
        mIsRunning = false;
        mChoreographer.removeFrameCallback(mFrameCallback);
    }

    // If resampling happened, the coordinates and event time would resample to new position.
    private static void compareEvent(final MotionEvent sentEvent,
            final ReceivedEvent receivedEvent, final int[] offsets) {
        assertEquals(sentEvent.getAction(), receivedEvent.mAction);
        assertEquals((int) sentEvent.getX(), receivedEvent.mX + offsets[0]);
        assertEquals((int) sentEvent.getY(), receivedEvent.mY + offsets[1]);
        assertEquals(sentEvent.getEventTime(), receivedEvent.mEventTime);
        assertEquals(sentEvent.getSource(), receivedEvent.mSource);
    }

    private void compareEvents(final BlockingQueue<MotionEvent> sentEvents,
            final BlockingQueue<ReceivedEvent> receivedEvents, final int[] offsets) {
        assertEquals(sentEvents.size(), receivedEvents.size());

        for (int i = 0; i < sentEvents.size(); i++) {
            MotionEvent sentEvent = sentEvents.poll();
            ReceivedEvent receivedEvent = receivedEvents.poll();
            compareEvent(sentEvent, receivedEvent, offsets);
        }
    }

    private int[] getViewLocationOnScreen(View view) {
        final int[] xy = new int[2];
        view.getLocationOnScreen(xy);
        return xy;
    }

    private Point getViewCenterOnScreen(View view) {
        final int[] xy = getViewLocationOnScreen(view);
        final int viewWidth = view.getWidth();
        final int viewHeight = view.getHeight();

        return new Point(xy[0] + viewWidth / 2, xy[1] + viewHeight / 2);
    }

    // Sent events with first and last events, rests are move events.
    private void sendPointerEvents(int x, int y, int[] actions, int source) {
        // Send a start event to make sure view handled unbuffered request.
        final long downTime = SystemClock.uptimeMillis();
        MotionEvent downEvent =
                MotionEvent.obtain(downTime, downTime, actions[0], x, y, 0);
        downEvent.setSource(source);
        mAutomation.injectInputEvent(downEvent, true /* sync */);

        // Inject move events.
        startResetReceivedCountPerFrame();
        for (int i = 0; i < SAMPLE_COUNT; i++, x += POS_STEP) {
            // Stop the loop when we receive more than 1 event, since we only care about whether
            // the events are buffered or not, and not about finding the actual max value.
            if (mMaxReceivedCountPerFrame > 1) {
                break;
            }

            final long eventTime = SystemClock.uptimeMillis();
            final MotionEvent moveEvent = MotionEvent.obtain(downTime, eventTime,
                    actions[1], x, y, 0);
            moveEvent.setSource(source);
            mAutomation.injectInputEvent(moveEvent, true /* sync */);
            mSentEvents.add(moveEvent);
        }

        // Send end event to make sure all events had been handled.
        final MotionEvent upEvent = MotionEvent.obtain(downTime, SystemClock.uptimeMillis(),
                actions[2], x, y, 0);
        upEvent.setSource(source);
        mAutomation.injectInputEvent(upEvent, true /* sync */);
    }

    // Joystick events could always be move events.
    private void sendJoystickEvents(int x, int y) {
        // Inject move events.
        startResetReceivedCountPerFrame();
        for (int i = 0; i < SAMPLE_COUNT; i++, x += POS_STEP) {
            // Stop the loop when we receive more than 1 event, since we only care about whether
            // the events are buffered or not, and not about finding the actual max value.
            if (mMaxReceivedCountPerFrame > 1) {
                break;
            }

            final long eventTime = SystemClock.uptimeMillis();
            final MotionEvent moveEvent = MotionEvent.obtain(eventTime, eventTime,
                    MotionEvent.ACTION_MOVE, x, y, 0);
            moveEvent.setSource(InputDevice.SOURCE_JOYSTICK);
            mAutomation.injectInputEvent(moveEvent, true /* sync */);
            mSentEvents.add(moveEvent);
        }
    }

    // Store move/hover move event into mReceivedEvents and unpack any historical
    // events.
    private boolean receivedEvent(MotionEvent event) {
        // Only care about move or hover move due to rest are not to be batched and might be sent
        // in same frame.
        if (event.getAction() != MotionEvent.ACTION_MOVE
                && event.getAction() != MotionEvent.ACTION_HOVER_MOVE) {
            // Always return true to make sure the event has been handled.
            return true;
        }

        // Increase the count and store the maximum value.
        final int count = mReceivedCountPerFrame.incrementAndGet();
        if (count > mMaxReceivedCountPerFrame) {
            mMaxReceivedCountPerFrame = count;
        }

        mReceivedEvents.add(new ReceivedEvent(event.getEventTime(), event.getAction(),
                (int) event.getX(), (int) event.getY(), event.getSource()));

        // Always return true to make sure the event has been handled.
        return true;
    }

    // Start the loop to reset the received count per frame.
    private void startResetReceivedCountPerFrame() {
        mIsRunning = true;
        mChoreographer.postFrameCallback(mFrameCallback);
    }

    // Normal view would expect to receive the buffered event.
    @Test
    public void testNormalTouch() {
        mView.setOnTouchListener((v, event) -> receivedEvent(event));

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);

        // Expect the buffered event could be received 1 per frame.
        assertFalse(mReceivedEvents.isEmpty());
        assertEquals(1, mMaxReceivedCountPerFrame);
    }

    // Test view requested touch screen unbuffered.
    @Test
    public void testUnbufferedTouch() {
        mView.setOnTouchListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_POINTER);

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);

        assertTrue(mMaxReceivedCountPerFrame > 1);
        compareEvents(mSentEvents, mReceivedEvents, getViewLocationOnScreen(mView));
    }

    // Test view requested touch screen unbuffered from MotionEvent.
    @Test
    public void testUnbufferedTouchByEvent() {
        mView.setOnTouchListener((v, event) -> {
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                v.requestUnbufferedDispatch(event);
            }
            return receivedEvent(event);
        });

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);

        assertTrue(mMaxReceivedCountPerFrame > 1);
        compareEvents(mSentEvents, mReceivedEvents, getViewLocationOnScreen(mView));
    }

    // Test view requested unbuffered source but reset it later.
    @Test
    public void testUnbufferedTouch_Reset() {
        mView.setOnTouchListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_POINTER);

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);
        assertTrue(mMaxReceivedCountPerFrame > 1);

        // Reset the unbuffered dispatch and re-send the events.
        mMaxReceivedCountPerFrame = 0;
        mView.requestUnbufferedDispatch(SOURCE_CLASS_NONE);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);

        assertEquals(1, mMaxReceivedCountPerFrame);
    }

    // Test view requested joystick unbuffered.
    @Test
    public void testUnbufferedJoystick() {
        mView.setOnGenericMotionListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_JOYSTICK);
        PollingCheck.waitFor(mView::hasFocus);

        sendJoystickEvents(0, 0);

        assertTrue(mMaxReceivedCountPerFrame > 1);
        compareEvents(mSentEvents, mReceivedEvents, new int[]{0, 0});
    }

    // Test view requested joystick unbuffered but no focus.
    @Test
    public void testUnbufferedJoystick_NoFocus() {
        mView.setOnGenericMotionListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_JOYSTICK);

        final View normalView = mActivity.findViewById(R.id.normal_view);
        normalView.setOnGenericMotionListener((v, event) -> receivedEvent(event));
        mInstrumentation.runOnMainSync(() -> normalView.requestFocus());
        PollingCheck.waitFor(normalView::hasFocus);

        sendJoystickEvents(0, 0);
        assertEquals(1, mMaxReceivedCountPerFrame);
    }

    // Test view requested unbuffered for hover events.
    @Test
    public void testUnbufferedHover() {
        mView.setOnHoverListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_POINTER);

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_HOVER_ENTER,
                        MotionEvent.ACTION_HOVER_MOVE,
                        MotionEvent.ACTION_HOVER_EXIT},
                InputDevice.SOURCE_TOUCHSCREEN);

        assertTrue(mMaxReceivedCountPerFrame > 1);
        compareEvents(mSentEvents, mReceivedEvents, getViewLocationOnScreen(mView));
    }

    // Test view requested different source unbuffered from the received events.
    @Test
    public void testUnbuffered_DifferentSource() {
        mView.setOnTouchListener((v, event) -> receivedEvent(event));
        mView.requestUnbufferedDispatch(SOURCE_CLASS_JOYSTICK);

        final Point point = getViewCenterOnScreen(mView);
        sendPointerEvents(point.x, point.y,
                new int[]{MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE, MotionEvent.ACTION_UP},
                InputDevice.SOURCE_TOUCHSCREEN);

        assertEquals(1, mMaxReceivedCountPerFrame);
    }
}
