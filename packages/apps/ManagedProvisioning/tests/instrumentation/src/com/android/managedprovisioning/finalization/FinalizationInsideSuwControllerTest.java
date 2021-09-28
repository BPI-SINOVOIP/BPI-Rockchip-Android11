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

import static android.app.admin.DevicePolicyManager.ACTION_ADMIN_POLICY_COMPLIANCE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE;
import static android.app.admin.DevicePolicyManager.PROVISIONING_MODE_MANAGED_PROFILE;

import static com.android.managedprovisioning.TestUtils.createTestAdminExtras;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.os.UserHandle;
import android.os.UserManager;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.managedprovisioning.TestUtils;
import com.android.managedprovisioning.analytics.DeferredMetricsReader;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.NotificationHelper;
import com.android.managedprovisioning.common.PolicyComplianceUtils;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link FinalizationInsideSuwControllerLogic}.
 */
public class FinalizationInsideSuwControllerTest extends AndroidTestCase {
    private static final UserHandle MANAGED_PROFILE_USER_HANDLE = UserHandle.of(123);
    private static final String TEST_MDM_PACKAGE_NAME = "mdm.package.name";
    private static final String TEST_MDM_ADMIN_RECEIVER = TEST_MDM_PACKAGE_NAME + ".AdminReceiver";
    private static final ComponentName TEST_MDM_ADMIN = new ComponentName(TEST_MDM_PACKAGE_NAME,
            TEST_MDM_ADMIN_RECEIVER);
    private static final PersistableBundle TEST_MDM_EXTRA_BUNDLE = createTestAdminExtras();
    private static final Account TEST_ACCOUNT = new Account("test@account.com", "account.type");
    private static final String TEST_DPC_INTENT_CATEGORY = "test_category";
    private static final Intent ACTIVITY_INTENT =
            new Intent("android.app.action.PROVISION_FINALIZATION_INSIDE_SUW")
            .putExtra(FinalizationInsideSuwControllerLogic.EXTRA_DPC_INTENT_CATEGORY,
                    TEST_DPC_INTENT_CATEGORY);

    @Mock private Activity mActivity;
    @Mock private Utils mUtils;
    @Mock private SettingsFacade mSettingsFacade;
    @Mock private UserProvisioningStateHelper mHelper;
    @Mock private NotificationHelper mNotificationHelper;
    @Mock private DeferredMetricsReader mDeferredMetricsReader;
    @Mock private ProvisioningAnalyticsTracker mProvisioningAnalyticsTracker;

    private PreFinalizationController mPreFinalizationController;
    private FinalizationController mFinalizationController;

    @Override
    public void setUp() throws Exception {
        // this is necessary for mockito to work
        System.setProperty("dexmaker.dexcache", getContext().getCacheDir().toString());
        MockitoAnnotations.initMocks(this);
        when(mUtils.canResolveIntentAsUser(any(Context.class), any(Intent.class), anyInt()))
                .thenReturn(true);
        when(mActivity.getFilesDir()).thenReturn(getContext().getFilesDir());
        when(mActivity.getIntent()).thenReturn(ACTIVITY_INTENT);
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(false);

        final ProvisioningParamsUtils provisioningParamsUtils = new ProvisioningParamsUtils();
        mPreFinalizationController = new PreFinalizationController(
                mActivity, mUtils, mSettingsFacade, mHelper,
                provisioningParamsUtils, new SendDpcBroadcastServiceUtils());
        mFinalizationController = new FinalizationController(
                mActivity,
                new FinalizationInsideSuwControllerLogic(
                        mActivity, mUtils, new PolicyComplianceUtils(),
                        mProvisioningAnalyticsTracker),
                mUtils, mSettingsFacade, mHelper, mNotificationHelper, mDeferredMetricsReader,
                provisioningParamsUtils);
    }

    @Override
    public void tearDown() throws Exception {
        mFinalizationController.clearParamsFile();
    }

