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

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;

import static com.android.internal.util.Preconditions.checkNotNull;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.UserHandle;
import android.os.UserManager;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.common.ProvisionLogger;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * This controller is invoked, via a call to
 * {@link #deviceManagementEstablished(ProvisioningParams)}, immediately after a Device Owner or
 * Profile Owner is provisioned.  If the user is creating a managed profile on a device post-Setup
 * Wizard, then we immediately start the DPC app and set the provisioning state to finalized, so no
 * further finalization calls are necessary.  On the other hand, if we are still inside of Setup
 * Wizard, provisioning needs to be finalized after this using an instance of
 * {@link FinalizationController}.
 */
public final class PreFinalizationController {
    private final Activity mActivity;
    private final Utils mUtils;
    private final SettingsFacade mSettingsFacade;
    private final UserProvisioningStateHelper mUserProvisioningStateHelper;
    private final ProvisioningParamsUtils mProvisioningParamsUtils;
    private final SendDpcBroadcastServiceUtils mSendDpcBroadcastServiceUtils;

    public PreFinalizationController(Activity activity,
          UserProvisioningStateHelper userProvisioningStateHelper) {
        this(
                activity,
                new Utils(),
                new SettingsFacade(),
                userProvisioningStateHelper,
                new ProvisioningParamsUtils(),
                new SendDpcBroadcastServiceUtils());
    }

    public PreFinalizationController(Activity activity) {
        this(
                activity,
                new Utils(),
                new SettingsFacade(),
                new UserProvisioningStateHelper(activity),
                new ProvisioningParamsUtils(),
                new SendDpcBroadcastServiceUtils());
    }

    @VisibleForTesting
    PreFinalizationController(Activity activity,
            Utils utils,
            SettingsFacade settingsFacade,
            UserProvisioningStateHelper helper,
            ProvisioningParamsUtils provisioningParamsUtils,
            SendDpcBroadcastServiceUtils sendDpcBroadcastServiceUtils) {
        mActivity = checkNotNull(activity);
        mUtils = checkNotNull(utils);
        mSettingsFacade = checkNotNull(settingsFacade);
        mUserProvisioningStateHelper = checkNotNull(helper);
        mProvisioningParamsUtils = provisioningParamsUtils;
        mSendDpcBroadcastServiceUtils = sendDpcBroadcastServiceUtils;
    }

    /**
     * This method is invoked after a Device Owner or Profile Owner has been set up.
     *
     * <p>If provisioning happens as part of SUW, we rely on an instance of
     * FinalizationControllerBase to be invoked later on. Otherwise, this method will finalize
     * provisioning. If called after SUW, this method notifies the DPC about the completed
     * provisioning; otherwise, it stores the provisioning params for later digestion.</p>
     *
     * <p>Note that fully managed device provisioning is only possible during SUW.
     *
     * @param params the provisioning params
     */
    public final void deviceManagementEstablished(ProvisioningParams params) {
        if (!mUserProvisioningStateHelper.isStateUnmanagedOrFinalized()) {
            // In any other state than STATE_USER_UNMANAGED and STATE_USER_SETUP_FINALIZED, we've
            // already run this method, so don't do anything.
            // STATE_USER_SETUP_FINALIZED can occur here if a managed profile is provisioned on a
            // device owner device.
            ProvisionLogger.logw("deviceManagementEstablished called, but state is not finalized "
                    + "or unmanaged");
            return;
        }

        mUserProvisioningStateHelper.markUserProvisioningStateInitiallyDone(params);
        if (ACTION_PROVISION_MANAGED_PROFILE.equals(params.provisioningAction)) {
            if (params.isOrganizationOwnedProvisioning) {
                markIsProfileOwnerOnOrganizationOwnedDevice();
                restrictRemovalOfManagedProfile();
            }
            if (!mSettingsFacade.isDuringSetupWizard(mActivity)) {
                // If a managed profile was provisioned after SUW, notify the DPC straight away.
                mSendDpcBroadcastServiceUtils.startSendDpcBroadcastService(mActivity, params);
            }
        }
        if (mSettingsFacade.isDuringSetupWizard(mActivity)) {
            // Store the information and wait for provisioningFinalized to be called
            storeProvisioningParams(params);
        }
    }

    private void restrictRemovalOfManagedProfile() {
        final UserManager userManager = UserManager.get(mActivity);
        userManager.setUserRestriction(UserManager.DISALLOW_REMOVE_MANAGED_PROFILE, true);
    }

    private void markIsProfileOwnerOnOrganizationOwnedDevice() {
        final DevicePolicyManager dpm = mActivity.getSystemService(DevicePolicyManager.class);
        final int managedProfileUserId = mUtils.getManagedProfile(mActivity).getIdentifier();
        final ComponentName admin = dpm.getProfileOwnerAsUser(managedProfileUserId);
        if (admin != null) {
            try {
                final Context profileContext = mActivity.createPackageContextAsUser(
                        mActivity.getPackageName(), 0 /* flags */,
                        UserHandle.of(managedProfileUserId));
                final DevicePolicyManager profileDpm =
                        profileContext.getSystemService(DevicePolicyManager.class);
                profileDpm.markProfileOwnerOnOrganizationOwnedDevice(admin);
            } catch (NameNotFoundException e) {
                ProvisionLogger.logw("Error setting access to Device IDs: " + e.getMessage());
            }
        }
    }

    private void storeProvisioningParams(ProvisioningParams params) {
        params.save(mProvisioningParamsUtils.getProvisioningParamsFile(mActivity));
    }
}
