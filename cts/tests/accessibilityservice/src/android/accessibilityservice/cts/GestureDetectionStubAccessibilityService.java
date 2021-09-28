/**
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibilityservice.AccessibilityGestureEvent;
import android.view.Display;
import android.view.accessibility.AccessibilityEvent;

import java.util.ArrayList;
import java.util.List;

/** Accessibility service stub, which will collect recognized gestures. */
public class GestureDetectionStubAccessibilityService extends InstrumentedAccessibilityService {
    private static final long GESTURE_RECOGNIZE_TIMEOUT_MS = 3000;
    private static final long EVENT_RECOGNIZE_TIMEOUT_MS = 5000;
    // Member variables
    protected final Object mLock = new Object();
    private ArrayList<Integer> mCollectedGestures = new ArrayList();
    private ArrayList<AccessibilityGestureEvent> mCollectedGestureEvents = new ArrayList();
    protected ArrayList<Integer> mCollectedEvents = new ArrayList();

    @Override
    protected boolean onGesture(int gestureId) {
        synchronized (mCollectedGestures) {
            mCollectedGestures.add(gestureId);
        }
        return true;
    }

    @Override
    public boolean onGesture(AccessibilityGestureEvent gestureEvent) {
        super.onGesture(gestureEvent);
        synchronized (mCollectedGestureEvents) {
            mCollectedGestureEvents.add(gestureEvent);
            mCollectedGestureEvents.notifyAll(); // Stop waiting for gesture.
        }
        return true;
    }

    public void clearGestures() {
        synchronized (mCollectedGestures) {
            mCollectedGestures.clear();
        }
        synchronized (mCollectedGestureEvents) {
            mCollectedGestureEvents.clear();
        }
    }

    public int getGesturesSize() {
        synchronized (mCollectedGestures) {
            return mCollectedGestures.size();
        }
    }

    public int getGesture(int index) {
        synchronized (mCollectedGestures) {
            return mCollectedGestures.get(index);
        }
    }

    /** Waits for {@link #onGesture(AccessibilityGestureEvent)} to collect next gesture. */
    public void waitUntilGestureInfo() {
        synchronized (mCollectedGestureEvents) {
            //Assume the size of mCollectedGestures is changed before mCollectedGestureEvents.
            if (mCollectedGestureEvents.size() > 0) {
                return;
            }
            try {
                mCollectedGestureEvents.wait(GESTURE_RECOGNIZE_TIMEOUT_MS);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    public int getGestureInfoSize() {
        synchronized (mCollectedGestureEvents) {
            return mCollectedGestureEvents.size();
        }
    }

    public AccessibilityGestureEvent getGestureInfo(int index) {
        synchronized (mCollectedGestureEvents) {
            return mCollectedGestureEvents.get(index);
        }
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        synchronized (mLock) {
            switch (event.getEventType()) {
                case AccessibilityEvent.TYPE_TOUCH_INTERACTION_END:
                    mCollectedEvents.add(event.getEventType());
                    mLock.notifyAll();
                    break;
                case AccessibilityEvent.TYPE_TOUCH_INTERACTION_START:
                case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START:
                case AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END:
                    mCollectedEvents.add(event.getEventType());
            }
        }
        super.onAccessibilityEvent(event);
    }

    public void clearEvents() {
        synchronized (mLock) {
            mCollectedEvents.clear();
        }
    }

    public int getEventsSize() {
        synchronized (mLock) {
            return mCollectedEvents.size();
        }
    }

    public int getEvent(int index) {
        synchronized (mLock) {
            return mCollectedEvents.get(index);
        }
    }

    /** Wait for onAccessibilityEvent() to collect next gesture. */
    public void waitUntilEvent(int count) {
        synchronized (mLock) {
            if (mCollectedEvents.size() >= count) {
                return;
            }
            try {
                mLock.wait(EVENT_RECOGNIZE_TIMEOUT_MS);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    /** Ensure that the specified accessibility gesture event has been received. */
    public void assertGestureReceived(int gestureId, int displayId) {
        // Wait for gesture recognizer, and check recognized gesture.
        waitUntilGestureInfo();
        if(displayId == Display.DEFAULT_DISPLAY) {
            assertEquals(1, getGesturesSize());
            assertEquals(gestureId, getGesture(0));
        }
        assertEquals(1, getGestureInfoSize());
        AccessibilityGestureEvent expectedGestureEvent =
                new AccessibilityGestureEvent(gestureId, displayId);
        AccessibilityGestureEvent actualGestureEvent = getGestureInfo(0);
        if (!expectedGestureEvent.toString().equals(actualGestureEvent.toString())) {
            fail("Unexpected gesture received, "
                    + "Received " + actualGestureEvent + ", Expected " + expectedGestureEvent);
        }
    }

    /** Insure that the specified accessibility events have been received. */
    public void assertPropagated(int... events) {
        waitUntilEvent(events.length);
        // Set up readable error reporting.
        List<String> received = new ArrayList<>();
        List<String> expected = new ArrayList<>();
        for (int event : events) {
            expected.add(AccessibilityEvent.eventTypeToString(event));
        }
        for (int i = 0; i < getEventsSize(); ++i) {
            received.add(AccessibilityEvent.eventTypeToString(getEvent(i)));
        }

        if (events.length != getEventsSize()) {
            String message =
                    String.format(
                            "Received %d events when expecting %d. Received %s, expected %s",
                            received.size(),
                            expected.size(),
                            received.toString(),
                            expected.toString());
            fail(message);
        }
        else if (!expected.equals(received)) {
            String message =
                    String.format(
                            "Received %s, expected %s",
                            received.toString(),
                            expected.toString());
            fail(message);
        }
    }
}
