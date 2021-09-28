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
 * limitations under the License
 */

package com.android.car.settings.users;

import android.car.Car;
import android.car.drivingstate.CarUxRestrictions;
import android.car.user.CarUserManager;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.UserManager;
import android.provider.Settings;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.ErrorDialog;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ProgressBarController;
import com.android.internal.annotations.VisibleForTesting;
import com.android.settingslib.search.SearchIndexable;

import java.util.Collections;
import java.util.List;

/**
 * Lists all Users available on this device.
 */
@SearchIndexable
public class UsersListFragment extends SettingsFragment implements
        AddNewUserTask.AddNewUserListener {
    private static final String FACTORY_RESET_PACKAGE_NAME = "android";
    private static final String FACTORY_RESET_REASON = "ExitRetailModeConfirmed";
    @VisibleForTesting
    static final String CONFIRM_EXIT_RETAIL_MODE_DIALOG_TAG =
            "com.android.car.settings.users.ConfirmExitRetailModeDialog";
    @VisibleForTesting
    static final String CONFIRM_CREATE_NEW_USER_DIALOG_TAG =
            "com.android.car.settings.users.ConfirmCreateNewUserDialog";
    @VisibleForTesting
    static final String MAX_USERS_LIMIT_REACHED_DIALOG_TAG =
            "com.android.car.settings.users.MaxUsersLimitReachedDialog";

    private Car mCar;
    private CarUserManager mCarUserManager;
    private UserManager mUserManager;

    private ProgressBarController mProgressBar;
    private MenuItem mAddUserButton;

    private AsyncTask mAddNewUserTask;
    /** Indicates that a task is running. */
    private boolean mIsBusy;

    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mConfirmCreateNewUserListener = arguments -> {
        mAddNewUserTask = new AddNewUserTask(getContext(),
                mCarUserManager, /* addNewUserListener= */ this).execute(
                getContext().getString(R.string.user_new_user_name));
        mIsBusy = true;
        updateUi();
    };

    /**
     * Will perform a factory reset. Copied from
     * {@link com.android.settings.MasterClearConfirm#doMasterClear()}
     */
    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mConfirmExitRetailModeListener = arguments -> {
        Intent intent = new Intent(Intent.ACTION_FACTORY_RESET);
        intent.setPackage(FACTORY_RESET_PACKAGE_NAME);
        intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        intent.putExtra(Intent.EXTRA_REASON, FACTORY_RESET_REASON);
        intent.putExtra(Intent.EXTRA_WIPE_EXTERNAL_STORAGE, true);
        intent.putExtra(Intent.EXTRA_WIPE_ESIMS, true);
        getActivity().sendBroadcast(intent);
        // Intent handling is asynchronous -- assume it will happen soon.
    };

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mAddUserButton);
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.users_list_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mCar = Car.createCar(context);
        mCarUserManager = (CarUserManager) mCar.getCarManager(Car.CAR_USER_SERVICE);
        mUserManager = UserManager.get(getContext());
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) findDialogByTag(CONFIRM_CREATE_NEW_USER_DIALOG_TAG),
                mConfirmCreateNewUserListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);
        ConfirmationDialogFragment.resetListeners(
                (ConfirmationDialogFragment) findDialogByTag(CONFIRM_EXIT_RETAIL_MODE_DIALOG_TAG),
                mConfirmExitRetailModeListener,
                /* rejectListener= */ null,
                /* neutralListener= */ null);

        mAddUserButton = new MenuItem.Builder(getContext())
                .setOnClickListener(i -> handleAddUserClicked())
                .setTitle(mUserManager.isDemoUser()
                        ? R.string.exit_retail_button_text : R.string.user_add_user_menu)
                .setVisible(mUserManager.isDemoUser()
                        || canCurrentProcessAddUsers())
                .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP)
                .build();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mProgressBar = getToolbar().getProgressBar();
    }

    @Override
    public void onStart() {
        super.onStart();
        updateUi();
    }

    @Override
    public void onStop() {
        super.onStop();
        mProgressBar.setVisible(false);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mAddNewUserTask != null) {
            mAddNewUserTask.cancel(/* mayInterruptIfRunning= */ false);
        }
        if (mCar != null) {
            mCar.disconnect();
        }
    }

    @Override
    public void onUserAddedSuccess() {
        mIsBusy = false;
        updateUi();
    }

    @Override
    public void onUserAddedFailure() {
        mIsBusy = false;
        updateUi();
        // Display failure dialog.
        ErrorDialog.show(this, R.string.add_user_error_title);
    }

    @VisibleForTesting
    void setCarUserManager(CarUserManager carUserManager) {
        mCarUserManager = carUserManager;
    }

    private void updateUi() {
        mAddUserButton.setEnabled(!mIsBusy);
        mProgressBar.setVisible(mIsBusy);
    }

    private void handleAddUserClicked() {
        // If the user is a demo user, show a dialog asking if they want to exit retail/demo mode.
        if (mUserManager.isDemoUser()) {
            ConfirmationDialogFragment dialogFragment =
                    UsersDialogProvider.getConfirmExitRetailModeDialogFragment(getContext(),
                            mConfirmExitRetailModeListener, null);

            dialogFragment.show(getFragmentManager(), CONFIRM_EXIT_RETAIL_MODE_DIALOG_TAG);
            return;
        }

        // If no more users can be added because the maximum allowed number is reached, let the user
        // know.
        if (!mUserManager.canAddMoreUsers()) {
            ConfirmationDialogFragment dialogFragment =
                    UsersDialogProvider.getMaxUsersLimitReachedDialogFragment(getContext(),
                            UserHelper.getInstance(getContext()).getMaxSupportedRealUsers());

            dialogFragment.show(getFragmentManager(), MAX_USERS_LIMIT_REACHED_DIALOG_TAG);
            return;
        }

        // Only add the add user button if the current user is allowed to add a user.
        if (canCurrentProcessAddUsers()) {
            ConfirmationDialogFragment dialogFragment =
                    UsersDialogProvider.getConfirmCreateNewUserDialogFragment(getContext(),
                            mConfirmCreateNewUserListener, null);

            dialogFragment.show(getFragmentManager(), CONFIRM_CREATE_NEW_USER_DIALOG_TAG);
        }
    }

    private boolean canCurrentProcessAddUsers() {
        return !mUserManager.hasUserRestriction(UserManager.DISALLOW_ADD_USER);
    }

    /**
     * Data provider for Settings Search.
     */
    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.users_list_fragment,
                    Settings.ACTION_USER_SETTINGS);
}
