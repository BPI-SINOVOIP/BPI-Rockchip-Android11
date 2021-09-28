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

package com.android.car.companiondevicesupport.feature.howitzer;

import static com.android.car.connecteddevice.util.SafeLog.logd;

import android.content.Context;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.feature.LocalFeature;
import com.android.car.connecteddevice.ConnectedDeviceManager;
import com.android.car.connecteddevice.ConnectedDeviceManager.ConnectionCallback;
import com.android.car.connecteddevice.ConnectedDeviceManager.DeviceCallback;
import com.android.car.connecteddevice.model.ConnectedDevice;
import com.android.car.connecteddevice.util.ByteUtils;

import java.util.UUID;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/** Reference feature used to validate the performance of the connected device connection. */
public class ConnectionHowitzer extends LocalFeature {

    private static final String TAG = "ConnectionHowitzer";

    private static final int LARGE_MESSAGE_SIZE = 100000;

    // TODO (b/143789390) Change to true to turn feature on for now.
    private static final boolean FEATURE_ENABLED = false;

    public ConnectionHowitzer(Context context, ConnectedDeviceManager connectedDeviceManager) {
        super(context, connectedDeviceManager,
                UUID.fromString(context.getString(R.string.connection_howitzer_feature_id)));
    }

    @Override
    protected void onSecureChannelEstablished(ConnectedDevice device) {
        if (FEATURE_ENABLED) {
            sendMessage(device, LARGE_MESSAGE_SIZE);
        }
    }

    private void sendMessage(ConnectedDevice device, int messageSize) {
        byte[] message = ByteUtils.randomBytes(messageSize);
        sendMessageSecurely(device, message);
    }
}
