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
 * limitations under the License.
 */

package com.android.car.settings.users;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.user.CarUserManager;
import android.car.user.UserCreationResult;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.os.UserManager;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import com.android.internal.infra.AndroidFuture;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class AddNewUserTaskTest {
    @Mock
    private UserManager mUserManager;
    @Mock
    private CarUserManager mCarUserManager;
    @Mock
    private AddNewUserTask.AddNewUserListener mAddNewUserListener;
    @Mock
    private Context mContext;

    private AddNewUserTask mTask;

    private final Resources mResources = InstrumentationRegistry.getTargetContext().getResources();

    @Before
    public void createAsyncTask() {
        MockitoAnnotations.initMocks(this);
        mTask = new AddNewUserTask(mContext, mCarUserManager, mAddNewUserListener);
    }

    @Test
    public void testTaskCallsCreateNewUser() {
        String newUserName = "Test name";
        UserInfo newUser = new UserInfo(10, newUserName, /* flags= */ 0);

        mockCreateUser(newUser, UserCreationResult.STATUS_SUCCESSFUL);

        mTask.execute(newUserName);
        Robolectric.flushBackgroundThreadScheduler();

        verify(mCarUserManager).createUser(newUserName, /* flags= */ 0);
    }

    @Test
    public void testSwitchToNewUserIfUserCreated() {
        String newUserName = "Test name";
        UserInfo newUser = new UserInfo(10, newUserName, /* flags= */ 0);

        mockCreateUser(newUser, UserCreationResult.STATUS_SUCCESSFUL);

        mTask.execute(newUserName);
        Robolectric.flushBackgroundThreadScheduler();

        verify(mCarUserManager).switchUser(newUser.id);
    }

    @Test
    public void testOnUserAddedSuccessCalledIfUserCreated() {
        String newUserName = "Test name";
        UserInfo newUser = new UserInfo(10, newUserName, /* flags= */ 0);

        mockCreateUser(newUser, UserCreationResult.STATUS_SUCCESSFUL);

        mTask.execute(newUserName);
        Robolectric.flushBackgroundThreadScheduler();

        verify(mAddNewUserListener).onUserAddedSuccess();
    }

    @Test
    public void testOnUserAddedFailureCalledIfNullReturned() {
        String newUserName = "Test name";

        mockCreateUser(/* user= */ null, UserCreationResult.STATUS_ANDROID_FAILURE);

        mTask.execute(newUserName);
        Robolectric.flushBackgroundThreadScheduler();

        verify(mAddNewUserListener).onUserAddedFailure();
    }

    private void mockCreateUser(@Nullable UserInfo user, int status) {
        when(mContext.getSystemService(Context.USER_SERVICE)).thenReturn(mUserManager);
        when(mContext.getResources()).thenReturn(mResources);

        AndroidFuture<UserCreationResult> future = new AndroidFuture<>();
        future.complete(new UserCreationResult(status,
                user, /* errorMessage= */ null));
        when(mCarUserManager.createUser(anyString(), anyInt())).thenReturn(future);
    }
}
