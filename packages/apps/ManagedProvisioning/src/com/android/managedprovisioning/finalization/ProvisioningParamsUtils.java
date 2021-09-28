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

import android.content.Context;

import java.io.File;

/**
 * Class containing utility methods for working with stored provisioning parameters.
 */
class ProvisioningParamsUtils {

    private static final String PROVISIONING_PARAMS_FILE_NAME =
            "finalization_activity_provisioning_params.xml";

    /**
     * Returns a handle to the provisioning params file, which is used to store the params
     * for use during provisioning finalization.
     */
    File getProvisioningParamsFile(Context context) {
        return new File(context.getFilesDir(), PROVISIONING_PARAMS_FILE_NAME);
    }
}
