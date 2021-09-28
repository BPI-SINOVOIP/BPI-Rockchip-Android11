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

package com.android.managedprovisioning.finalization;

import static com.android.managedprovisioning.finalization.SendDpcBroadcastService.EXTRA_PROVISIONING_PARAMS;

import android.content.Context;
import android.content.Intent;

import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * Class containing utility methods for starting up the SendDpcBroadcastService.
 */
class SendDpcBroadcastServiceUtils {

    /**
     * Start a service which notifies the DPC on the managed profile that provisioning has
     * completed. When the DPC has received the intent, send notify the primary instance that the
     * profile is ready. The service is needed to prevent the managed provisioning process from
     * getting killed while the user is on the DPC screen.
     */
    void startSendDpcBroadcastService(Context context, ProvisioningParams params) {
        context.startService(
                new Intent(context, SendDpcBroadcastService.class)
                        .putExtra(EXTRA_PROVISIONING_PARAMS, params));
    }
}
