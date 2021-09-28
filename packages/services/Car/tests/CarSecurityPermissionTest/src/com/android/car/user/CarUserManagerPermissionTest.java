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

package com.android.car.user;

import static android.Manifest.permission.CREATE_USERS;
import static android.Manifest.permission.INTERACT_ACROSS_USERS;
import static android.Manifest.permission.INTERACT_ACROSS_USERS_FULL;
import static android.Manifest.permission.MANAGE_USERS;
import static android.car.Car.CAR_USER_SERVICE;
import static android.car.Car.createCar;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_1;

import static com.android.compatibility.common.util.ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.expectThrows;

import android.app.Instrumentation;
import android.car.Car;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.content.Context;
import android.os.Handler;
import android.os.UserManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Objects;

/**
 * This class contains security permission tests for the {@link CarUserManager}'s system APIs.
 */
@RunWith(AndroidJUnit4.class)
public final class CarUserManagerPermissionTest {
    private static final int USRE_TYPE = 1;

    private CarUserManager mCarUserManager;
    private Context mContext;
    private Instrumentation mInstrumentation;

    @Before
    public void setUp() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        Car car = Objects.requireNonNull(createCar(mContext, (Handler) null));
        mCarUserManager = (CarUserManager) car.getCarManager(CAR_USER_SERVICE);

    }

    @Test
    public void testSwitchUserPermission() throws Exception {
        Exception e = expectThrows(SecurityException.class, () -> mCarUserManager.switchUser(100));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
    }

    @Test
    public void testCreateUserPermission() throws Exception {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.createUser(null, UserManager.USER_TYPE_FULL_SECONDARY, 0));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
        assertThat(e.getMessage()).contains(CREATE_USERS);
    }

    @Test
    public void testRemoveUserPermission() throws Exception {
        Exception e = expectThrows(SecurityException.class, () -> mCarUserManager.removeUser(100));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
    }

    @Test
    public void testAddListenerPermission() {
        UserLifecycleListener listener = (e) -> { };

        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.addListener(Runnable::run, listener));

        assertThat(e.getMessage()).contains(INTERACT_ACROSS_USERS);
        assertThat(e.getMessage()).contains(INTERACT_ACROSS_USERS_FULL);
    }

    @Test
    public void testRemoveListenerPermission() throws Exception {
        UserLifecycleListener listener = (e) -> { };
        invokeMethodWithShellPermissionsNoReturn(mCarUserManager,
                (um) -> um.addListener(Runnable::run, listener));

        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.removeListener(listener));

        assertThat(e.getMessage()).contains(INTERACT_ACROSS_USERS);
        assertThat(e.getMessage()).contains(INTERACT_ACROSS_USERS_FULL);
    }

    @Test
    public void testGetUserIdentificationAssociationPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.getUserIdentificationAssociation(CUSTOM_1));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
    }

    @Test
    public void testSetUserIdentificationAssociationPermission() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.setUserIdentificationAssociation(
                        new int[] {CUSTOM_1}, new int[] {42}));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
    }

    @Test
    public void testSetUserSwitchUiCallback() {
        Exception e = expectThrows(SecurityException.class,
                () -> mCarUserManager.getUserIdentificationAssociation(CUSTOM_1));
        assertThat(e.getMessage()).contains(MANAGE_USERS);
    }
}
