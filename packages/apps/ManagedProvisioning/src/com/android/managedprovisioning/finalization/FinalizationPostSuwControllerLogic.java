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

import static com.android.managedprovisioning.finalization.FinalizationController.PROVISIONING_FINALIZED_RESULT_CHILD_ACTIVITY_LAUNCHED;
import static com.android.managedprovisioning.finalization.FinalizationController.PROVISIONING_FINALIZED_RESULT_NO_CHILD_ACTIVITY_LAUNCHED;
import static com.android.managedprovisioning.finalization.FinalizationController.ProvisioningFinalizedResult;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserHandle;

import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * This controller is used to finalize managed provisioning when a Device Owner or Profile Owner
 * was set up during Setup Wizard, but we are running finalization after Setup Wizard has completed.
 */
public class FinalizationPostSuwControllerLogic implements FinalizationControllerLogic {
    private final Activity mActivity;
    private final Utils mUtils;
    private final ProvisioningIntentProvider mProvisioningIntentProvider;
    private final SendDpcBroadcastServiceUtils mSendDpcBroadcastServiceUtils;
    private final ProvisioningAnalyticsTracker mProvisioningAnalyticsTracker;

    public FinalizationPostSuwControllerLogic(Activity activity) {
        this(activity, new Utils(), new SendDpcBroadcastServiceUtils(),
                new ProvisioningAnalyticsTracker(
                        MetricsWriterFactory.getMetricsWriter(activity, new SettingsFacade()),
                        new ManagedProvisioningSharedPreferences(activity)));
    }

    public FinalizationPostSuwControllerLogic(Activity activity, Utils utils,
            SendDpcBroadcastServiceUtils sendDpcBroadcastServiceUtils,
            ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        mActivity = activity;
        mUtils = utils;
        mProvisioningIntentProvider = new ProvisioningIntentProvider();
        mSendDpcBroadcastServiceUtils = sendDpcBroadcastServiceUtils;
        mProvisioningAnalyticsTracker = provisioningAnalyticsTracker;
    }

    @Override
    public boolean isReadyForFinalization(ProvisioningParams params) {
        return true;
    }

    @Override
    public @ProvisioningFinalizedResult int notifyDpcManagedProfile(
            ProvisioningParams params, int requestCode) {
        mSendDpcBroadcastServiceUtils.startSendDpcBroadcastService(mActivity, params);

        final UserHandle managedProfileUserHandle = mUtils.getManagedProfile(mActivity);
        final int userId = managedProfileUserHandle.getIdentifier();
        return mProvisioningIntentProvider.canLaunchDpc(params, userId, mUtils, mActivity)
                        ? PROVISIONING_FINALIZED_RESULT_CHILD_ACTIVITY_LAUNCHED
                        : PROVISIONING_FINALIZED_RESULT_NO_CHILD_ACTIVITY_LAUNCHED;
    }

    @Override
    public @ProvisioningFinalizedResult int notifyDpcManagedDeviceOrUser(
            ProvisioningParams params, int requestCode) {
        // For managed user and device owner, we send the provisioning complete intent and
        // maybe launch the DPC.
        final int userId = UserHandle.myUserId();
        final Intent provisioningCompleteIntent = mProvisioningIntentProvider
                .createProvisioningCompleteIntent(params, userId, mUtils, mActivity);
        if (provisioningCompleteIntent == null) {
            return PROVISIONING_FINALIZED_RESULT_NO_CHILD_ACTIVITY_LAUNCHED;
        }
        mActivity.sendBroadcast(provisioningCompleteIntent);

        mProvisioningIntentProvider.maybeLaunchDpc(params, userId, mUtils, mActivity,
                mProvisioningAnalyticsTracker);

        return PROVISIONING_FINALIZED_RESULT_NO_CHILD_ACTIVITY_LAUNCHED;
    }

    @Override
    public boolean shouldFinalizePrimaryProfile(ProvisioningParams params) {
        // If we're not in the admin-integrated flow, SendDpcBroadcastService has already invoked
        // PrimaryProfileFinalizationHelper, so we don't invoke it again in commitFinalizedState().
        return mUtils.isAdminIntegratedFlow(params);
    }

    @Override
    public void saveInstanceState(Bundle outState) {}

    @Override
    public void restoreInstanceState(Bundle savedInstanceState, ProvisioningParams params) {}

    @Override
    public void activityDestroyed(boolean isFinishing) {}
}
