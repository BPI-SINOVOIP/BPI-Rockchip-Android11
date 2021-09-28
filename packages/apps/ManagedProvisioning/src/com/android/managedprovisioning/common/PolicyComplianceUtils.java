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

package com.android.managedprovisioning.common;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;

import android.annotation.Nullable;
import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.Intent;
import android.os.UserHandle;

import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * Class containing utility methods for starting up a DPC during Setup Wizard.
 */
public class PolicyComplianceUtils {

    /**
     * Returns whether the DPC handles the policy compliance activity.
     */
    public boolean isPolicyComplianceActivityResolvable(Activity parentActivity,
            ProvisioningParams params, @Nullable String category, Utils utils) {
        return getPolicyComplianceIntentIfResolvable(parentActivity, params, category, utils) !=
                null;
    }

    /**
     * Starts the policy compliance activity if it can be resolved, and returns whether the
     * activity was started.
     */
    public boolean startPolicyComplianceActivityForResultIfResolved(Activity parentActivity,
            ProvisioningParams params, @Nullable String category, int requestCode, Utils utils,
            ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        final UserHandle userHandle = getPolicyComplianceUserHandle(parentActivity, params, utils);
        final Intent policyComplianceIntent = getPolicyComplianceIntentIfResolvable(
                parentActivity, params, category, utils);

        if (policyComplianceIntent != null) {
            parentActivity.startActivityForResultAsUser(
                    policyComplianceIntent, requestCode, userHandle);
            // Override the animation to avoid the transition jumping back and forth (b/149463287).
            parentActivity.overridePendingTransition(/* enterAnim = */ 0, /* exitAnim= */ 0);
            provisioningAnalyticsTracker.logDpcSetupStarted(parentActivity,
                    policyComplianceIntent.getAction());
            return true;
        }

        return false;
    }

    private Intent getPolicyComplianceIntentIfResolvable(Activity parentActivity,
            ProvisioningParams params, @Nullable String category, Utils utils) {
        final UserHandle userHandle = getPolicyComplianceUserHandle(parentActivity, params, utils);
        final Intent policyComplianceIntent = getPolicyComplianceIntent(params, category);

        final boolean intentResolvable = utils.canResolveIntentAsUser(parentActivity,
                policyComplianceIntent, userHandle.getIdentifier());

        return intentResolvable ? policyComplianceIntent : null;
    }

    private Intent getPolicyComplianceIntent(ProvisioningParams params, @Nullable String category) {
        final String adminPackage = params.inferDeviceAdminPackageName();

        final Intent policyComplianceIntent =
                new Intent(DevicePolicyManager.ACTION_ADMIN_POLICY_COMPLIANCE);
        if (category != null) {
            policyComplianceIntent.addCategory(category);
        }
        policyComplianceIntent.putExtra(
                DevicePolicyManager.EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE,
                params.adminExtrasBundle);
        policyComplianceIntent.setPackage(adminPackage);

        return policyComplianceIntent;
    }

    private UserHandle getPolicyComplianceUserHandle(Activity parentActivity,
            ProvisioningParams params, Utils utils) {
        return params.provisioningAction.equals(ACTION_PROVISION_MANAGED_PROFILE)
                ? utils.getManagedProfile(parentActivity)
                : UserHandle.of(UserHandle.myUserId());
    }
}
