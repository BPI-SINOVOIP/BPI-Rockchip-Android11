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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.internal.annotations.VisibleForTesting;

/**
 * Shows details for a user with the ability to remove user and edit current user.
 */
public class UserDetailsFragment extends UserDetailsBaseFragment {

    private boolean mIsStarted;

    /** Creates instance of UserDetailsFragment. */
    public static UserDetailsFragment newInstance(int userId) {
        return (UserDetailsFragment) UserDetailsBaseFragment.addUserIdToFragmentArguments(
                new UserDetailsFragment(), userId);
    }

    @VisibleForTesting
    final BroadcastReceiver mUserUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            // Update the user info value, as it may have changed.
            refreshUserInfo();
            if (mIsStarted) {
                // Update the text in the action bar when there is a user update.
                getToolbar().setTitle(getTitleText());
            }
        }
    };

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.user_details_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        use(EditUserNameEntryPreferenceController.class,
                R.string.pk_edit_user_name_entry).setUserInfo(getUserInfo());
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        registerForUserEvents();
    }

    @Override
    public void onStart() {
        super.onStart();
        mIsStarted = true;
        getToolbar().setTitle(getTitleText());
    }

    @Override
    public void onStop() {
        mIsStarted = false;
        super.onStop();
    }

    @Override
    public void onDestroy() {
        unregisterForUserEvents();
        super.onDestroy();
    }

    @Override
    protected String getTitleText() {
        return UserUtils.getUserDisplayName(getContext(), getUserInfo());
    }

    private void registerForUserEvents() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_USER_INFO_CHANGED);
        getContext().registerReceiver(mUserUpdateReceiver, filter);
    }

    private void unregisterForUserEvents() {
        getContext().unregisterReceiver(mUserUpdateReceiver);
    }
}
