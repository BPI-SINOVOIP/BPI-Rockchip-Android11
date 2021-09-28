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

package android.car.testapi;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.UserIdInt;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.car.user.CarUserManager.UserLifecycleEventType;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.Preconditions;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

/**
 * {@link UserLifecycleListener} that blocks until the proper events are received.
 *
 * <p>It can be used in 2 "modes":
 *
 * <ul>
 *   <li>{@link #forAnyEvent()}: it blocks (through the {@link #waitForAnyEvent()} call) until any
 *   any event is received. It doesn't allow any customization (other than
 *   {@link Builder#setTimeout(long)}).
 *   <li>{@link #forSpecificEvents()}: it blocks (through the {@link #waitForEvents()} call) until
 *   all events specified by the {@link Builder} are received.
 * </ul>
 */
public final class BlockingUserLifecycleListener implements UserLifecycleListener {

    private static final String TAG = BlockingUserLifecycleListener.class.getSimpleName();

    private static final long DEFAULT_TIMEOUT_MS = 2_000;

    private final Object mLock = new Object();

    private final CountDownLatch mLatch = new CountDownLatch(1);

    @GuardedBy("mLock")
    private final List<UserLifecycleEvent> mAllReceivedEvents = new ArrayList<>();
    @GuardedBy("mLock")
    private final List<UserLifecycleEvent> mExpectedEventsReceived = new ArrayList<>();

    @UserLifecycleEventType
    private final List<Integer> mExpectedEventTypes;

    @UserLifecycleEventType
    private final List<Integer> mExpectedEventTypesLeft;

    @UserIdInt
    @Nullable
    private final Integer mForUserId;

    @UserIdInt
    @Nullable
    private final Integer mForPreviousUserId;

    private final long mTimeoutMs;

    private BlockingUserLifecycleListener(Builder builder) {
        mExpectedEventTypes = Collections
                .unmodifiableList(new ArrayList<>(builder.mExpectedEventTypes));
        mExpectedEventTypesLeft = builder.mExpectedEventTypes;
        mTimeoutMs = builder.mTimeoutMs;
        mForUserId = builder.mForUserId;
        mForPreviousUserId = builder.mForPreviousUserId;
        Log.d(TAG, "constructor: " + this);
    }

    /**
     * Creates a builder for tests that need to wait for an arbitrary event.
     */
    @NonNull
    public static Builder forAnyEvent() {
        return new Builder(/* forAnyEvent= */ true);
    }

    /**
     * Creates a builder for tests that need to wait for specific events.
     */
    @NonNull
    public static Builder forSpecificEvents() {
        return new Builder(/* forAnyEvent= */ false);
    }

    /**
     * Builder for a customized {@link BlockingUserLifecycleListener} instance.
     */
    public static final class Builder {
        private long mTimeoutMs = DEFAULT_TIMEOUT_MS;
        private final boolean mForAnyEvent;

        private Builder(boolean forAnyEvent) {
            mForAnyEvent = forAnyEvent;
        }

        @UserLifecycleEventType
        private final List<Integer> mExpectedEventTypes = new ArrayList<>();

        @UserIdInt
        @Nullable
        private Integer mForUserId;

        @UserIdInt
        @Nullable
        private Integer mForPreviousUserId;

        /**
         * Sets the timeout.
         */
        public Builder setTimeout(long timeoutMs) {
            mTimeoutMs = timeoutMs;
            return this;
        }

        /**
         * Sets the expected type - once the given event is received, the listener will unblock.
         *
         * @throws IllegalStateException if builder is {@link #forAnyEvent}.
         * @throws IllegalArgumentException if the expected type was already added.
         */
        public Builder addExpectedEvent(@UserLifecycleEventType int eventType) {
            assertNotForAnyEvent();
            mExpectedEventTypes.add(eventType);
            return this;
        }

        /**
         * Filters received events just for the given user.
         */
        public Builder forUser(@UserIdInt int userId) {
            assertNotForAnyEvent();
            mForUserId = userId;
            return this;
        }

        /**
         * Filters received events just for the given previous user.
         */
        public Builder forPreviousUser(@UserIdInt int userId) {
            assertNotForAnyEvent();
            mForPreviousUserId = userId;
            return this;
        }

        /**
         * Builds a new instance.
         */
        @NonNull
        public BlockingUserLifecycleListener build() {
            return new BlockingUserLifecycleListener(Builder.this);
        }

        private void assertNotForAnyEvent() {
            Preconditions.checkState(!mForAnyEvent, "not allowed forAnyEvent()");
        }
    }

