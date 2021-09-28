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

import android.car.user.CarUserManager;
import android.car.user.UserCreationResult;
import android.car.userlib.UserHelper;
import android.content.Context;
import android.content.pm.UserInfo;
import android.os.AsyncTask;

import com.android.car.settings.common.Logger;
import com.android.internal.infra.AndroidFuture;

import java.util.concurrent.ExecutionException;

/**
 * Task to add a new user to the device
 */
public class AddNewUserTask extends AsyncTask<String, Void, UserInfo> {
    private static final Logger LOG = new Logger(AddNewUserTask.class);

    private final Context mContext;
    private final CarUserManager mCarUserManager;
    private final AddNewUserListener mAddNewUserListener;

    public AddNewUserTask(Context context, CarUserManager carUserManager,
            AddNewUserListener addNewUserListener) {
        mContext = context;
        mCarUserManager = carUserManager;
        mAddNewUserListener = addNewUserListener;
    }

    @Override
    protected UserInfo doInBackground(String... userNames) {
        AndroidFuture<UserCreationResult> future = mCarUserManager.createUser(userNames[0],
                /* flags= */ 0);
        try {
            UserCreationResult result = future.get();
            if (result.isSuccess()) {
                UserInfo user = result.getUser();
                if (user != null) {
                    UserHelper.setDefaultNonAdminRestrictions(mContext, user, /* enable= */ true);
                    UserHelper.assignDefaultIcon(mContext, user);
                } else {
                    LOG.wtf("Inconsistent state: successful future with null user - "
                            + result.toString());
                }
                return user;
            }
        } catch (InterruptedException | ExecutionException e) {
            if (e instanceof InterruptedException) {
                Thread.currentThread().interrupt();
            }
            LOG.e("Error creating new user: ", e);
        }
        return null;
    }

    @Override
    protected void onPreExecute() { }

    @Override
    protected void onPostExecute(UserInfo user) {
        if (user != null) {
            mAddNewUserListener.onUserAddedSuccess();
            mCarUserManager.switchUser(user.id);
        } else {
            mAddNewUserListener.onUserAddedFailure();
        }
    }

    /**
     * Interface for getting notified when AddNewUserTask has been completed.
     */
    public interface AddNewUserListener {
        /**
         * Invoked in AddNewUserTask.onPostExecute after the user has been created successfully.
         */
        void onUserAddedSuccess();

        /**
         * Invoked in AddNewUserTask.onPostExecute if new user creation failed.
         */
        void onUserAddedFailure();
    }
}
