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

package com.google.android.car.kitchensink.packageinfo;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.pm.UserInfo;
import android.os.Bundle;
import android.os.UserManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Test fragment to check packages installed for each user
 * <p>
 * Options to apply conditions to filter out packages, if package
 * <ul>
 * <li>only have activities.
 * <li>have service but not exported or for single user.
 * <li>does not require any key permission
 * (INTERACT_ACROSS_USERS, INTERACT_ACROSS_USERS_FULL, WRITE_DEVICE_CONFIG).
 * <li>does not have sharedUserId or shardUserId is not system uid.
 * </ul>
 */
public final class PackageInfoFragment extends Fragment{
    private static final String TAG = "PackageInfoTest";
    private static final boolean DEBUG = true;
    private static final int PACKAGE_FLAGS = PackageManager.GET_META_DATA
            | PackageManager.GET_ACTIVITIES | PackageManager.GET_SERVICES
            | PackageManager.GET_PROVIDERS | PackageManager.GET_RECEIVERS
            | PackageManager.GET_PERMISSIONS | PackageManager.GET_SIGNATURES;
    private static final List<String> IMPORTANT_PERMISSIONS = Arrays.asList(
            "android.permission.INTERACT_ACROSS_USERS",
            "android.permission.INTERACT_ACROSS_USERS_FULL",
            "android.permission.WRITE_DEVICE_CONFIG");
    private static final String SYSTEM_UID = "android.uid.system";

    private final List<PackageInfo> mPackagesToDisableForSystemUser = new ArrayList<>();

    private UserManager mUserManager;
    private PackageManager mPackageManager;
    private UserInfo mUserToShow;
    private int mShowFlags;
    private TextView mPackageView;
    private boolean mFilterActivities;
    private boolean mFilterServices;
    private boolean mFilterPermissions;
    private boolean mFilterSharedUid;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUserManager = (UserManager) getContext().getSystemService(Context.USER_SERVICE);
        mPackageManager = getActivity().getPackageManager();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        View view = inflater.inflate(R.layout.package_info_test, container, false);
        setListener(view);

        return view;
    }

    private void refreshPackages() {
        List<PackageInfo> packages = new ArrayList<PackageInfo>();
        try {
            packages = mPackageManager.getInstalledPackagesAsUser(PACKAGE_FLAGS, mUserToShow.id);
            if (DEBUG) {
                Log.d(TAG, "total packages found: " + packages.size());
            }
        } catch (Exception e) {
            if (DEBUG) {
                Log.e(TAG, "failed to get packages for given user: " + mUserToShow);
            }
            mPackageView.setText("Cannot retrieve packages for this user..");
            return;
        }

        mPackagesToDisableForSystemUser.clear();

        for (PackageInfo packageInfo : packages) {
            Log.d(TAG, "checking package: " + packageInfo);
            boolean toBlacklist = true;
            // check share user id, show package does not have sharedUserId or not system uid
            if (mFilterSharedUid) {
                if (DEBUG) {
                    Log.d(TAG, "sharedUid flagged: " + (packageInfo.sharedUserId == null
                                || !packageInfo.sharedUserId.equals(SYSTEM_UID)));
                }

                toBlacklist &= (packageInfo.sharedUserId == null
                        || !packageInfo.sharedUserId.equals(SYSTEM_UID));
            }

            // check permissions, show package does not require selected permissions
            if (mFilterPermissions && packageInfo.requestedPermissions != null) {
                if (DEBUG) {
                    for (String info : Arrays.asList(packageInfo.requestedPermissions)) {
                        Log.d(TAG, info + " flagged: " + (!IMPORTANT_PERMISSIONS.contains(info)));
                    }
                }

                toBlacklist &= !(Arrays.asList(packageInfo.requestedPermissions).stream().anyMatch(
                        info -> IMPORTANT_PERMISSIONS.contains(info)));
            }
            // check services, w/o service or service not exported and w/o single user flag
            if (mFilterServices && packageInfo.services != null) {
                if (DEBUG) {
                    for (ServiceInfo info : Arrays.asList(packageInfo.services)) {
                        Log.d(TAG, info + " flagged: " + (!info.exported
                                && (info.flags & ServiceInfo.FLAG_SINGLE_USER) == 0));
                    }
                }

                toBlacklist &= Arrays.asList(packageInfo.services).stream().anyMatch(info ->
                    !info.exported && (info.flags & ServiceInfo.FLAG_SINGLE_USER) == 0);
            }
            // check activities
            if (mFilterActivities) {
                if (DEBUG) {
                    Log.d(TAG, packageInfo + " contain activities only, flagged: " + (
                            packageInfo.activities != null
                            && packageInfo.services == null
                            && packageInfo.providers == null));
                }
                toBlacklist &= (packageInfo.activities != null
                    && packageInfo.services == null && packageInfo.providers == null);
            }

            if (toBlacklist) {
                mPackagesToDisableForSystemUser.add(packageInfo);
            }
        }
    }

    private void showPackagesOnView(TextView tv) {
        refreshPackages();

        tv.setText(mPackagesToDisableForSystemUser.size() + " Packages found ...\n");

        for (PackageInfo info : mPackagesToDisableForSystemUser) {
            tv.append(info.packageName.toString() + "\n");
        }
    }

    private void setListener(View v) {
        List<UserInfo> users = mUserManager.getUsers();
        mUserToShow = users.get(0);
        Spinner spinner = v.findViewById(R.id.spinner);
        ArrayAdapter<UserInfo> userArrayAdapter = new ArrayAdapter<UserInfo>(
                    getContext(), android.R.layout.simple_spinner_item, users);
        userArrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(userArrayAdapter);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View currentView,
                    int position, long id) {
                mUserToShow = (UserInfo) parent.getItemAtPosition(position);
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        CheckBox activities = v.findViewById(R.id.checkbox_activities);
        CheckBox services = v.findViewById(R.id.checkbox_services);
        CheckBox permissions = v.findViewById(R.id.checkbox_permissions);
        CheckBox shareduid = v.findViewById(R.id.checkbox_shareduid);
        Button showButton = v.findViewById(R.id.button_show);
        TextView packageView = v.findViewById(R.id.packages);

        activities.setOnClickListener(view -> mFilterActivities = ((CheckBox) view).isChecked());
        services.setOnClickListener(view -> mFilterServices = ((CheckBox) view).isChecked());
        permissions.setOnClickListener(view -> mFilterPermissions = ((CheckBox) view).isChecked());
        shareduid.setOnClickListener(view -> mFilterSharedUid = ((CheckBox) view).isChecked());
        showButton.setOnClickListener(view -> showPackagesOnView(packageView));
    }
}
