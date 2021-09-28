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
package com.google.android.car.kitchensink.users;

import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.IActivityManager;
import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;
import android.view.Display;
import android.view.DisplayAddress;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.KitchenSinkActivity;
import com.google.android.car.kitchensink.R;

import java.util.ArrayList;
import java.util.List;

public class ProfileUserFragment extends Fragment {

    private static final String TAG = ProfileUserFragment.class.getSimpleName();

    private static final int ERROR_MESSAGE = 0;
    private static final int WARN_MESSAGE = 1;
    private static final int INFO_MESSAGE = 2;

    private SpinnerWrapper mUsersSpinner;
    private SpinnerWrapper mZonesSpinner;
    private SpinnerWrapper mDisplaysSpinner;
    private SpinnerWrapper mAppsSpinner;

    private Button mCreateRestrictedUserButton;
    private Button mCreateManagedUserButton;
    private Button mRemoveUserButton;
    private Button mStartUserButton;
    private Button mStopUserButton;
    private Button mAssignUserToZoneButton;
    private Button mLaunchAppForZoneButton;
    private Button mLaunchAppForDisplayAndUserButton;

    private TextView mUserIdText;
    private TextView mZoneInfoText;
    private TextView mUserStateText;
    private TextView mErrorMessageText;

    private UserManager mUserManager;
    private DisplayManager mDisplayManager;
    private CarOccupantZoneManager mZoneManager;


    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.profile_user, container, false);
    }

    public void onViewCreated(View view, Bundle savedInstanceState) {
        mUserManager = getContext().getSystemService(UserManager.class);
        mDisplayManager = getContext().getSystemService(DisplayManager.class);
        Car car = ((KitchenSinkActivity) getHost()).getCar();
        mZoneManager = (CarOccupantZoneManager) car.getCarManager(Car.CAR_OCCUPANT_ZONE_SERVICE);

        mUserIdText = view.findViewById(R.id.profile_textView_state);
        mZoneInfoText = view.findViewById(R.id.profile_textView_zoneinfo);
        mUserStateText = view.findViewById(R.id.profile_textView_userstate);
        updateTextInfo();

        mUsersSpinner = SpinnerWrapper.create(getContext(),
                view.findViewById(R.id.profile_spinner_users), getUsers());
        mZonesSpinner = SpinnerWrapper.create(getContext(),
                view.findViewById(R.id.profile_spinner_zones), getZones());
        mDisplaysSpinner = SpinnerWrapper.create(getContext(),
                view.findViewById(R.id.profile_spinner_displays), getDisplays());
        mAppsSpinner = SpinnerWrapper.create(getContext(),
                view.findViewById(R.id.profile_spinner_apps), getApps());

        mCreateRestrictedUserButton = view.findViewById(R.id.profile_button_create_restricted_user);
        mCreateRestrictedUserButton.setOnClickListener(v -> {
            createUser(/* restricted= */ true);
        });
        mCreateManagedUserButton = view.findViewById(R.id.profile_button_create_managed_user);
        mCreateManagedUserButton.setOnClickListener(v -> {
            createUser(/* restricted= */ false);
        });
        mRemoveUserButton = view.findViewById(R.id.profile_button_remove_user);
        mRemoveUserButton.setOnClickListener(v -> {
            removeUser();
        });
        mStartUserButton = view.findViewById(R.id.profile_button_start_user);
        mStartUserButton.setOnClickListener(v -> {
            startUser();
        });
        mStopUserButton = view.findViewById(R.id.profile_button_stop_user);
        mStopUserButton.setOnClickListener(v -> {
            stopUser();
        });
        mAssignUserToZoneButton = view.findViewById(R.id.profile_button_assign_user_to_zone);
        mAssignUserToZoneButton.setOnClickListener(v -> {
            assignUserToZone();
        });
        mLaunchAppForZoneButton = view.findViewById(R.id.profile_button_launch_app_for_zone);
        mLaunchAppForZoneButton.setOnClickListener(v -> {
            launchAppForZone();
        });
        mLaunchAppForDisplayAndUserButton = view.findViewById(
                R.id.profile_button_launch_app_for_display);
        mLaunchAppForDisplayAndUserButton.setOnClickListener(v -> {
            launchAppForDisplayAndUser();
        });

        mErrorMessageText = view.findViewById(R.id.status_message_text_view);
    }

    private void updateTextInfo() {
        int currentUserId = ActivityManager.getCurrentUser();
        int myUserId = UserHandle.myUserId();
        mUserIdText.setText("Current userId:" + currentUserId + " myUserId:" + myUserId);
        StringBuilder zoneStatebuilder = new StringBuilder();
        zoneStatebuilder.append("Zone-User-Displays:");
        List<CarOccupantZoneManager.OccupantZoneInfo> zonelist = mZoneManager.getAllOccupantZones();
        for (CarOccupantZoneManager.OccupantZoneInfo zone : zonelist) {
            zoneStatebuilder.append(zone.zoneId);
            zoneStatebuilder.append("-");
            zoneStatebuilder.append(mZoneManager.getUserForOccupant(zone));
            zoneStatebuilder.append("-");
            List<Display> displays = mZoneManager.getAllDisplaysForOccupant(zone);
            for (Display display : displays) {
                zoneStatebuilder.append(display.getDisplayId());
                zoneStatebuilder.append(",");
            }
            zoneStatebuilder.append(":");
        }
        mZoneInfoText.setText(zoneStatebuilder.toString());
        StringBuilder userStateBuilder = new StringBuilder();
        userStateBuilder.append("UserId-state;");
        int[] profileUsers = mUserManager.getEnabledProfileIds(currentUserId);
        for (int profileUser : profileUsers) {
            userStateBuilder.append(profileUser);
            userStateBuilder.append("-");
            if (mUserManager.isUserRunning(profileUser)) {
                userStateBuilder.append("R:");
            } else {
                userStateBuilder.append("S:");
            }
        }
        mUserStateText.setText(userStateBuilder.toString());
    }

    private void createUser(boolean restricted) {
        try {
            UserInfo user;
            if (restricted) {
                user = mUserManager.createRestrictedProfile("RestrictedProfile");
            } else {
                user = mUserManager.createProfileForUser("ManagedProfile",
                        UserManager.USER_TYPE_PROFILE_MANAGED, /* flags= */ 0,
                        ActivityManager.getCurrentUser());
            }
            setMessage(INFO_MESSAGE, "Created User " + user);
            mUsersSpinner.updateEntries(getUsers());
            updateTextInfo();
        } catch (Exception e) {
            setMessage(ERROR_MESSAGE, e);
        }
    }

    private void removeUser() {
        int userToRemove = getSelectedUser();
        if (userToRemove == UserHandle.USER_NULL) {
            setMessage(INFO_MESSAGE, "Cannot remove null user");
            return;
        }
        int currentUser = ActivityManager.getCurrentUser();
        if (userToRemove == currentUser) {
            setMessage(WARN_MESSAGE, "Cannot remove current user");
            return;
        }
        Log.i(TAG, "removing user:" + userToRemove);
        try {
            mUserManager.removeUser(userToRemove);
            mUsersSpinner.updateEntries(getUsers());
            updateTextInfo();
            setMessage(INFO_MESSAGE, "Removed user " + userToRemove);
        } catch (Exception e) {
            setMessage(ERROR_MESSAGE, e);
        }
    }

    private void stopUser() {
        int userToUpdate = getSelectedUser();
        if (!canChangeUser(userToUpdate)) {
            return;
        }

        if (!mUserManager.isUserRunning(userToUpdate)) {
            setMessage(WARN_MESSAGE, "User " + userToUpdate + " is already stopped");
            return;
        }
        IActivityManager am = ActivityManager.getService();
        Log.i(TAG, "stop user:" + userToUpdate);
        try {
            am.stopUser(userToUpdate, /* force= */ false, /* callback= */ null);
        } catch (RemoteException e) {
            setMessage(WARN_MESSAGE, "Cannot stop user", e);
            return;
        }
        setMessage(INFO_MESSAGE, "Stopped user " + userToUpdate);
        updateTextInfo();
    }

    private void startUser() {
        int userToUpdate = getSelectedUser();
        if (!canChangeUser(userToUpdate)) {
            return;
        }

        if (mUserManager.isUserRunning(userToUpdate)) {
            setMessage(WARN_MESSAGE, "User " + userToUpdate + " is already running");
            return;
        }
        IActivityManager am = ActivityManager.getService();
        Log.i(TAG, "start user:" + userToUpdate);
        try {
            am.startUserInBackground(userToUpdate);
        } catch (RemoteException e) {
            setMessage(WARN_MESSAGE, "Cannot start user", e);
            return;
        }
        setMessage(INFO_MESSAGE, "Started user " + userToUpdate);
        updateTextInfo();
    }

    private boolean canChangeUser(int userToUpdate) {
        if (userToUpdate == UserHandle.USER_NULL) {
            return false;
        }
        int currentUser = ActivityManager.getCurrentUser();
        if (userToUpdate == currentUser) {
            setMessage(WARN_MESSAGE, "Can not change current user");
            return false;
        }
        return true;
    }

    private void assignUserToZone() {
        int userId = getSelectedUser();
        if (userId == UserHandle.USER_NULL) {
            return;
        }
        Integer zoneId = getSelectedZone();
        if (zoneId == null) {
            return;
        }
        Log.i(TAG, "assigning user:" + userId + " to zone:" + zoneId);
        boolean assignUserToZoneResults;
        try {
            assignUserToZoneResults =
                    mZoneManager.assignProfileUserToOccupantZone(getZoneInfoForId(zoneId), userId);
        } catch (IllegalArgumentException e) {
            setMessage(ERROR_MESSAGE, e.getMessage());
            return;
        }
        if (!assignUserToZoneResults) {
            Log.e(TAG, "Assignment failed");
            setMessage(ERROR_MESSAGE, "Failed to assign user " + userId + " to zone "
                    + zoneId);
            return;
        }
        setMessage(INFO_MESSAGE, "Assigned user " + userId + " to zone " + zoneId);
        updateTextInfo();
    }

    private void setMessage(int messageType, String title, Exception e) {
        StringBuilder messageTextBuilder = new StringBuilder();
        messageTextBuilder.append(title);
        messageTextBuilder.append(": ");
        messageTextBuilder.append(e.getMessage());
        setMessage(messageType, messageTextBuilder.toString());
    }

    private void setMessage(int messageType, Exception e) {
        setMessage(messageType, e.getMessage());
    }

    private void setMessage(int messageType, String message) {
        int textColor;
        switch (messageType) {
            case ERROR_MESSAGE:
                Log.e(TAG, message);
                textColor = Color.RED;
                break;
            case WARN_MESSAGE:
                Log.w(TAG, message);
                textColor = Color.GREEN;
                break;
            case INFO_MESSAGE:
            default:
                Log.i(TAG, message);
                textColor = Color.WHITE;
        }
        mErrorMessageText.setTextColor(textColor);
        mErrorMessageText.setText(message);
    }

    private void launchAppForZone() {
        Intent intent = getSelectedApp();
        if (intent == null) {
            return;
        }
        Integer zoneId = getSelectedZone();
        if (zoneId == null) {
            return;
        }
        CarOccupantZoneManager.OccupantZoneInfo zoneInfo = getZoneInfoForId(zoneId);
        if (zoneInfo == null) {
            Log.e(TAG, "launchAppForZone, invalid zoneId:" + zoneId);
            return;
        }
        int assignedUserId = mZoneManager.getUserForOccupant(zoneInfo);
        if (assignedUserId == UserHandle.USER_NULL) {
            Log.e(TAG, "launchAppForZone, invalid user for zone:" + zoneId);
            return;
        }
        Log.i(TAG, "Launching Intent:" + intent + " for user:" + assignedUserId);
        getContext().startActivityAsUser(intent, UserHandle.of(assignedUserId));
    }

    private void launchAppForDisplayAndUser() {
        Intent intent = getSelectedApp();
        if (intent == null) {
            return;
        }
        int displayId = getSelectedDisplay();
        if (displayId == Display.INVALID_DISPLAY) {
            return;
        }
        int userId = getSelectedUser();
        if (userId == UserHandle.USER_NULL) {
            return;
        }
        Log.i(TAG, "Launching Intent:" + intent + " for user:" + userId
                + " to display:" + displayId);
        Bundle bundle = ActivityOptions.makeBasic().setLaunchDisplayId(displayId).toBundle();
        getContext().startActivityAsUser(intent, bundle, UserHandle.of(userId));
    }

    @Nullable
    private CarOccupantZoneManager.OccupantZoneInfo getZoneInfoForId(int zoneId) {
        List<CarOccupantZoneManager.OccupantZoneInfo> zonelist = mZoneManager.getAllOccupantZones();
        for (CarOccupantZoneManager.OccupantZoneInfo zone : zonelist) {
            if (zone.zoneId == zoneId) {
                return zone;
            }
        }
        return null;
    }

    @Nullable
    private Intent getSelectedApp() {
        String appStr = (String) mAppsSpinner.getSelectedEntry();
        if (appStr == null) {
            Log.w(TAG, "getSelectedApp, no app selected", new RuntimeException());
            return null;
        }
        Intent intent = new Intent();
        intent.setComponent(ComponentName.unflattenFromString(appStr));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    @Nullable
    private Integer getSelectedZone() {
        String zoneStr = mZonesSpinner.getSelectedEntry();
        if (zoneStr == null) {
            Log.w(TAG, "getSelectedZone, no zone selected", new RuntimeException());
            return null;
        }
        return Integer.valueOf(zoneStr.split(",")[0]);
    }

    @Nullable
    private int getSelectedDisplay() {
        String displayStr = mDisplaysSpinner.getSelectedEntry();
        if (displayStr == null) {
            Log.w(TAG, "getSelectedDisplay, no display selected", new RuntimeException());
            return Display.INVALID_DISPLAY;
        }
        return Integer.parseInt(displayStr.split(",")[0]);
    }

    private int getSelectedUser() {
        String userStr = mUsersSpinner.getSelectedEntry();
        if (userStr == null) {
            Log.w(TAG, "getSelectedUser, user not selected", new RuntimeException());
            return UserHandle.USER_NULL;
        }
        return Integer.parseInt(userStr.split(",")[0]);
    }

    // format: id,[CURRENT|PROFILE]
    private ArrayList<String> getUsers() {
        ArrayList<String> users = new ArrayList<>();
        int currentUser = ActivityManager.getCurrentUser();
        users.add(Integer.toString(currentUser) + ",CURRENT");
        int[] profileUsers = mUserManager.getEnabledProfileIds(currentUser);
        for (int profileUser : profileUsers) {
            if (profileUser == currentUser) {
                continue;
            }
            users.add(Integer.toString(profileUser) + ",PROFILE");
        }
        return users;
    }

    // format: displayId,[P,]?,address]
    private ArrayList<String> getDisplays() {
        ArrayList<String> displays = new ArrayList<>();
        Display[] disps = mDisplayManager.getDisplays();
        int uidSelf = Process.myUid();
        for (Display disp : disps) {
            if (!disp.hasAccess(uidSelf)) {
                continue;
            }
            StringBuilder builder = new StringBuilder();
            int displayId = disp.getDisplayId();
            builder.append(displayId);
            builder.append(",");
            DisplayAddress address = disp.getAddress();
            if (address instanceof  DisplayAddress.Physical) {
                builder.append("P,");
            }
            builder.append(address);
            displays.add(builder.toString());
        }
        return displays;
    }

    // format: zoneId,[D|F|R]
    private ArrayList<String> getZones() {
        ArrayList<String> zones = new ArrayList<>();
        List<CarOccupantZoneManager.OccupantZoneInfo> zonelist = mZoneManager.getAllOccupantZones();
        for (CarOccupantZoneManager.OccupantZoneInfo zone : zonelist) {
            StringBuilder builder = new StringBuilder();
            builder.append(zone.zoneId);
            builder.append(",");
            if (zone.occupantType == CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER) {
                builder.append("D");
            } else if (zone.occupantType == CarOccupantZoneManager.OCCUPANT_TYPE_FRONT_PASSENGER) {
                builder.append("F");
            } else {
                builder.append("R");
            }
            zones.add(builder.toString());
        }
        return zones;
    }

    private ArrayList<String> getApps() {
        ArrayList<String> apps = new ArrayList<>();
        apps.add("com.google.android.car.kitchensink/.KitchenSinkActivity");
        apps.add("com.android.car.multidisplay/.launcher.LauncherActivity");
        apps.add("com.google.android.car.multidisplaytest/.MDTest");
        return apps;
    }

    private static class SpinnerWrapper {
        private final Spinner mSpinner;
        private final ArrayList<String> mEntries;
        private final ArrayAdapter<String> mAdapter;

        private static SpinnerWrapper create(Context context, Spinner spinner,
                ArrayList<String> entries) {
            SpinnerWrapper wrapper = new SpinnerWrapper(context, spinner, entries);
            wrapper.init();
            return wrapper;
        }

        private SpinnerWrapper(Context context, Spinner spinner, ArrayList<String> entries) {
            mSpinner = spinner;
            mEntries = new ArrayList<>(entries);
            mAdapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item,
                    mEntries);
        }

        private void init() {
            mAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            mSpinner.setAdapter(mAdapter);
        }

        private void updateEntries(ArrayList<String> entries) {
            mEntries.clear();
            mEntries.addAll(entries);
            mAdapter.notifyDataSetChanged();
        }

        @Nullable
        private String getSelectedEntry() {
            return (String) mSpinner.getSelectedItem();
        }
    }
}
