/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.tv.settings.deviceadmin;

import android.app.Activity;
import android.app.admin.DeviceAdminInfo;
import android.app.admin.DeviceAdminReceiver;
import android.app.admin.DevicePolicyManager;
import android.app.settings.SettingsEnums;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.UserHandle;
import android.util.EventLog;
import android.util.Log;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.List;

public class DeviceAdminAdd extends Activity {
    static final String TAG = "DeviceAdminAdd";

    DevicePolicyManager mDPM;
    DeviceAdminInfo mDeviceAdmin;
    String mProfileOwnerName;

    boolean mRefreshing;
    boolean mAddingProfileOwner;

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        mDPM = (DevicePolicyManager)getSystemService(Context.DEVICE_POLICY_SERVICE);
        PackageManager packageManager = getPackageManager();

        if ((getIntent().getFlags()&Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
            Log.w(TAG, "Cannot start ADD_DEVICE_ADMIN as a new task");
            finish();
            return;
        }

        String action = getIntent().getAction();
        ComponentName who = (ComponentName)getIntent().getParcelableExtra(
                DevicePolicyManager.EXTRA_DEVICE_ADMIN);
        if (who == null) {
            Log.w(TAG, "No component specified in " + action);
            finish();
            return;
        }

        if (action != null && action.equals(DevicePolicyManager.ACTION_SET_PROFILE_OWNER)) {
            setResult(RESULT_CANCELED);
            setFinishOnTouchOutside(true);
            mAddingProfileOwner = true;
            mProfileOwnerName =
                    getIntent().getStringExtra(DevicePolicyManager.EXTRA_PROFILE_OWNER_NAME);
            String callingPackage = getCallingPackage();
            if (callingPackage == null || !callingPackage.equals(who.getPackageName())) {
                Log.e(TAG, "Unknown or incorrect caller");
                finish();
                return;
            }
            try {
                PackageInfo packageInfo = packageManager.getPackageInfo(callingPackage, 0);
                if ((packageInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
                    Log.e(TAG, "Cannot set a non-system app as a profile owner");
                    finish();
                    return;
                }
            } catch (NameNotFoundException nnfe) {
                Log.e(TAG, "Cannot find the package " + callingPackage);
                finish();
                return;
            }
        }

        ActivityInfo ai;
        try {
            ai = packageManager.getReceiverInfo(who, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Unable to retrieve device policy " + who, e);
            finish();
            return;
        }

        // When activating, make sure the given component name is actually a valid device admin.
        // No need to check this when deactivating, because it is safe to deactivate an active
        // invalid device admin.
        if (!mDPM.isAdminActive(who)) {
            List<ResolveInfo> avail = packageManager.queryBroadcastReceivers(
                    new Intent(DeviceAdminReceiver.ACTION_DEVICE_ADMIN_ENABLED),
                    PackageManager.GET_DISABLED_UNTIL_USED_COMPONENTS);
            int count = avail == null ? 0 : avail.size();
            boolean found = false;
            for (int i=0; i<count; i++) {
                ResolveInfo ri = avail.get(i);
                if (ai.packageName.equals(ri.activityInfo.packageName)
                        && ai.name.equals(ri.activityInfo.name)) {
                    try {
                        // We didn't retrieve the meta data for all possible matches, so
                        // need to use the activity info of this specific one that was retrieved.
                        ri.activityInfo = ai;
                        DeviceAdminInfo dpi = new DeviceAdminInfo(this, ri);
                        found = true;
                    } catch (XmlPullParserException e) {
                        Log.w(TAG, "Bad " + ri.activityInfo, e);
                    } catch (IOException e) {
                        Log.w(TAG, "Bad " + ri.activityInfo, e);
                    }
                    break;
                }
            }
            if (!found) {
                Log.w(TAG, "Request to add invalid device admin: " + who);
                finish();
                return;
            }
        }

        ResolveInfo ri = new ResolveInfo();
        ri.activityInfo = ai;
        try {
            mDeviceAdmin = new DeviceAdminInfo(this, ri);
        } catch (XmlPullParserException e) {
            Log.w(TAG, "Unable to retrieve device policy " + who, e);
            finish();
            return;
        } catch (IOException e) {
            Log.w(TAG, "Unable to retrieve device policy " + who, e);
            finish();
            return;
        }

        if (mAddingProfileOwner) {
            // If we're trying to add a profile owner and user setup hasn't completed yet, no
            // need to prompt for permission. Just add and finish
            if (!mDPM.hasUserSetupCompleted()) {
                addAndFinish();
                return;
            }

            // othewise, only the defined default supervision profile owner can be set after user
            // setup.
            final String supervisor = getString(
                    com.android.internal.R.string.config_defaultSupervisionProfileOwnerComponent);
            if (supervisor == null) {
                Log.w(TAG, "Unable to set profile owner post-setup, no default supervisor"
                        + "profile owner defined");
                finish();
                return;
            }

            final ComponentName supervisorComponent = ComponentName.unflattenFromString(
                    supervisor);
            if (who.compareTo(supervisorComponent) != 0) {
                Log.w(TAG, "Unable to set non-default profile owner post-setup " + who);
                finish();
                return;
            } else {
                addAndFinish();
            }
        }

        // do not finish if ACTION_ADD_DEVICE_ADMIN, expected by some CTS tests
        if (DevicePolicyManager.ACTION_ADD_DEVICE_ADMIN.equals(getIntent().getAction())) {
            if (mDPM.isAdminActive(who)) {
                if (mDPM.isRemovingAdmin(who, android.os.Process.myUserHandle().getIdentifier())) {
                    Log.w(TAG, "Requested admin is already being removed: " + who);
                    finish();
                    return;
                }
            }
        } else {
            finish();
        }
    }

    void addAndFinish() {
        try {
            logSpecialPermissionChange(true, mDeviceAdmin.getComponent().getPackageName());
            mDPM.setActiveAdmin(mDeviceAdmin.getComponent(), mRefreshing);
            EventLog.writeEvent(1234,
                mDeviceAdmin.getActivityInfo().applicationInfo.uid);

            setResult(Activity.RESULT_OK);
        } catch (RuntimeException e) {
            // Something bad happened...  could be that it was
            // already set, though.
            Log.w(TAG, "Exception trying to activate admin "
                    + mDeviceAdmin.getComponent(), e);
            if (mDPM.isAdminActive(mDeviceAdmin.getComponent())) {
                setResult(Activity.RESULT_OK);
            }
        }
        if (mAddingProfileOwner) {
            try {
                mDPM.setProfileOwner(mDeviceAdmin.getComponent(),
                        mProfileOwnerName, UserHandle.myUserId());
            } catch (RuntimeException re) {
                setResult(Activity.RESULT_CANCELED);
            }
        }
        finish();
    }


    void logSpecialPermissionChange(boolean allow, String packageName) {
        int logCategory = allow ? SettingsEnums.APP_SPECIAL_PERMISSION_ADMIN_ALLOW :
                SettingsEnums.APP_SPECIAL_PERMISSION_ADMIN_DENY;
    }
}
