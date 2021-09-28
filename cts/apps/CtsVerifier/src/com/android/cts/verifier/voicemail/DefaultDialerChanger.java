/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.cts.verifier.voicemail;

import android.app.Activity;
import android.app.role.RoleManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.provider.Settings;
import android.telecom.TelecomManager;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.cts.verifier.R;

/**
 * Provides common UI for tests that needs to be set as the default dialer.
 */
public class DefaultDialerChanger {

    private final Activity mActivity;

    private final ImageView mSetDefaultDialerImage;
    private final Button mSetDefaultDialerButton;

    private final ImageView mRestoreDefaultDialerImage;
    private final Button mRestoreDefaultDialerButton;

    private boolean mRestorePending;

    public DefaultDialerChanger(Activity activity) {
        mActivity = activity;

        mSetDefaultDialerImage = (ImageView) mActivity.findViewById(R.id.set_default_dialer_image);
        mRestoreDefaultDialerImage = (ImageView) mActivity
                .findViewById(R.id.restore_default_dialer_image);

        mSetDefaultDialerButton = (Button) mActivity.findViewById(R.id.set_default_dialer);
        mRestoreDefaultDialerButton = (Button) mActivity.findViewById(R.id.restore_default_dialer);

        final TelecomManager telecomManager = mActivity.getSystemService(TelecomManager.class);
        updateSetDefaultDialerState(telecomManager.getDefaultDialerPackage());

        mSetDefaultDialerButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (telecomManager.getDefaultDialerPackage().equals(mActivity.getPackageName())) {
                    Toast.makeText(mActivity,
                            R.string.voicemail_default_dialer_already_set, Toast.LENGTH_SHORT)
                            .show();
                    return;
                }

                final RoleManager roleManager = mActivity.getSystemService(RoleManager.class);
                final Intent intent = roleManager.createRequestRoleIntent(RoleManager.ROLE_DIALER);
                mActivity.startActivityForResult(intent, 0);
            }
        });

        mRestoreDefaultDialerButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (!telecomManager.getDefaultDialerPackage().equals(mActivity.getPackageName())) {
                    Toast.makeText(mActivity,
                            R.string.voicemail_default_dialer_already_restored, Toast.LENGTH_SHORT)
                            .show();
                    return;
                }

                final Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
                mActivity.startActivityForResult(intent, 0);
            }
        });

        mActivity.registerReceiver(mDefaultDialerChangedReceiver,
                new IntentFilter(TelecomManager.ACTION_DEFAULT_DIALER_CHANGED));
    }

    public void setRestorePending(boolean value) {
        mRestorePending = value;
    }

    private BroadcastReceiver mDefaultDialerChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String packageName =
                    intent.getStringExtra(TelecomManager.EXTRA_CHANGE_DEFAULT_DIALER_PACKAGE_NAME);
            if (!mRestorePending) {
                updateSetDefaultDialerState(packageName);
            } else {
                if (packageName.equals(mActivity.getPackageName())) {
                    mRestoreDefaultDialerImage
                            .setImageDrawable(mActivity.getDrawable(R.drawable.fs_indeterminate));
                } else {
                    mRestoreDefaultDialerImage
                            .setImageDrawable(mActivity.getDrawable(R.drawable.fs_good));
                }
            }
        }
    };

    private void updateSetDefaultDialerState(String packageName) {
        if (packageName.equals(mActivity.getPackageName())) {
            mSetDefaultDialerImage.setImageDrawable(mActivity.getDrawable(R.drawable.fs_good));
        } else {
            mSetDefaultDialerImage
                    .setImageDrawable(mActivity.getDrawable(R.drawable.fs_indeterminate));
        }
    }

    public void destroy() {
        mActivity.unregisterReceiver(mDefaultDialerChangedReceiver);
    }
}
