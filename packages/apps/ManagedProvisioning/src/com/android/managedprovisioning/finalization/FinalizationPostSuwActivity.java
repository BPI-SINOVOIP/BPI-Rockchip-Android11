/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.finalization;

import android.app.admin.DevicePolicyManager;

/**
 * This class is used to make sure that we start the MDM after we shut the setup wizard down.
 * The shut down of the setup wizard is initiated in the
 * {@link com.android.managedprovisioning.provisioning.ProvisioningActivity} by calling
 * {@link DevicePolicyManager#setUserProvisioningState(int, int)}. This will cause the
 * Setup wizard to shut down and send a ACTION_PROVISIONING_FINALIZATION intent. This intent is
 * caught by this receiver instead which will send the
 * ACTION_PROFILE_PROVISIONING_COMPLETE broadcast to the MDM, which can then present it's own
 * activities.
 */
public class FinalizationPostSuwActivity extends FinalizationActivityBase {

    @Override
    protected FinalizationController createFinalizationController() {
        return new FinalizationController(this, new FinalizationPostSuwControllerLogic(this));
    }

    @Override
    protected void onFinalizationCompletedWithChildActivityLaunched() {
        // Commit state immediately, without waiting for child activity to complete.
        getFinalizationController().commitFinalizedState();
        // To prevent b/131315856, we don't call finish() when we have launched the admin app.
        // Instead, we let android:noHistory automatically finish this activity.
    }
}
