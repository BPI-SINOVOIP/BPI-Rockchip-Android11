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

package android.accessibilityservice.cts.utils;

import static android.view.MotionEvent.ACTION_HOVER_MOVE;
import static java.util.concurrent.TimeUnit.SECONDS;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/** This Listener listens for and logs hover events so they can be checked later by tests. */
public class EventCapturingHoverListener implements View.OnHoverListener {

    private boolean shouldConsumeEvents; // whether or not to keep events from propagating to other
    // listeners
    private final BlockingQueue<MotionEvent> mEvents = new LinkedBlockingQueue<>();

    public EventCapturingHoverListener(boolean shouldConsumeEvents) {
        this.shouldConsumeEvents = shouldConsumeEvents;
    }

    public EventCapturingHoverListener() {
        this.shouldConsumeEvents = true;
    }

    @Override
    public boolean onHover(View view, MotionEvent MotionEvent) {
        assertTrue(mEvents.offer(MotionEvent.obtain(MotionEvent)));
        return shouldConsumeEvents;
    }

    /** Insure that no hover events have been detected. */
    public void assertNonePropagated() {
        try {
            long waitTime = 1; // seconds
            MotionEvent event = mEvents.poll(waitTime, SECONDS);
            if (event != null) {
                fail("Unexpected touch event " + event.toString());
            }
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check for the specified hover events. Note that specifying ACTION_HOVER_MOVE will match one
     * or more consecutive ACTION_HOVER_MOVE events.
     */
    public void assertPropagated(int... eventTypes) {
        MotionEvent ev;
        long waitTime = 5; // seconds
        try {
            List<String> expected = new ArrayList<>();
            List<String> received = new ArrayList<>();
            for (int e : eventTypes) {
                expected.add(MotionEvent.actionToString(e));
            }
            ev = mEvents.poll(waitTime, SECONDS);
            assertNotNull(
                    "Expected " + expected + " but none present after " + waitTime + " seconds",
                    ev);
            // By this point there is at least one received event.
            received.add(MotionEvent.actionToString(ev.getActionMasked()));
            ev = mEvents.poll(waitTime, SECONDS);
            while (ev != null) {
                int action = ev.getActionMasked();
                if (action != ACTION_HOVER_MOVE) {
                    received.add(MotionEvent.actionToString(action));
                } else {
                    // Add the current event if the previous received event was not ACTION_MOVE
                    String prev = received.get(received.size() - 1);
                    if (!prev.equals(MotionEvent.actionToString(ACTION_HOVER_MOVE))) {
                        received.add(MotionEvent.actionToString(action));
                    }
                }
                ev = mEvents.poll(waitTime, SECONDS);
            }
            assertEquals(expected, received);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
