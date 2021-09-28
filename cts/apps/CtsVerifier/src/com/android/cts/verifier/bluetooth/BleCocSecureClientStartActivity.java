/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.cts.verifier.bluetooth;

import android.content.Intent;
import android.os.Bundle;
import com.android.cts.verifier.R;

public class BleCocSecureClientStartActivity extends BleCocClientTestBaseActivity {
    private Intent mIntent;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setInfoResources(R.string.ble_coc_client_test_name,
                R.string.ble_coc_secure_client_test_info, -1);

        mIntent = new Intent(this, BleCocClientService.class);
        mIntent.setAction(BleCocClientService.BLE_COC_CLIENT_ACTION_LE_SECURE_CONNECT);

        startService(mIntent);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopService(mIntent);
    }
}
