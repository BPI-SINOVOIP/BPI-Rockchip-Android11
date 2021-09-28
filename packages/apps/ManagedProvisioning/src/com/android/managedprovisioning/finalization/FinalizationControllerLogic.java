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

import static com.android.managedprovisioning.finalization.FinalizationController.ProvisioningFinalizedResult;

import android.os.Bundle;

import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * The methods in this interface are used to customize the behavior of
 * {@link FinalizationController}.  An implementation of this interface must be provided when a
 * {@link FinalizationController} is constructed.
 */
public interface FinalizationControllerLogic {
    /**
     * Return true if all preconditions for {@link FinalizationController#provisioningFinalized()}
     * to execute have been satisfied.  If false is returned,
     * {@link FinalizationController#provisioningFinalized()} will need to be attempted again later.
     */
    boolean isReadyForFinalization(ProvisioningParams params);

    /**
     * Notify the DPC for a managed profile that provisioning is completed.
     *
     * @return a {@link ProvisioningFinalizedResult} indicating whether the DPC started an activity
     * as a result of that notification.
     */
    @ProvisioningFinalizedResult int notifyDpcManagedProfile(
            ProvisioningParams params, int requestCode);

    /**
     * Notify the DPC for a managed device or managed user that provisioning is completed.
     *
     * @return a {@link ProvisioningFinalizedResult} indicating whether the DPC started an activity
     * as a result of that notification.
     */
    @ProvisioningFinalizedResult int notifyDpcManagedDeviceOrUser(
            ProvisioningParams params, int requestCode);

    /**
     * Return true if, after managed profile provisioning, {@link PrimaryProfileFinalizationHelper}
     * should be invoked at the time we update the system's provisioning state.
     *
     * If this is false, then {@link PrimaryProfileFinalizationHelper} must have already been
     * invoked prior to reaching this point.
     */
    boolean shouldFinalizePrimaryProfile(ProvisioningParams params);

    /**
     * This method is called when onSaveInstanceState() executes on the finalization activity.
     */
    void saveInstanceState(Bundle outState);

    /**
     * When saved instance state is passed to the finalization activity in its onCreate() method,
     * that state is passed to the FinalizationControllerLogic object here so it can be restored.
     */
    void restoreInstanceState(Bundle savedInstanceState, ProvisioningParams params);

    /**
     * Execute cleanup actions that need to be performed when the finalization activity is
     * destroyed, even if the system's provisioning state has not yet been finalized.
     */
    void activityDestroyed(boolean isFinishing);
}
