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

package com.android.car.testapi;

import static android.car.test.mocks.JavaMockitoHelper.await;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STARTING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKING;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.car.testapi.BlockingUserLifecycleListener;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.car.user.CarUserManager.UserLifecycleEventType;
import android.os.UserHandle;
import android.util.Log;

import org.junit.Test;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.stream.Collectors;

public final class BlockingUserLifecycleListenerTest {

    private static final String TAG = BlockingUserLifecycleListenerTest.class.getSimpleName();
    private static final long TIMEOUT_MS = 500;

    @Test
    public void testListener_forAnyEvent_invalidBuilderMethods() throws Exception {
        BlockingUserLifecycleListener.Builder builder = BlockingUserLifecycleListener.forAnyEvent()
                .setTimeout(666);

        assertThrows(IllegalStateException.class, () -> builder.forUser(10));
        assertThrows(IllegalStateException.class, () -> builder.forPreviousUser(10));
        assertThrows(IllegalStateException.class,
                () -> builder.addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED));
    }

    @Test
    public void testForAnyEvent_invalidMethods() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forAnyEvent()
                .build();

        assertThrows(IllegalStateException.class, () -> listener.waitForEvents());
        assertThrows(IllegalStateException.class, () -> listener.getAllReceivedEvents());
    }

    @Test
    public void testForAnyEvent_timesout() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forAnyEvent()
                .build();

        assertThrows(IllegalStateException.class, () -> listener.waitForAnyEvent());
    }

    @Test
    public void testForAnyEvent_ok() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forAnyEvent()
                .build();
        sendAsyncEvents(listener, 10, USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);

        UserLifecycleEvent event = listener.waitForAnyEvent();
        assertEvent(event, 10, USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);
    }

    @Test
    public void testForSpecificEvents_invalidMethods() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forSpecificEvents()
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING).build();

        assertThrows(IllegalStateException.class, () -> listener.waitForAnyEvent());
    }

    @Test
    public void testForSpecificEvents_receivedOnlyExpected() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forSpecificEvents()
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();

        sendAsyncEvents(listener, 10,
                USER_LIFECYCLE_EVENT_TYPE_STARTING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertEvents(events, 10,
                USER_LIFECYCLE_EVENT_TYPE_STARTING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);

        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertThat(allReceivedEvents).containsAllIn(events).inOrder();
    }

    @Test
    public void testForSpecificEvents_receivedExtraEvents() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forSpecificEvents()
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKING)
                .build();

        CountDownLatch latch = sendAsyncEvents(listener, 10,
                USER_LIFECYCLE_EVENT_TYPE_STARTING,
                USER_LIFECYCLE_EVENT_TYPE_SWITCHING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertEvents(events, 10,
                USER_LIFECYCLE_EVENT_TYPE_STARTING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKING);

        await(latch, TIMEOUT_MS);
        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertEvents(allReceivedEvents, 10,
                USER_LIFECYCLE_EVENT_TYPE_STARTING,
                USER_LIFECYCLE_EVENT_TYPE_SWITCHING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKING,
                USER_LIFECYCLE_EVENT_TYPE_UNLOCKED);
    }

    @Test
    public void testForSpecificEvents_filterByUser() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forSpecificEvents()
                .forUser(10)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        UserLifecycleEvent wrong1 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING, 11);
        UserLifecycleEvent wrong2 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED, 11);
        UserLifecycleEvent right1 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING, 10);
        UserLifecycleEvent right2 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED, 10);

        CountDownLatch latch = sendAsyncEvents(listener,
                Arrays.asList(wrong1, right1, right2, wrong2));

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertThat(events).containsExactly(right1, right2).inOrder();

        await(latch, TIMEOUT_MS);
        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertThat(allReceivedEvents)
                .containsExactly(wrong1, right1, right2, wrong2)
                .inOrder();
    }

    @Test
    public void testForSpecificEvents_filterByUserDuplicatedEventTypes() throws Exception {
        BlockingUserLifecycleListener listener =  BlockingUserLifecycleListener.forSpecificEvents()
                .forUser(10)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .build();
        UserLifecycleEvent wrong1 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING, 11);
        UserLifecycleEvent wrong2 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED, 11);
        UserLifecycleEvent wrong3 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING, 11);
        UserLifecycleEvent right1 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING, 10);
        UserLifecycleEvent right2 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED, 10);
        UserLifecycleEvent right3 = new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING, 10);

        CountDownLatch latch = sendAsyncEvents(listener,
                Arrays.asList(wrong1, right1, wrong2, right2, right3, wrong3));

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertThat(events).containsExactly(right1, right2, right3).inOrder();

        await(latch, TIMEOUT_MS);
        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertThat(allReceivedEvents)
                .containsExactly(wrong1, right1, wrong2, right2, right3, wrong3)
                .inOrder();
    }

    @Test
    public void testForSpecificEvents_filterByPreviousUser() throws Exception {
        BlockingUserLifecycleListener listener = BlockingUserLifecycleListener.forSpecificEvents()
                .forPreviousUser(10)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        UserLifecycleEvent wrong1 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING, 11);
        UserLifecycleEvent wrong2 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED, 10);
        UserLifecycleEvent wrong3 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 11, 10);
        UserLifecycleEvent right1 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 10, 11);
        UserLifecycleEvent right2 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED, 10, 12);

        CountDownLatch latch = sendAsyncEvents(listener,
                Arrays.asList(wrong1, right1, wrong2, right2, wrong3));

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertThat(events).containsExactly(right1, right2).inOrder();

        await(latch, TIMEOUT_MS);
        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertThat(allReceivedEvents)
                .containsExactly(wrong1, right1, wrong2, right2, wrong3)
                .inOrder();
    }

    @Test
    public void testForSpecificEvents_filterByPreviousAndTargetUsers() throws Exception {
        BlockingUserLifecycleListener listener = BlockingUserLifecycleListener.forSpecificEvents()
                .forPreviousUser(10)
                .forUser(11)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        UserLifecycleEvent wrong1 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING, 11);
        UserLifecycleEvent wrong2 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED, 10);
        UserLifecycleEvent wrong3 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 10, 12);
        UserLifecycleEvent right1 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING, 10, 11);
        UserLifecycleEvent right2 =
                new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED, 10, 11);

        CountDownLatch latch = sendAsyncEvents(listener,
                Arrays.asList(wrong1, right1, wrong2, right2, wrong3));

        List<UserLifecycleEvent> events = listener.waitForEvents();
        assertThat(events).containsExactly(right1, right2).inOrder();

        await(latch, TIMEOUT_MS);
        List<UserLifecycleEvent> allReceivedEvents = listener.getAllReceivedEvents();
        assertThat(allReceivedEvents)
                .containsExactly(wrong1, right1, wrong2, right2, wrong3)
                .inOrder();
    }

    @NonNull
    private static CountDownLatch sendAsyncEvents(@NonNull BlockingUserLifecycleListener listener,
            @UserIdInt int userId, @UserLifecycleEventType int... eventTypes) {
        List<UserLifecycleEvent> events = Arrays.stream(eventTypes)
                .mapToObj((type) -> new UserLifecycleEvent(type, userId))
                .collect(Collectors.toList());
        return sendAsyncEvents(listener, events);
    }

    @NonNull
    private static CountDownLatch sendAsyncEvents(@NonNull BlockingUserLifecycleListener listener,
            @NonNull List<UserLifecycleEvent> events) {
        Log.d(TAG, "sendAsyncEvents(" + events + "): called on thread " + Thread.currentThread());
        CountDownLatch latch = new CountDownLatch(1);
        new Thread(() -> {
            Log.d(TAG, "sending " + events.size() + " on thread " + Thread.currentThread());
            events.forEach((e) -> listener.onEvent(e));
            Log.d(TAG, "sent");
            latch.countDown();
        }, "AsyncEventsThread").start();
        return latch;
    }

    private static void assertEvent(UserLifecycleEvent event, @UserIdInt int expectedUserId,
            @UserLifecycleEventType int expectedType) {
        assertThat(event).isNotNull();
        assertWithMessage("wrong type on %s; expected %s", event,
                CarUserManager.lifecycleEventTypeToString(expectedType)).that(event.getEventType())
                        .isEqualTo(expectedType);
        assertThat(event.getUserId()).isEqualTo(expectedUserId);
        assertThat(event.getUserHandle().getIdentifier()).isEqualTo(expectedUserId);
        assertThat(event.getPreviousUserId()).isEqualTo(UserHandle.USER_NULL);
        assertThat(event.getPreviousUserHandle()).isNull();
    }

    private static void assertEvents(List<UserLifecycleEvent> events, @UserIdInt int expectedUserId,
            @UserLifecycleEventType int... expectedTypes) {
        assertThat(events).isNotNull();
        assertThat(events).hasSize(expectedTypes.length);
        for (int i = 0; i < expectedTypes.length; i++) {
            int expectedType = expectedTypes[i];
            UserLifecycleEvent event = events.get(i);
            assertEvent(event, expectedUserId, expectedType);
        }
    }
}
