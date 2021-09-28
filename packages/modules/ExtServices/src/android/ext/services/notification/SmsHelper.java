/**
 * Copyright (C) 2019 The Android Open Source Project
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
package android.ext.services.notification;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.provider.Telephony;
import android.util.Log;

import androidx.annotation.Nullable;

/**
 * A helper class for storing and retrieving the default SMS application.
 */
public class SmsHelper {
    private static final String TAG = "SmsHelper";

    // TODO: user RoleManager instead of copying this constant
    private static String ACTION_DEFAULT_SMS_PACKAGE_CHANGED_INTERNAL
            = "android.provider.action.DEFAULT_SMS_PACKAGE_CHANGED_INTERNAL";

    private final Context mContext;
    private String mDefaultSmsPackage;
    private BroadcastReceiver mBroadcastReceiver;

    SmsHelper(Context context) {
        mContext = context.getApplicationContext();
    }

    void initialize() {
        if (mBroadcastReceiver == null) {
            mDefaultSmsPackage = Telephony.Sms.getDefaultSmsPackage(mContext);
            mBroadcastReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (ACTION_DEFAULT_SMS_PACKAGE_CHANGED_INTERNAL.equals(intent.getAction())) {
                        mDefaultSmsPackage = Telephony.Sms.getDefaultSmsPackage(mContext);
                    } else {
                        Log.w(TAG, "Unknown broadcast received: " + intent.getAction());
                    }
                }
            };
            mContext.registerReceiver(
                    mBroadcastReceiver,
                    new IntentFilter(ACTION_DEFAULT_SMS_PACKAGE_CHANGED_INTERNAL));
        }
    }

    void destroy() {
        if (mBroadcastReceiver != null) {
            mContext.unregisterReceiver(mBroadcastReceiver);
            mBroadcastReceiver = null;
        }
    }

    @Nullable
    public String getDefaultSmsPackage() {
        return mDefaultSmsPackage;
    }
}