    @Override
    public void onEvent(UserLifecycleEvent event) {
        synchronized (mLock) {
            Log.d(TAG, "onEvent(): expecting=" + mExpectedEventTypesLeft + ", received=" + event);

            mAllReceivedEvents.add(event);

            if (expectingSpecificUser() && event.getUserId() != mForUserId) {
                Log.w(TAG, "ignoring event for different user (expecting " + mForUserId + ")");
                return;
            }

            if (expectingSpecificPreviousUser()
                    && event.getPreviousUserId() != mForPreviousUserId) {
                Log.w(TAG, "ignoring event for different previous user (expecting "
                        + mForPreviousUserId + ")");
                return;
            }

            Integer actualType = event.getEventType();
            boolean removed = mExpectedEventTypesLeft.remove(actualType);
            if (removed) {
                Log.v(TAG, "event removed; still expecting for "
                        + toString(mExpectedEventTypesLeft));
                mExpectedEventsReceived.add(event);
            } else {
                Log.v(TAG, "event not removed");
            }

            if (mExpectedEventTypesLeft.isEmpty() && mLatch.getCount() == 1) {
                Log.d(TAG, "all expected events received, counting down " + mLatch);
                mLatch.countDown();
            }
        }
    }

    /**
     * Blocks until any event is received, and returns it.
     *
     * @throws IllegalStateException if listener was built using {@link #forSpecificEvents()}.
     * @throws IllegalStateException if it times out before any event is received.
     * @throws InterruptedException if interrupted before any event is received.
     */
    @Nullable
    public UserLifecycleEvent waitForAnyEvent() throws InterruptedException {
        Preconditions.checkState(isForAnyEvent(),
                "cannot call waitForEvent() when built with expected events");
        waitForExpectedEvents();

        UserLifecycleEvent event;
        synchronized (mLock) {
            event = mAllReceivedEvents.isEmpty() ? null : mAllReceivedEvents.get(0);
            Log.v(TAG, "waitForAnyEvent(): returning " + event);
        }
        return event;
    }

    /**
     * Blocks until the events specified in the {@link Builder} are received, and returns them.
     *
     * @throws IllegalStateException if listener was built without any call to
     * {@link Builder#addExpectedEvent(int)} or using {@link #forAnyEvent().
     * @throws IllegalStateException if it times out before all specified events are received.
     * @throws InterruptedException if interrupted before all specified events are received.
     */
    @NonNull
    public List<UserLifecycleEvent> waitForEvents() throws InterruptedException {
        Preconditions.checkState(!isForAnyEvent(),
                "cannot call waitForEvents() when built without specific expected events");
        waitForExpectedEvents();
        List<UserLifecycleEvent> events;
        synchronized (mLock) {
            events = mExpectedEventsReceived;
        }
        Log.v(TAG, "waitForEvents(): returning " + events);
        return events;
    }

    /**
     * Gets a list with all received events until now.
     */
    @NonNull
    public List<UserLifecycleEvent> getAllReceivedEvents() {
        Preconditions.checkState(!isForAnyEvent(),
                "cannot call getAllReceivedEvents() when built without specific expected events");
        synchronized (mLock) {
            return Collections.unmodifiableList(new ArrayList<>(mAllReceivedEvents));
        }
    }

    @Override
    public String toString() {
        return "[" + getClass().getSimpleName() + ": " + stateToString() + "]";
    }

    @NonNull
    private String stateToString() {
        synchronized (mLock) {
            return "timeout=" + mTimeoutMs + "ms"
                    + ",expectedEventTypes=" + toString(mExpectedEventTypes)
                    + ",expectedEventTypesLeft=" + toString(mExpectedEventTypesLeft)
                    + (expectingSpecificUser() ? ",forUser=" + mForUserId : "")
                    + (expectingSpecificPreviousUser() ? ",forPrevUser=" + mForPreviousUserId : "")
                    + ",received=" + mAllReceivedEvents
                    + ",waiting=" + mExpectedEventTypesLeft;
        }
    }

    private void waitForExpectedEvents() throws InterruptedException {
        if (!mLatch.await(mTimeoutMs, TimeUnit.MILLISECONDS)) {
            String errorMessage = "did not receive all expected events (" + stateToString() + ")";
            Log.e(TAG, errorMessage);
            throw new IllegalStateException(errorMessage);
        }
    }

    @NonNull
    private static String toString(@NonNull List<Integer> eventTypes) {
        return eventTypes.stream()
                .map((i) -> CarUserManager.lifecycleEventTypeToString(i))
                .collect(Collectors.toList())
                .toString();
    }

    private boolean isForAnyEvent() {
        return mExpectedEventTypes.isEmpty();
    }

    private boolean expectingSpecificUser() {
        return mForUserId != null;
    }

    private boolean expectingSpecificPreviousUser() {
        return mForPreviousUserId != null;
    }
}
