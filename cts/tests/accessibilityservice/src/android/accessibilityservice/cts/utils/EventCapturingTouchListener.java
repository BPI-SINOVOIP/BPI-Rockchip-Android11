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

package android.accessibilityservice.cts.utils;

import static android.view.MotionEvent.ACTION_MOVE;

import static java.util.concurrent.TimeUnit.SECONDS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class EventCapturingTouchListener implements View.OnTouchListener {
    private static final long WAIT_TIME_SECONDS = 5;
    private static final long MIN_WAIT_TIME_SECONDS = 2;
    // whether or not to keep events from propagating to other listeners
    private boolean shouldConsumeEvents;
    private final BlockingQueue<MotionEvent> events = new LinkedBlockingQueue<>();

    public EventCapturingTouchListener(boolean shouldConsumeEvents) {
        this.shouldConsumeEvents = shouldConsumeEvents;
    }

    public EventCapturingTouchListener() {
        this.shouldConsumeEvents = true;
    }

    @Override
    public boolean onTouch(View view, MotionEvent motionEvent) {
        assertTrue(events.offer(MotionEvent.obtain(motionEvent)));
        return shouldConsumeEvents;
    }

    /** Insure that no touch events have been detected. */
    public void assertNonePropagated() {
        try {
            MotionEvent event = events.poll(MIN_WAIT_TIME_SECONDS, SECONDS);
            if (event != null) {
                fail("Unexpected touch event " + event.toString());
            }
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check for the specified touch events. Note that specifying ACTION_MOVE will match one or more
     * consecutive ACTION_MOVE events.
     */
    public void assertPropagated(int... eventTypes) {
        MotionEvent ev;
        try {
            List<String> expected = new ArrayList<>();
            List<String> received = new ArrayList<>();
            for (int e : eventTypes) {
                expected.add(MotionEvent.actionToString(e));
            }
            ev = events.poll(WAIT_TIME_SECONDS, SECONDS);
            assertNotNull(
                    "Expected " + expected + " but none present after " + WAIT_TIME_SECONDS
                            + " seconds",
                    ev);
            // By this point there is at least one received event.
            received.add(MotionEvent.actionToString(ev.getActionMasked()));
            ev = events.poll(WAIT_TIME_SECONDS, SECONDS);
            while (ev != null) {
                int action = ev.getActionMasked();
                if (action != ACTION_MOVE) {
                    received.add(MotionEvent.actionToString(action));
                } else {
                    // Add the current event if the previous received event was not ACTION_MOVE
                    String prev = received.get(received.size() - 1);
                    if (!prev.equals(MotionEvent.actionToString(ACTION_MOVE))) {
                        received.add(MotionEvent.actionToString(action));
                    }
                }
                ev = events.poll(WAIT_TIME_SECONDS, SECONDS);
            }
            assertEquals(expected, received);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    public List<MotionEvent> getRawEvents() {
        List<MotionEvent> motionEvents = new ArrayList<>();
        MotionEvent ev;
        try {
            ev = events.poll(WAIT_TIME_SECONDS, SECONDS);
            while (ev != null) {
                motionEvents.add(ev);
                ev = events.poll(WAIT_TIME_SECONDS, SECONDS);
            }
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        assertFalse(motionEvents.isEmpty());
        return motionEvents;
    }
}
