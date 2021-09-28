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
package android.platform.helpers;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.car.Car;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.car.user.UserSwitchResult;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.SystemClock;
import android.os.UserManager;
import android.support.test.uiautomator.UiDevice;

import androidx.test.InstrumentationRegistry;

import com.android.internal.infra.AndroidFuture;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper class that is used by integration test only. It is wrapping around exiting platform APIs
 * {@link CarUserManager}, {@link UserManager} to expose functions for user switch end-to-end tests.
 */
public class MultiUserHelper {
    /** For testing purpose we allow a wide range of switching time. */
    private static final int USER_SWITCH_TIMEOUT_SECOND = 300;

    private static MultiUserHelper sMultiUserHelper;
    private CarUserManager mCarUserManager;
    private UserManager mUserManager;

    private MultiUserHelper() {
        Context context = InstrumentationRegistry.getTargetContext();
        mUserManager = UserManager.get(context);
        Car car = Car.createCar(context);
        mCarUserManager = (CarUserManager) car.getCarManager(Car.CAR_USER_SERVICE);
    }

    /**
     * It will always be used as a singleton class
     *
     * @return MultiUserHelper instance
     */
    public static MultiUserHelper getInstance() {
        if (sMultiUserHelper == null) {
            sMultiUserHelper = new MultiUserHelper();
        }
        return sMultiUserHelper;
    }

    /**
     * Creates a regular user or guest
     *
     * @param name the name of the user or guest
     * @param isGuestUser true if want to create a guest, otherwise create a regular user
     * @return User Id for newly created user
     */
    public int createUser(String name, boolean isGuestUser) throws Exception {
        if (isGuestUser) {
            return mUserManager.createUser(name, UserManager.USER_TYPE_FULL_GUEST, /* flags= */ 0)
                    .id;
        }
        return mUserManager.createUser(name, /* flags= */ 0).id;
    }

    /**
     * Switches to the target user at API level. Always waits until user switch complete. Besides,
     * it waits for an additional amount of time for switched user to become idle (stable)
     *
     * @param id user id
     * @param timeoutMs the time to wait (in msec) after user switch complete
     */
    public void switchAndWaitForStable(int id, long timeoutMs) throws Exception {
        switchToUserId(id);
        SystemClock.sleep(timeoutMs);
    }

    /**
     * Switches to the target user at API level. Always wait until user switch complete.
     *
     * <p>User switch complete only means the user ready at API level. It doesn't mean the UI is
     * completely ready for the target user. It doesn't include unlocking user data and loading car
     * launcher page
     *
     * @param id Id of the user to switch to
     */
    public void switchToUserId(int id) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        // A UserLifeCycleListener to wait for user switch event. It is equivalent to
        // UserSwitchObserver#onUserSwitchComplete callback
        // TODO(b/155434907): Should eventually wait for "user unlocked" event which is better
        UserLifecycleListener userSwitchListener =
                e -> {
                    if (e.getEventType() == CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING) {
                        latch.countDown();
                    }
                };
        mCarUserManager.addListener(Runnable::run, userSwitchListener);
        AndroidFuture<UserSwitchResult> future = mCarUserManager.switchUser(id);
        UserSwitchResult result = null;
        try {
            result = future.get(USER_SWITCH_TIMEOUT_SECOND, TimeUnit.SECONDS);
        } catch (Exception e) {
            throw new Exception(
                    String.format("Exception when switching to target user: %d", id), e);
        }

        if (!result.isSuccess()) {
            throw new Exception(String.format("User switch failed: %s", result));
        }
        // Wait for user switch complete event, which seems to happen later than UserSwitchResult.
        if (!latch.await(USER_SWITCH_TIMEOUT_SECOND, TimeUnit.SECONDS)) {
            throw new Exception(
                    String.format(
                            "Timeout while switching to user %d after %d seconds",
                            id, USER_SWITCH_TIMEOUT_SECOND));
        }
        mCarUserManager.removeListener(userSwitchListener);
    }

    /**
     * Removes the target user. For now it is a non-blocking call.
     *
     * @param userInfo info of the user to be removed
     * @return true if removed successfully
     */
    public boolean removeUser(UserInfo userInfo) {
        return mUserManager.removeUser(userInfo.id);
    }

    public UserInfo getCurrentForegroundUserInfo() {
        return mUserManager.getUserInfo(ActivityManager.getCurrentUser());
    }

    /**
     * Get default initial user
     *
     * @return user ID of initial user
     */
    public int getInitialUser() {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        try {
            String userId = device.executeShellCommand("cmd car_service get-initial-user").trim();
            return Integer.parseInt(userId);
        } catch (IOException e) {
            throw new RuntimeException("Failed to get initial user", e);
        }
    }

    /**
     * Tries to find an existing user with the given name
     *
     * @param name the name of the user
     * @return A {@link UserInfo} if the user is found, or {@code null} if not found
     */
    @Nullable
    public UserInfo getUserByName(String name) {
        return mUserManager
                .getUsers(/* excludeDying= */ true)
                .stream()
                .filter(user -> user.name.equals(name))
                .findFirst()
                .orElse(null);
    }
}
