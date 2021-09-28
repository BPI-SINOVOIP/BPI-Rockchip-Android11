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
package android.car.apitest;

import static android.car.test.util.UserTestingHelper.clearUserLockCredentials;
import static android.car.test.util.UserTestingHelper.setMaxSupportedUsers;
import static android.car.test.util.UserTestingHelper.setUserLockCredentials;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STARTING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKING;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.app.ActivityManager;
import android.app.IActivityManager;
import android.car.Car;
import android.car.testapi.BlockingUserLifecycleListener;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.car.user.UserSwitchResult;
import android.content.pm.UserInfo;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.test.filters.FlakyTest;

import com.android.internal.infra.AndroidFuture;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public final class CarUserManagerTest extends CarApiTestBase {

    private static final String TAG = CarUserManagerTest.class.getSimpleName();

    private static final int PIN = 2345;

    private static final int SWITCH_TIMEOUT_MS = 70_000;
    private static final int STOP_TIMEOUT_MS = 300_000;

    /**
     * Stopping the user takes a while, even when calling force stop - change it to false if this
     * test becomes flaky.
     */
    private static final boolean TEST_STOP = false;

    private static final UserManager sUserManager = UserManager.get(sContext);

    private static final int sMaxNumberUsersBefore = UserManager.getMaxSupportedUsers();

    private static final List<Integer> sCreatedUsers = new ArrayList<>();

    private static int sInitialUserId = UserHandle.USER_NULL;

    private CarUserManager mCarUserManager;

    @BeforeClass
    public static void setupUsers() {
        sInitialUserId = ActivityManager.getCurrentUser();
        Log.i(TAG, "Running test as user " + sInitialUserId);
        setMaxSupportedUsers(8); // Total 6 users will be created for all tests
    }

    @AfterClass
    public static void cleanupUsers() {
        setMaxSupportedUsers(sMaxNumberUsersBefore);
        switchUserDirectly(sInitialUserId);

        for (int i = 0; i < sCreatedUsers.size(); i++) {
            int id = sCreatedUsers.get(i);
            Log.d(TAG, "removeCreatedUsers: " + id);
            if (!sUserManager.removeUser(id)) {
                Log.wtf(TAG, "Failed to remove user " + id);
            }
        }
    }

    @Before
    public void setManager() throws Exception {
        mCarUserManager = (CarUserManager) getCar().getCarManager(Car.CAR_USER_SERVICE);
    }

    @Test
    public void testLifecycleListener() throws Exception {
        int oldUserId = ActivityManager.getCurrentUser();
        int newUserId = createNewUser("Test", /* isGuest= */ false).id;

        BlockingUserLifecycleListener startListener = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forUser(newUserId)
                .setTimeout(SWITCH_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();

        Log.d(TAG, "registering start listener: " + startListener);
        AtomicBoolean executedRef = new AtomicBoolean();

        Executor mExecutor = (r) -> {
            executedRef.set(true);
            r.run();
        };
        mCarUserManager.addListener(mExecutor, startListener);

        // Switch while listener is registered
        switchUser(newUserId);

        List<UserLifecycleEvent> startEvents  = startListener.waitForEvents();
        Log.d(TAG, "Received start events: " + startEvents);

        // Make sure listener callback was executed in the proper threaqd
        assertWithMessage("not executed on executor").that(executedRef.get()).isTrue();

        // Assert user ids
        List<UserLifecycleEvent> expectedEvents = startListener.waitForEvents();
        Log.d(TAG, "Received expected events: " + expectedEvents);
        for (UserLifecycleEvent event : expectedEvents) {
            assertWithMessage("wrong userId on %s", event)
                .that(event.getUserId()).isEqualTo(newUserId);
            assertWithMessage("wrong userHandle on %s", event)
                .that(event.getUserHandle().getIdentifier()).isEqualTo(newUserId);
            if (event.getEventType() == USER_LIFECYCLE_EVENT_TYPE_SWITCHING) {
                assertWithMessage("wrong previousUserId on %s", event)
                    .that(event.getPreviousUserId()).isEqualTo(oldUserId);
                assertWithMessage("wrong previousUserHandle on %s", event)
                    .that(event.getPreviousUserHandle().getIdentifier()).isEqualTo(oldUserId);
            }
        }

        Log.d(TAG, "unregistering start listener: " + startListener);
        mCarUserManager.removeListener(startListener);

        BlockingUserLifecycleListener stopListener = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forUser(newUserId)
                .setTimeout(STOP_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED)
                .build();

        Log.d(TAG, "registering stop listener: " + stopListener);
        mCarUserManager.addListener(mExecutor, stopListener);

        // Switch back to the initial user
        switchUser(sInitialUserId);

        if (TEST_STOP) {
            // Must force stop the user, otherwise it can take minutes for its process to finish
            forceStopUser(newUserId);

            List<UserLifecycleEvent> stopEvents = stopListener.waitForEvents();
            Log.d(TAG, "stopEvents: " + stopEvents + "; all events on stop listener: "
                    + stopListener.getAllReceivedEvents());

            // Assert user ids
            for (UserLifecycleEvent event : stopEvents) {
                assertWithMessage("wrong userId on %s", event)
                    .that(event.getUserId()).isEqualTo(newUserId);
                assertWithMessage("wrong userHandle on %s", event)
                    .that(event.getUserHandle().getIdentifier()).isEqualTo(newUserId);
            }
        } else {
            Log.w(TAG, "NOT testing user stop events");
        }

        // Make sure unregistered listener didn't receive any more events
        List<UserLifecycleEvent> allStartEvents = startListener.getAllReceivedEvents();
        Log.d(TAG, "All start events: " + startEvents);
        assertThat(allStartEvents).containsAllIn(startEvents).inOrder();

        Log.d(TAG, "unregistering stop listener: " + stopListener);
        mCarUserManager.removeListener(stopListener);
    }

    /**
     * Tests resume behavior when current user is ephemeral guest, a new guest user should be
     * created and switched to.
     */
    @Test
    @FlakyTest // TODO(b/158050171) remove once process is stable on user switching.
    public void testGuestUserResumeToNewGuestUser() throws Exception {
        // Create new guest user
        UserInfo guestUser = createNewUser("Guest", /* isGuestUser= */ true);
        int guestUserId = guestUser.id;

        // Wait for this user to be active
        BlockingUserLifecycleListener listener1 = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forUser(guestUserId)
                .setTimeout(SWITCH_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        mCarUserManager.addListener(Runnable::run, listener1);
        switchUser(guestUserId);
        listener1.waitForEvents();
        mCarUserManager.removeListener(listener1);

        BlockingUserLifecycleListener listener2 = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forPreviousUser(guestUserId)
                .setTimeout(SWITCH_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING)
                .build();
        // Make sure listener callback was executed in the proper threaqd
        mCarUserManager.addListener(Runnable::run, listener2);

        // Emulate suspend to RAM
        suspendToRamAndResume();
        UserLifecycleEvent event = listener2.waitForEvents().get(0);

        int newGuestId = event.getUserId();
        sCreatedUsers.add(newGuestId);

        assertWithMessage("wrong user on event %s", event).that(newGuestId)
                .isNotEqualTo(guestUserId);
        assertWithMessage("wrong current user").that(newGuestId)
                .isEqualTo(ActivityManager.getCurrentUser());
        UserInfo newGuest = sUserManager.getUserInfo(newGuestId);
        assertWithMessage("new user (%s) is not a guest", newGuest.toFullString())
                .that(newGuest.isGuest()).isTrue();
        assertWithMessage("wrong name on new guest(%s)", newGuest.toFullString())
                .that(newGuest.name).isNotEqualTo(guestUser.name);
        mCarUserManager.removeListener(listener2);
    }

    /**
     * Tests resume behavior when current user is guest guest but with secured lock screen,
     * resume to same guest user.
     */
    @Test
    @FlakyTest // TODO(b/158050171) remove once process is stable on user switching.
    public void testSecuredGuestUserResumeToSameUser() throws Exception {
        // Create new guest user
        UserInfo guestUser = createNewUser("Guest", /* isGuestUser= */ true);
        int guestUserId = guestUser.id;

        // Wait for this user to be active
        BlockingUserLifecycleListener listener = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forUser(guestUserId)
                .setTimeout(SWITCH_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        mCarUserManager.addListener(Runnable::run, listener);

        switchUser(guestUserId);

        listener.waitForEvents();
        mCarUserManager.removeListener(listener);

        setUserLockCredentials(guestUserId, PIN);
        try {
            // Emulate suspend to RAM
            suspendToRamAndResume();

            assertWithMessage("not resumed to previous user: %s", guestUser)
                    .that(ActivityManager.getCurrentUser()).isEqualTo(guestUserId);
        } finally {
            clearUserLockCredentials(guestUserId, PIN);
        }

    }

    /**
     * Tests resume behavior when current user is persistent user.
     */
    @Test
    @FlakyTest // TODO(b/158050171) remove once process is stable on user switching.
    public void testPersistentUserResumeToUser() throws Exception {
        int newUserId = createNewUser("Test", /* isGuest= */ false).id;
        BlockingUserLifecycleListener listener = BlockingUserLifecycleListener
                .forSpecificEvents()
                .forUser(newUserId)
                .setTimeout(SWITCH_TIMEOUT_MS)
                .addExpectedEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED)
                .build();
        mCarUserManager.addListener(Runnable::run, listener);
        switchUser(newUserId);
        listener.waitForEvents();

        // Emulate suspend to RAM
        suspendToRamAndResume();

        listener.waitForEvents();
        assertWithMessage("not resumed to previous user: %s", newUserId)
                .that(ActivityManager.getCurrentUser()).isEqualTo(newUserId);

        mCarUserManager.removeListener(listener);
    }

    @NonNull
    private static UserInfo createNewUser(String name, boolean isGuest) {
        name = "CarUserManagerTest." + name;
        Log.i(TAG, "Creating new user " + name);
        UserInfo newUser = isGuest ? sUserManager.createGuest(sContext, name)
                : sUserManager.createUser(name, /* flags= */ 0);
        sCreatedUsers.add(newUser.id);
        Log.i(TAG, "Created new user: " + newUser.toFullString());
        return newUser;
    }

    private void switchUser(@UserIdInt int userId) throws Exception {
        Log.i(TAG, "Switching to user " + userId + " using CarUserManager");

        AndroidFuture<UserSwitchResult> future = mCarUserManager.switchUser(userId);
        UserSwitchResult result = future.get(SWITCH_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Log.d(TAG, "Result: " + result);

        // TODO(b/155326051): use result.isSuccess()
        if (result.getStatus() != UserSwitchResult.STATUS_SUCCESSFUL) {
            fail("Could not switch to user " + userId + ": " + result);
        }
    }

    // TODO: ideally should use switchUser(), but that requires CarUserManager, which is not static.
    private static void switchUserDirectly(@UserIdInt int userId) {
        ActivityManager am = sContext.getSystemService(ActivityManager.class);
        int currentUserId = am.getCurrentUser();
        Log.i(TAG, "Switching to user " + userId + " using AM, when current is " + currentUserId);

        if (currentUserId == userId) {
            Log.v(TAG, "current user is already " + userId);
            return;
        }
        if (!am.switchUser(userId)) {
            fail("Could not switch to user " + userId + " using ActivityManager (current user is "
                    + currentUserId + ")");
        }
    }

    private static void forceStopUser(@UserIdInt int userId) throws RemoteException {
        Log.i(TAG, "Force-stopping user " + userId);
        IActivityManager am = ActivityManager.getService();
        am.stopUser(userId, /* force=*/ true, /* listener= */ null);
    }
}
