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
import android.content.Intent;
import android.content.pm.UserInfo;
import android.os.Bundle;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.ErrorDialog;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/** Common logic shared for controlling the action bar which contains a button to delete a user. */
public abstract class UserDetailsBaseFragment extends SettingsFragment {
    private UserManager mUserManager;
    private UserInfo mUserInfo;
    private MenuItem mDeleteButton;

    private final ConfirmationDialogFragment.ConfirmListener mConfirmListener = arguments -> {
        String userType = arguments.getString(UsersDialogProvider.KEY_USER_TYPE);
        if (userType.equals(UsersDialogProvider.LAST_ADMIN)) {
            launchFragment(ChooseNewAdminFragment.newInstance(mUserInfo));
        } else {
            Context context = getContext();
            if (UserHelper.getInstance(context).removeUser(context, mUserInfo)) {
                getActivity().onBackPressed();
            } else {
                // If failed, need to show error dialog for users.
                ErrorDialog.show(this, R.string.delete_user_error_title);
            }
        }
    };

    /** Adds user id to fragment arguments. */
    protected static UserDetailsBaseFragment addUserIdToFragmentArguments(
            UserDetailsBaseFragment fragment, int userId) {
        Bundle bundle = new Bundle();
        bundle.putInt(Intent.EXTRA_USER_ID, userId);
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mDeleteButton);
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        int userId = getArguments().getInt(Intent.EXTRA_USER_ID);
        mUserManager = UserManager.get(getContext());
        mUserInfo = UserUtils.getUserInfo(getContext(), userId);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ConfirmationDialogFragment dialogFragment =
                (ConfirmationDialogFragment) findDialogByTag(ConfirmationDialogFragment.TAG);
        ConfirmationDialogFragment.resetListeners(
                dialogFragment,
                mConfirmListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);

        // If the current user is not allowed to remove users, the user trying to be removed
        // cannot be removed, or the current user is a demo user, do not show delete button.
        boolean isVisible = !mUserManager.hasUserRestriction(UserManager.DISALLOW_REMOVE_USER)
                && mUserInfo.id != UserHandle.USER_SYSTEM
                && !mUserManager.isDemoUser();
        mDeleteButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.delete_button)
                .setOnClickListener(i -> showConfirmRemoveUserDialog())
                .setVisible(isVisible)
                .build();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        getToolbar().setTitle(getTitleText());
    }

    /** Make UserInfo available to subclasses. */
    protected UserInfo getUserInfo() {
        return mUserInfo;
    }

    /** Refresh UserInfo in case it becomes invalid. */
    protected void refreshUserInfo() {
        mUserInfo = UserUtils.getUserInfo(getContext(), mUserInfo.id);
    }

    /** Defines the text that should be shown in the action bar. */
    protected abstract String getTitleText();

    private void showConfirmRemoveUserDialog() {
        UserHelper userHelper = UserHelper.getInstance(getContext());
        boolean isLastUser = userHelper.getAllPersistentUsers().size() == 1;
        boolean isLastAdmin = mUserInfo.isAdmin()
                && userHelper.getAllAdminUsers().size() == 1;

        ConfirmationDialogFragment dialogFragment;

        if (isLastUser) {
            dialogFragment = UsersDialogProvider.getConfirmRemoveLastUserDialogFragment(
                    getContext(), mConfirmListener, /* rejectListener= */ null);
        } else if (isLastAdmin) {
            dialogFragment = UsersDialogProvider.getConfirmRemoveLastAdminDialogFragment(
                    getContext(), mConfirmListener, /* rejectListener= */ null);
        } else {
            dialogFragment = UsersDialogProvider.getConfirmRemoveUserDialogFragment(getContext(),
                    mConfirmListener, /* rejectListener= */ null);
        }

        dialogFragment.show(getFragmentManager(), ConfirmationDialogFragment.TAG);
    }
}
