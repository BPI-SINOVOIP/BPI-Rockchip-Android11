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

package com.android.cts.verifier.audio;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.Manifest;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Collection;
import java.util.HashMap;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;  // needed to access resource in CTSVerifier project namespace.

/*
 * This tests the USB Restrict Record functionality for the explicit USB device open case
 *   (case "A").
 * The other 2 cases are:
 *   A SINGLE activity is invoked when a USB device is plugged in. (Case B)
 *   ONE OF A MULTIPLE activities is iUSBRestrictedRecordAActivity. (Case C)
 *
 * We are using simple single-character distiguishes to avoid really long class names.
 */
public class USBRestrictRecordAActivity extends PassFailButtons.Activity {
    private static final String TAG = "USBRestrictRecordAActivity";
    private static final boolean DEBUG = false;

    private LocalClickListener mButtonClickListener = new LocalClickListener();

    private Context mContext;

    // Test MUST be run WITHOUT record pemission
    private boolean mHasRecordPermission;

    // System USB stuff
    private UsbManager mUsbManager;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.usb_restrictrecord);

        findViewById(R.id.test_button).setOnClickListener(mButtonClickListener);

        mContext = this;

        mUsbManager = (UsbManager)getSystemService(Context.USB_SERVICE);

        setPassFailButtonClickListeners();
        getPassButton().setEnabled(false);
        setInfoResources(R.string.audio_usb_restrict_record_test,
                R.string.audio_usb_restrict_record_entry, -1);

        mHasRecordPermission = hasRecordPermission();

        if (mHasRecordPermission) {
            TextView tx = findViewById(R.id.usb_restrictrecord_instructions);
            tx.setText(getResources().getString(R.string.audio_usb_restrict_permission_info));
        }
        findViewById(R.id.test_button).setEnabled(!mHasRecordPermission);
    }

    private boolean hasRecordPermission() {
        try {
            PackageManager pm = getPackageManager();
            PackageInfo packageInfo = pm.getPackageInfo(
                    getApplicationInfo().packageName, PackageManager.GET_PERMISSIONS);

            if (packageInfo.requestedPermissions != null) {
                for (String permission : packageInfo.requestedPermissions) {
                    if (permission.equals(Manifest.permission.RECORD_AUDIO)) {
                        return checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
                    }
                }
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Unable to load package's permissions", e);
            Toast.makeText(this, R.string.runtime_permissions_error, Toast.LENGTH_SHORT).show();
        }
        return false;
    }

    public class LocalClickListener implements View.OnClickListener {
        @Override
        public void onClick(View view) {
            int id = view.getId();
            switch (id) {
                case R.id.test_button:
                    connectUSB(mContext);
                    break;
            }
        }
    }

    private class ConnectDeviceBroadcastReceiver extends BroadcastReceiver {
        private final String TAG = "ConnectDeviceBroadcastReceiver";
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    getPassButton().setEnabled(true);

                    // These messages don't really matter
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        Toast.makeText(mContext, "Permission Granted.", Toast.LENGTH_SHORT).show();
                    }
                    else {
                        Toast.makeText(mContext, "Permission Denied.", Toast.LENGTH_SHORT).show();
                    }
                }
            }
        }
    }

    private static final String ACTION_USB_PERMISSION = "com.android.usbdescriptors.USB_PERMISSION";

    public void connectUSB(Context context) {
        HashMap<String, UsbDevice> deviceList = mUsbManager.getDeviceList();
        Collection<UsbDevice> deviceCollection = deviceList.values();
        Object[] devices = deviceCollection.toArray();
        if (devices.length > 0) {
            UsbDevice theDevice = (UsbDevice) devices[0];

            PendingIntent permissionIntent =
                    PendingIntent.getBroadcast(context, 0, new Intent(ACTION_USB_PERMISSION), 0);

            IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
            ConnectDeviceBroadcastReceiver usbReceiver =
                    new ConnectDeviceBroadcastReceiver();
            context.registerReceiver(usbReceiver, filter);

            mUsbManager.requestPermission(theDevice, permissionIntent);
        }
    }
}
