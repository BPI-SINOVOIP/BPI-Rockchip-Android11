/*
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

package com.android.bluetooth;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Activity that routes to Bluetooth settings when launched
 */
public class BluetoothPrefs extends Activity {

    public static final String BLUETOOTH_SETTING_ACTION = "android.settings.BLUETOOTH_SETTINGS";
    public static final String BLUETOOTH_SETTING_CATEGORY = "android.intent.category.DEFAULT";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent launchIntent = new Intent();
        launchIntent.setAction(BLUETOOTH_SETTING_ACTION);
        launchIntent.addCategory(BLUETOOTH_SETTING_CATEGORY);
        startActivity(launchIntent);
        finish();
    }
}