    @SmallTest
    public void testFinalized_alreadyCalled() {
        // GIVEN that deviceManagementEstablished has already been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_PROFILE, false);

        // WHEN calling provisioningFinalized and commitFinalizedState
        mFinalizationController.provisioningFinalized();
        mFinalizationController.commitFinalizedState();

        // THEN nothing should happen
        verify(mHelper, never()).markUserProvisioningStateInitiallyDone(params);
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);
        verifyZeroInteractions(mDeferredMetricsReader);
    }

    @SmallTest
    public void testFinalized_noParamsStored() {
        // GIVEN that the user provisioning state is correct
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN calling provisioningFinalized and commitFinalizedState
        mFinalizationController.provisioningFinalized();
        mFinalizationController.commitFinalizedState();

        // THEN nothing should happen
        verify(mHelper, never())
                .markUserProvisioningStateInitiallyDone(any(ProvisioningParams.class));
        verify(mHelper, never()).markUserProvisioningStateFinalized(any(ProvisioningParams.class));
        verifyZeroInteractions(mDeferredMetricsReader);
    }

    @SmallTest
    public void testManagedProfileFinalizationDuringSuw() {
        // GIVEN that deviceManagementEstablished has never been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        // GIVEN that we've provisioned a managed profile after SUW
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_PROFILE, true);
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(false);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(true);
        when(mUtils.getManagedProfile(mActivity)).thenReturn(MANAGED_PROFILE_USER_HANDLE);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN the user provisioning state should be marked as initially done
        verify(mHelper).markUserProvisioningStateInitiallyDone(params);
        // THEN the provisioning params have been stored and will be read in provisioningFinalized

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN we save and restore controller state
        saveAndRestoreControllerState();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN no intent should be sent to the dpc.
        verify(mActivity, never()).startActivityForResultAsUser(
                any(Intent.class), anyInt(), eq(MANAGED_PROFILE_USER_HANDLE));

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should be sent to the dpc.
        verifyDpcLaunchedForUser(MANAGED_PROFILE_USER_HANDLE, 1);

        // WHEN calling provisioningFinalized again
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

         // THEN intent should not be sent to the dpc again
        verifyDpcLaunchedForUser(MANAGED_PROFILE_USER_HANDLE, 1);

        // WHEN simulating a DPC cancel by calling activityDestroyed(true), and then
        // provisioningFinalized again
        mFinalizationController.activityDestroyed(true);
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should be sent to the dpc again
        verifyDpcLaunchedForUser(MANAGED_PROFILE_USER_HANDLE, 2);

        // WHEN we save and restore controller state, and then call provisioningFinalized again
        saveAndRestoreControllerState();
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent is not sent to the dpc again
        verifyDpcLaunchedForUser(MANAGED_PROFILE_USER_HANDLE, 2);

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics have been written exactly once
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);

        // THEN the service which starts the DPC, has never been started.
        verifySendDpcServiceNotStarted();

        // THEN account migration is triggered exactly once
        verify(mUtils).removeAccountAsync(eq(mActivity), eq(TEST_ACCOUNT), any());
    }

    @SmallTest
    public void testDeviceOwnerFinalizationDuringSuw() {
        // GIVEN that deviceManagementEstablished has never been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        // GIVEN that we've provisioned a device owner during SUW
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_DEVICE, false);
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(false);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(true);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN the user provisioning state should be marked as initially done
        verify(mHelper).markUserProvisioningStateInitiallyDone(params);
        // THEN the provisioning params have been stored and will be read in provisioningFinalized

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN we save and restore controller state
        saveAndRestoreControllerState();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN no intent should be sent to the dpc.
        verify(mActivity, never()).startActivityForResultAsUser(
                any(Intent.class), anyInt(), eq(UserHandle.of(UserHandle.myUserId())));

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should be sent to the dpc.
        verifyDpcLaunchedForUser(UserHandle.of(UserHandle.myUserId()), 1);

        // WHEN calling provisioningFinalized again
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should not be sent to the dpc again
        verifyDpcLaunchedForUser(UserHandle.of(UserHandle.myUserId()), 1);

        // WHEN simulating a DPC cancel by calling activityDestroyed(true), and then
        // provisioningFinalized again
        mFinalizationController.activityDestroyed(true);
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should be sent to the dpc again
        verifyDpcLaunchedForUser(UserHandle.of(UserHandle.myUserId()), 2);

        // WHEN we save and restore controller state, and then call provisioningFinalized again
        saveAndRestoreControllerState();
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN intent should not be sent to the dpc again
        verifyDpcLaunchedForUser(UserHandle.of(UserHandle.myUserId()), 2);

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics have been written exactly once
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);

        // THEN a privacy reminder is shown to the user exactly once
        verify(mNotificationHelper).showPrivacyReminderNotification(eq(mActivity), anyInt());

        // THEN no broadcast was ever sent to the primary user
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity, never()).sendBroadcast(intentCaptor.capture());
    }

    @SmallTest
    public void testCorpOwnedManagedProfileDuringSuw() throws PackageManager.NameNotFoundException {
        // GIVEN that deviceManagementEstablished has never been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        // GIVEN that we're provisioning a corp-owned managed profile DURING SUW
        final ProvisioningParams params =
                createProvisioningParamsBuilder(ACTION_PROVISION_MANAGED_PROFILE, true)
                        .setIsOrganizationOwnedProvisioning(true)
                        .setProvisioningMode(PROVISIONING_MODE_MANAGED_PROFILE)
                        .build();

        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(false);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(true);
        when(mUtils.getManagedProfile(mActivity))
                .thenReturn(MANAGED_PROFILE_USER_HANDLE);
        when(mUtils.isAdminIntegratedFlow(params)).thenCallRealMethod();

        // Mock DPM for testing access to device IDs is granted.
        final DevicePolicyManager mockDpm = mock(DevicePolicyManager.class);
        when(mActivity.getSystemServiceName(DevicePolicyManager.class))
                .thenReturn(Context.DEVICE_POLICY_SERVICE);
        when(mActivity.getSystemService(DevicePolicyManager.class)).thenReturn(mockDpm);
        final int managedProfileUserId = MANAGED_PROFILE_USER_HANDLE.getIdentifier();
        when(mockDpm.getProfileOwnerAsUser(managedProfileUserId)).thenReturn(TEST_MDM_ADMIN);

        // Actual Device IDs access is  granted to the DPM of the managed profile, in the context
        // of the managed profile.
        final Context profileContext = mock(Context.class);
        when(mActivity.createPackageContextAsUser(mActivity.getPackageName(), /*flags=*/ 0,
                MANAGED_PROFILE_USER_HANDLE)).thenReturn(profileContext);
        when(profileContext.getSystemServiceName(DevicePolicyManager.class))
                .thenReturn(Context.DEVICE_POLICY_SERVICE);
        final DevicePolicyManager mockProfileDpm = mock(DevicePolicyManager.class);
        when(profileContext.getSystemService(DevicePolicyManager.class)).thenReturn(mockProfileDpm);

        // Mock UserManager for testing user restriction.
        final UserManager mockUserManager = mock(UserManager.class);
        when(mActivity.getSystemServiceName(UserManager.class))
                .thenReturn(Context.USER_SERVICE);
        when(mActivity.getSystemService(Context.USER_SERVICE)).thenReturn(mockUserManager);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN the user provisioning state should be marked as initially done
        verify(mHelper).markUserProvisioningStateInitiallyDone(params);

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN no intent should be sent to the dpc.
        verify(mActivity, never()).startActivityForResultAsUser(
                any(Intent.class), anyInt(), eq(MANAGED_PROFILE_USER_HANDLE));

        // WHEN calling provisioningFinalized again
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN no intent should be sent to the dpc.
        verify(mActivity, never()).startActivityForResultAsUser(
                any(Intent.class), anyInt(), eq(MANAGED_PROFILE_USER_HANDLE));

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics are written exactly once
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);

        // THEN account migration is triggered exactly once
        verify(mUtils).removeAccountAsync(eq(mActivity), eq(TEST_ACCOUNT), any());
    }

    private void verifyDpcLaunchedForUser(UserHandle userHandle, int numTimes) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity, times(numTimes)).startActivityForResultAsUser(
                intentCaptor.capture(), anyInt(), eq(userHandle));
        final String intentAction = intentCaptor.getValue().getAction();
        // THEN the intent should be ACTION_PROVISIONING_SUCCESSFUL
        assertEquals(ACTION_ADMIN_POLICY_COMPLIANCE, intentAction);
        // THEN the intent should only be sent to the dpc
        assertEquals(TEST_MDM_PACKAGE_NAME, intentCaptor.getValue().getPackage());
        // THEN the admin extras bundle should contain mdm extras
        assertExtras(intentCaptor.getValue());
        // THEN the intent should have the category that was passed into the parent activity
        assertTrue(intentCaptor.getValue().hasCategory(TEST_DPC_INTENT_CATEGORY));
        // THEN a metric should be logged
        verify(mProvisioningAnalyticsTracker, times(numTimes)).logDpcSetupStarted(
                eq(mActivity), eq(intentAction));
    }

    private void verifySendDpcServiceNotStarted() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity, never()).startService(intentCaptor.capture());
    }

    private void assertExtras(Intent intent) {
        assertTrue(TestUtils.bundleEquals(TEST_MDM_EXTRA_BUNDLE,
                (PersistableBundle) intent.getExtra(EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE)));
    }

    private ProvisioningParams createProvisioningParams(String action, boolean migrateAccount) {
        return createProvisioningParamsBuilder(action, migrateAccount).build();
    }

    private ProvisioningParams.Builder createProvisioningParamsBuilder(String action,
            boolean migrateAccount) {
        ProvisioningParams.Builder builder = new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(TEST_MDM_ADMIN)
                .setProvisioningAction(action)
                .setAdminExtrasBundle(TEST_MDM_EXTRA_BUNDLE);

        if (migrateAccount) {
            builder.setAccountToMigrate(TEST_ACCOUNT);
            builder.setKeepAccountMigrated(false);
        }

        return builder;
    }

    private void saveAndRestoreControllerState() {
        final Bundle savedInstanceState = new Bundle();
        mFinalizationController.saveInstanceState(savedInstanceState);
        mFinalizationController.activityDestroyed(false);
        mFinalizationController.restoreInstanceState(savedInstanceState);
    }
}
