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

import static android.os.UserManager.DISALLOW_ADD_USER;
import static android.os.UserManager.SWITCHABILITY_STATUS_OK;

import android.annotation.IntDef;
import android.app.Activity;
import android.app.ActivityManager;
import android.car.Car;
import android.car.user.CarUserManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.settings.R;
import com.android.car.settings.common.BaseFragment;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.ErrorDialog;
import com.android.internal.util.UserIcons;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Displays a GridLayout with icons for the users in the system to allow switching between users.
 * One of the uses of this is for the lock screen in auto.
 */
public class UserGridRecyclerView extends RecyclerView {

    private static final String MAX_USERS_LIMIT_REACHED_DIALOG_TAG =
            "com.android.car.settings.users.MaxUsersLimitReachedDialog";
    private static final String CONFIRM_CREATE_NEW_USER_DIALOG_TAG =
            "com.android.car.settings.users.ConfirmCreateNewUserDialog";

    private UserAdapter mAdapter;
    private UserManager mUserManager;
    private Context mContext;
    private BaseFragment mBaseFragment;
    private AddNewUserTask mAddNewUserTask;
    private boolean mEnableAddUserButton;
    private UserIconProvider mUserIconProvider;
    private Car mCar;
    private CarUserManager mCarUserManager;

    private final BroadcastReceiver mUserUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            onUsersUpdate();
        }
    };

    public UserGridRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mUserManager = UserManager.get(mContext);
        mUserIconProvider = new UserIconProvider();
        mEnableAddUserButton = true;
        mCar = Car.createCar(mContext);
        mCarUserManager = (CarUserManager) mCar.getCarManager(Car.CAR_USER_SERVICE);

        addItemDecoration(new ItemSpacingDecoration(context.getResources().getDimensionPixelSize(
                R.dimen.user_switcher_vertical_spacing_between_users)));
    }

    /**
     * Register listener for any update to the users
     */
    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        registerForUserEvents();
    }

    /**
     * Unregisters listener checking for any change to the users
     */
    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        unregisterForUserEvents();
        if (mAddNewUserTask != null) {
            mAddNewUserTask.cancel(/* mayInterruptIfRunning= */ false);
        }
        if (mCar != null) {
            mCar.disconnect();
        }
    }

    /**
     * Initializes the adapter that populates the grid layout
     */
    public void buildAdapter() {
        List<UserRecord> userRecords = createUserRecords(getUsersForUserGrid());
        mAdapter = new UserAdapter(mContext, userRecords);
        super.setAdapter(mAdapter);
    }

    private List<UserRecord> createUserRecords(List<UserInfo> userInfoList) {
        int fgUserId = ActivityManager.getCurrentUser();
        UserHandle fgUserHandle = UserHandle.of(fgUserId);
        List<UserRecord> userRecords = new ArrayList<>();

        // If the foreground user CANNOT switch to other users, only display the foreground user.
        if (mUserManager.getUserSwitchability(fgUserHandle) != SWITCHABILITY_STATUS_OK) {
            userRecords.add(createForegroundUserRecord());
            return userRecords;
        }

        // If the foreground user CAN switch to other users, iterate through all users.
        for (UserInfo userInfo : userInfoList) {
            boolean isForeground = fgUserId == userInfo.id;

            if (!isForeground && userInfo.isGuest()) {
                // Don't display temporary running background guests in the switcher.
                continue;
            }

            UserRecord record = new UserRecord(userInfo,
                    isForeground ? UserRecord.FOREGROUND_USER : UserRecord.BACKGROUND_USER);
            userRecords.add(record);
        }

        // Add start guest user record if the system is not logged in as guest already.
        if (!getCurrentForegroundUserInfo().isGuest()) {
            userRecords.add(createStartGuestUserRecord());
        }

        // Add "add user" record if the foreground user can add users
        if (!mUserManager.hasUserRestriction(DISALLOW_ADD_USER, fgUserHandle)) {
            userRecords.add(createAddUserRecord());
        }

        return userRecords;
    }

    private UserRecord createForegroundUserRecord() {
        return new UserRecord(getCurrentForegroundUserInfo(), UserRecord.FOREGROUND_USER);
    }

    private UserInfo getCurrentForegroundUserInfo() {
        return mUserManager.getUserInfo(ActivityManager.getCurrentUser());
    }

    /**
     * Show the "Add User" Button
     */
    public void enableAddUser() {
        mEnableAddUserButton = true;
        onUsersUpdate();
    }

    /**
     * Hide the "Add User" Button
     */
    public void disableAddUser() {
        mEnableAddUserButton = false;
        onUsersUpdate();
    }

    /**
     * Create guest user record
     */
    private UserRecord createStartGuestUserRecord() {
        return new UserRecord(/* userInfo= */ null, UserRecord.START_GUEST);
    }

    /**
     * Create add user record
     */
    private UserRecord createAddUserRecord() {
        return new UserRecord(/* userInfo= */ null, UserRecord.ADD_USER);
    }

    public void setFragment(BaseFragment fragment) {
        mBaseFragment = fragment;
    }

    private void onUsersUpdate() {
        // If you can show the add user button, there is no restriction
        mAdapter.setAddUserRestricted(!mEnableAddUserButton);
        mAdapter.clearUsers();
        mAdapter.updateUsers(createUserRecords(getUsersForUserGrid()));
        mAdapter.notifyDataSetChanged();
    }

    private List<UserInfo> getUsersForUserGrid() {
        List<UserInfo> users = UserManager.get(mContext).getUsers(/* excludeDying= */ true);
        return users.stream()
                .filter(UserInfo::supportsSwitchToByUser)
                .collect(Collectors.toList());
    }

    private void registerForUserEvents() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_USER_REMOVED);
        filter.addAction(Intent.ACTION_USER_ADDED);
        filter.addAction(Intent.ACTION_USER_INFO_CHANGED);
        filter.addAction(Intent.ACTION_USER_SWITCHED);
        filter.addAction(Intent.ACTION_USER_STOPPED);
        filter.addAction(Intent.ACTION_USER_UNLOCKED);
        mContext.registerReceiverAsUser(
                mUserUpdateReceiver,
                UserHandle.ALL,
                filter,
                /* broadcastPermission= */ null,
                /* scheduler= */ null);
    }

    private void unregisterForUserEvents() {
        mContext.unregisterReceiver(mUserUpdateReceiver);
    }

    /**
     * Adapter to populate the grid layout with the available user profiles
     */
    public final class UserAdapter extends RecyclerView.Adapter<UserAdapter.UserAdapterViewHolder>
            implements AddNewUserTask.AddNewUserListener {

        private final Resources mRes;
        private final String mGuestName;

        private Context mContext;
        private List<UserRecord> mUsers;
        private String mNewUserName;
        // View that holds the add user button.  Used to enable/disable the view
        private View mAddUserView;
        private float mOpacityDisabled;
        private float mOpacityEnabled;
        private boolean mIsAddUserRestricted;

        private final ConfirmationDialogFragment.ConfirmListener mConfirmListener = arguments -> {
            mAddNewUserTask = new AddNewUserTask(mContext,
                    mCarUserManager, /* addNewUserListener= */this);
            mAddNewUserTask.execute(mNewUserName);
        };

        /**
         * Enable the "add user" button if the user cancels adding an user
         */
        private final ConfirmationDialogFragment.RejectListener mRejectListener =
                arguments -> enableAddView();


        public UserAdapter(Context context, List<UserRecord> users) {
            mRes = context.getResources();
            mContext = context;
            updateUsers(users);
            mGuestName = mRes.getString(R.string.user_guest);
            mNewUserName = mRes.getString(R.string.user_new_user_name);
            mOpacityDisabled = mRes.getFloat(R.dimen.opacity_disabled);
            mOpacityEnabled = mRes.getFloat(R.dimen.opacity_enabled);
            resetDialogListeners();
        }

        /**
         * Removes all the users from the User Grid.
         */
        public void clearUsers() {
            mUsers.clear();
        }

        /**
         * Refreshes the User Grid with the new List of users.
         */
        public void updateUsers(List<UserRecord> users) {
            mUsers = users;
        }

        @Override
        public UserAdapterViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(mContext)
                    .inflate(R.layout.user_switcher_pod, parent, false);
            view.setAlpha(mOpacityEnabled);
            view.bringToFront();
            return new UserAdapterViewHolder(view);
        }

        @Override
        public void onBindViewHolder(UserAdapterViewHolder holder, int position) {
            UserRecord userRecord = mUsers.get(position);
            RoundedBitmapDrawable circleIcon = getCircularUserRecordIcon(userRecord);
            holder.mUserAvatarImageView.setImageDrawable(circleIcon);
            holder.mUserNameTextView.setText(getUserRecordName(userRecord));

            // Defaults to 100% opacity and no circle around the icon.
            holder.mView.setAlpha(mOpacityEnabled);
            holder.mFrame.setBackgroundResource(0);

            // Foreground user record.
            switch (userRecord.mType) {
                case UserRecord.FOREGROUND_USER:
                    // Add a circle around the icon.
                    holder.mFrame.setBackgroundResource(R.drawable.user_avatar_bg_circle);
                    // Go back to quick settings if user selected is already the foreground user.
                    holder.mView.setOnClickListener(v
                            -> mBaseFragment.getActivity().onBackPressed());
                    break;

                case UserRecord.START_GUEST:
                    holder.mView.setOnClickListener(v -> handleGuestSessionClicked());
                    break;

                case UserRecord.ADD_USER:
                    if (mIsAddUserRestricted) {
                        // If there are restrictions, show a 50% opaque "add user" view
                        holder.mView.setAlpha(mOpacityDisabled);
                        holder.mView.setOnClickListener(
                                v -> mBaseFragment.getFragmentHost().showBlockingMessage());
                    } else {
                        holder.mView.setOnClickListener(v -> handleAddUserClicked(v));
                    }
                    break;

                default:
                    // User record;
                    holder.mView.setOnClickListener(v -> handleUserSwitch(userRecord.mInfo));
            }
        }

        /**
         * Specify if adding a user should be restricted.
         *
         * @param isAddUserRestricted should adding a user be restricted
         */
        public void setAddUserRestricted(boolean isAddUserRestricted) {
            mIsAddUserRestricted = isAddUserRestricted;
        }

        /** Resets listeners for shown dialog fragments. */
        private void resetDialogListeners() {
            if (mBaseFragment != null) {
                ConfirmationDialogFragment dialog =
                        (ConfirmationDialogFragment) mBaseFragment
                                .getFragmentManager()
                                .findFragmentByTag(CONFIRM_CREATE_NEW_USER_DIALOG_TAG);
                ConfirmationDialogFragment.resetListeners(
                        dialog,
                        mConfirmListener,
                        mRejectListener,
                        /* neutralListener= */ null);
            }
        }

        private void handleUserSwitch(UserInfo userInfo) {
            mCarUserManager.switchUser(userInfo.id).thenRun(() -> {
                // Successful switch, close Settings app.
                closeSettingsTask();
            });
        }

        private void handleGuestSessionClicked() {
            UserInfo guest =
                    UserHelper.getInstance(mContext).createNewOrFindExistingGuest(mContext);
            if (guest != null) {
                mCarUserManager.switchUser(guest.id).thenRun(() -> {
                    // Successful start, will switch to guest now. Close Settings app.
                    closeSettingsTask();
                });
            }
        }

        private void handleAddUserClicked(View addUserView) {
            if (!mUserManager.canAddMoreUsers()) {
                showMaxUsersLimitReachedDialog();
            } else {
                mAddUserView = addUserView;
                // Disable button so it cannot be clicked multiple times
                mAddUserView.setEnabled(false);
                showConfirmCreateNewUserDialog();
            }
        }

        private void showMaxUsersLimitReachedDialog() {
            ConfirmationDialogFragment dialogFragment =
                    UsersDialogProvider.getMaxUsersLimitReachedDialogFragment(getContext(),
                            UserHelper.getInstance(mContext).getMaxSupportedRealUsers());
            dialogFragment.show(
                    mBaseFragment.getFragmentManager(), MAX_USERS_LIMIT_REACHED_DIALOG_TAG);
        }

        private void showConfirmCreateNewUserDialog() {
            ConfirmationDialogFragment dialogFragment =
                    UsersDialogProvider.getConfirmCreateNewUserDialogFragment(getContext(),
                            mConfirmListener, mRejectListener);
            dialogFragment.show(
                    mBaseFragment.getFragmentManager(), CONFIRM_CREATE_NEW_USER_DIALOG_TAG);
        }

        private RoundedBitmapDrawable getCircularUserRecordIcon(UserRecord userRecord) {
            Resources resources = mContext.getResources();
            RoundedBitmapDrawable circleIcon;
            switch (userRecord.mType) {
                case UserRecord.START_GUEST:
                    circleIcon = mUserIconProvider.getRoundedGuestDefaultIcon(resources);
                    break;
                case UserRecord.ADD_USER:
                    circleIcon = getCircularAddUserIcon();
                    break;
                default:
                    circleIcon = mUserIconProvider.getRoundedUserIcon(userRecord.mInfo, mContext);
            }
            return circleIcon;
        }

        private RoundedBitmapDrawable getCircularAddUserIcon() {
            RoundedBitmapDrawable circleIcon =
                    RoundedBitmapDrawableFactory.create(mRes, UserIcons.convertToBitmap(
                            mContext.getDrawable(R.drawable.user_add_circle)));
            circleIcon.setCircular(true);
            return circleIcon;
        }

        private String getUserRecordName(UserRecord userRecord) {
            String recordName;
            switch (userRecord.mType) {
                case UserRecord.START_GUEST:
                    recordName = mContext.getString(R.string.start_guest_session);
                    break;
                case UserRecord.ADD_USER:
                    recordName = mContext.getString(R.string.user_add_user_menu);
                    break;
                default:
                    recordName = userRecord.mInfo.name;
            }
            return recordName;
        }

        @Override
        public void onUserAddedSuccess() {
            enableAddView();
            // New user added. Will switch to new user, therefore close the app.
            closeSettingsTask();
        }

        @Override
        public void onUserAddedFailure() {
            enableAddView();
            // Display failure dialog.
            if (mBaseFragment != null) {
                ErrorDialog.show(mBaseFragment, R.string.add_user_error_title);
            }
        }

        /**
         * When we switch users, we also want to finish the QuickSettingActivity, so we send back a
         * result telling the QuickSettingActivity to finish.
         */
        private void closeSettingsTask() {
            mBaseFragment.getActivity().setResult(Activity.FINISH_TASK_WITH_ACTIVITY, new Intent());
            mBaseFragment.getActivity().finish();
        }

        @Override
        public int getItemCount() {
            return mUsers.size();
        }

        /**
         * Layout for each individual pod in the Grid RecyclerView
         */
        public class UserAdapterViewHolder extends RecyclerView.ViewHolder {

            public ImageView mUserAvatarImageView;
            public TextView mUserNameTextView;
            public View mView;
            public FrameLayout mFrame;

            public UserAdapterViewHolder(View view) {
                super(view);
                mView = view;
                mUserAvatarImageView = view.findViewById(R.id.user_avatar);
                mUserNameTextView = view.findViewById(R.id.user_name);
                mFrame = view.findViewById(R.id.current_user_frame);
            }
        }

        private void enableAddView() {
            if (mAddUserView != null) {
                mAddUserView.setEnabled(true);
            }
        }
    }

    /**
     * Object wrapper class for the userInfo.  Use it to distinguish if a profile is a
     * guest profile, add user profile, or the foreground user.
     */
    public static final class UserRecord {

        public final UserInfo mInfo;
        public final @UserRecordType int mType;

        public static final int START_GUEST = 0;
        public static final int ADD_USER = 1;
        public static final int FOREGROUND_USER = 2;
        public static final int BACKGROUND_USER = 3;

        @IntDef({START_GUEST, ADD_USER, FOREGROUND_USER, BACKGROUND_USER})
        @Retention(RetentionPolicy.SOURCE)
        public @interface UserRecordType {}

        public UserRecord(@Nullable UserInfo userInfo, @UserRecordType int recordType) {
            mInfo = userInfo;
            mType = recordType;
        }
    }

    /**
     * A {@link RecyclerView.ItemDecoration} that will add spacing between each item in the
     * RecyclerView that it is added to.
     */
    private static class ItemSpacingDecoration extends RecyclerView.ItemDecoration {
        private int mItemSpacing;

        private ItemSpacingDecoration(int itemSpacing) {
            mItemSpacing = itemSpacing;
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent,
                RecyclerView.State state) {
            super.getItemOffsets(outRect, view, parent, state);
            int position = parent.getChildAdapterPosition(view);

            // Skip offset for last item except for GridLayoutManager.
            if (position == state.getItemCount() - 1
                    && !(parent.getLayoutManager() instanceof GridLayoutManager)) {
                return;
            }

            outRect.bottom = mItemSpacing;
        }
    }
}
