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

import android.content.Context;
import android.content.pm.UserInfo;

import androidx.preference.Preference;

import com.android.car.settings.R;
import com.android.car.ui.preference.CarUiPreference;

import java.util.ArrayList;
import java.util.List;

/**
 * Constructs the preferences to be displayed in {@link UsersListFragment} and
 * {@link ChooseNewAdminFragment}.
 */
public class UsersPreferenceProvider {

    /**
     * Interface for registering clicks on users.
     */
    public interface UserClickListener {
        /**
         * Invoked when user is clicked.
         *
         * @param userInfo User for which the click is registered.
         */
        void onUserClicked(UserInfo userInfo);
    }

    private final Context mContext;
    private final UserClickListener mUserPreferenceClickListener;
    private boolean mIncludeCurrentUser;
    private boolean mIncludeGuest;

    public UsersPreferenceProvider(Context context, UserClickListener listener) {
        mContext = context;
        mUserPreferenceClickListener = listener;
        mIncludeCurrentUser = true;
        mIncludeGuest = true;
    }

    /**
     * Sets whether createUserList should include an entry for the current user. Default is
     * {@code true}.
     */
    public void setIncludeCurrentUser(boolean includeCurrentUser) {
        mIncludeCurrentUser = includeCurrentUser;
    }

    /**
     * Sets whether createUserList should include an entry for the guest profile. Default is
     * {@code true}.
     */
    public void setIncludeGuest(boolean includeGuest) {
        mIncludeGuest = includeGuest;
    }

    /**
     * Creates the list of users (as preferences). The first user will be the current user (if
     * requested) and the last user will be the guest user (if requested). Otherwise, the list is
     * populated with all of the remaining switchable users.
     */
    public List<Preference> createUserList() {
        List<Preference> users = new ArrayList<>();
        UserInfo currUserInfo = UserHelper.getInstance(mContext).getCurrentProcessUserInfo();

        // Show current user at the top of the list.
        if (mIncludeCurrentUser) {
            users.add(createUserPreference(currUserInfo));
        }

        // Display all users on the system, except: Guests and current user who's displayed already.
        List<UserInfo> infos = UserHelper.getInstance(mContext).getAllLivingUsers(
                userInfo -> !userInfo.isGuest() && userInfo.id != currUserInfo.id);
        for (UserInfo userInfo : infos) {
            users.add(createUserPreference(userInfo));
        }

        // Display guest session option.
        if (mIncludeGuest) {
            users.add(createGuestUserPreference());
        }

        return users;
    }

    private Preference createUserPreference(UserInfo userInfo) {
        CarUiPreference preference = new CarUiPreference(mContext);
        preference.setIcon(
                new UserIconProvider().getRoundedUserIcon(userInfo, mContext));
        preference.setTitle(UserUtils.getUserDisplayName(mContext, userInfo));

        if (!userInfo.isInitialized()) {
            preference.setSummary(R.string.user_summary_not_set_up);
        }
        if (userInfo.isAdmin()) {
            preference.setSummary(
                    isCurrentUser(userInfo) ? R.string.signed_in_admin_user : R.string.user_admin);
        }

        preference.setOnPreferenceClickListener(pref -> {
            if (mUserPreferenceClickListener == null) {
                return false;
            }
            mUserPreferenceClickListener.onUserClicked(userInfo);
            return true;
        });

        return preference;
    }

    private Preference createGuestUserPreference() {
        CarUiPreference preference = new CarUiPreference(mContext);
        preference.setIcon(
                new UserIconProvider().getRoundedGuestDefaultIcon(mContext.getResources()));
        preference.setTitle(R.string.user_guest);
        return preference;
    }

    private boolean isCurrentUser(UserInfo userInfo) {
        return UserHelper.getInstance(mContext).isCurrentProcessUser(userInfo);
    }
}
