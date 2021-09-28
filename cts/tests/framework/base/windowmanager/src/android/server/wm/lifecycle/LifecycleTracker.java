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
 * limitations under the License
 */

package android.server.wm.lifecycle;

import static org.junit.Assert.fail;

import android.app.Activity;
import android.server.wm.lifecycle.LifecycleLog.ActivityCallback;
import android.util.Pair;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Gets notified about activity lifecycle updates and provides blocking mechanism to wait until
 * expected activity states are reached.
 */
public class LifecycleTracker implements LifecycleLog.LifecycleTrackerCallback {

    private LifecycleLog mLifecycleLog;

    LifecycleTracker(LifecycleLog lifecycleLog) {
        mLifecycleLog = lifecycleLog;
        mLifecycleLog.setLifecycleTracker(this);
    }

    void waitAndAssertActivityStates(
            Pair<Class<? extends Activity>, ActivityCallback>[] activityCallbacks) {
        final boolean waitResult = waitForConditionWithTimeout(
                () -> pendingCallbacks(activityCallbacks).isEmpty(), 5 * 1000);

        if (!waitResult) {
            fail("Expected lifecycle states not achieved: " + pendingCallbacks(activityCallbacks));
        }
    }

    /**
     * Waits for a specific sequence of events to happen.
     * When there is a possibility of some lifecycle state happening more than once in a sequence,
     * it is better to use this method instead of {@link #waitAndAssertActivityStates(Pair[])}.
     * Otherwise we might stop tracking too early.
     */
    void waitForActivityTransitions(Class<? extends Activity> activityClass,
            List<ActivityCallback> expectedTransitions) {
        waitForConditionWithTimeout(
                () -> mLifecycleLog.getActivityLog(activityClass).equals(expectedTransitions),
                5 * 1000);
    }

    @Override
    synchronized public void onActivityLifecycleChanged() {
        notify();
    }

    /** Get a list of activity states that were not reached yet. */
    private List<Pair<Class<? extends Activity>, ActivityCallback>> pendingCallbacks(
            Pair<Class<? extends Activity>, ActivityCallback>[] activityCallbacks) {
        final List<Pair<Class<? extends Activity>, ActivityCallback>> notReachedActivityCallbacks =
                new ArrayList<>();

        for (Pair<Class<? extends Activity>, ActivityCallback> callbackPair : activityCallbacks) {
            final Class<? extends Activity> activityClass = callbackPair.first;
            final List<ActivityCallback> transitionList =
                    mLifecycleLog.getActivityLog(activityClass);
            if (transitionList.isEmpty()
                    || transitionList.get(transitionList.size() - 1) != callbackPair.second) {
                // The activity either hasn't got any state transitions yet or the current state is
                // not the one we expect.
                notReachedActivityCallbacks.add(callbackPair);
            }
        }
        return notReachedActivityCallbacks;
    }

    /** Blocking call to wait for a condition to become true with max timeout. */
    synchronized private boolean waitForConditionWithTimeout(BooleanSupplier waitCondition,
            long timeoutMs) {
        final long timeout = System.currentTimeMillis() + timeoutMs;
        while (!waitCondition.getAsBoolean()) {
            final long waitMs = timeout - System.currentTimeMillis();
            if (waitMs <= 0) {
                // Timeout expired.
                return false;
            }
            try {
                wait(timeoutMs);
            } catch (InterruptedException e) {
                // Weird, let's retry.
            }
        }
        return true;
    }
}
