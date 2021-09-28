/*
 * Copyright 2016, The Android Open Source Project
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

import static android.app.admin.DeviceAdminReceiver.ACTION_PROFILE_PROVISIONING_COMPLETE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISIONING_SUCCESSFUL;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE;
import static android.app.admin.DevicePolicyManager.PROVISIONING_MODE_MANAGED_PROFILE;

import static com.android.managedprovisioning.TestUtils.createTestAdminExtras;
import static com.android.managedprovisioning.finalization.SendDpcBroadcastService.EXTRA_PROVISIONING_PARAMS;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doCallRealMethod;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import android.content.pm.PackageManager;
import android.os.PersistableBundle;
import android.os.UserHandle;
import android.os.UserManager;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.managedprovisioning.TestUtils;
import com.android.managedprovisioning.analytics.DeferredMetricsReader;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.NotificationHelper;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import static com.google.common.truth.Truth.assertThat;

/**
 * Unit tests for {@link FinalizationPostSuwControllerLogic}.
 */
public class FinalizationPostSuwControllerTest extends AndroidTestCase {
    private static final UserHandle MANAGED_PROFILE_USER_HANDLE = UserHandle.of(123);
    private static final String TEST_MDM_PACKAGE_NAME = "mdm.package.name";
    private static final String TEST_MDM_ADMIN_RECEIVER = TEST_MDM_PACKAGE_NAME + ".AdminReceiver";
    private static final ComponentName TEST_MDM_ADMIN = new ComponentName(TEST_MDM_PACKAGE_NAME,
            TEST_MDM_ADMIN_RECEIVER);
    private static final PersistableBundle TEST_MDM_EXTRA_BUNDLE = createTestAdminExtras();
    private static final Account TEST_ACCOUNT = new Account("test@account.com", "account.type");

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

        final ProvisioningParamsUtils provisioningParamsUtils = new ProvisioningParamsUtils();
        mPreFinalizationController = new PreFinalizationController(
                mActivity, mUtils, mSettingsFacade, mHelper,
                provisioningParamsUtils, new SendDpcBroadcastServiceUtils());
        mFinalizationController = new FinalizationController(
                mActivity,
                new FinalizationPostSuwControllerLogic(
                        mActivity, mUtils, new SendDpcBroadcastServiceUtils(),
                        mProvisioningAnalyticsTracker),
                mUtils, mSettingsFacade, mHelper, mNotificationHelper, mDeferredMetricsReader,
                provisioningParamsUtils);
    }

    @Override
    public void tearDown() throws Exception {
        mFinalizationController.clearParamsFile();
    }

    @SmallTest
    public void testInitiallyDone_alreadyCalled() {
        // GIVEN that deviceManagementEstablished has already been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_PROFILE, false);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN nothing should happen
        verify(mHelper, never()).markUserProvisioningStateInitiallyDone(params);
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);
        verifyZeroInteractions(mDeferredMetricsReader);
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
    public void testManagedProfileAfterSuw() {
        // GIVEN that deviceManagementEstablished has never been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        // GIVEN that we've provisioned a managed profile after SUW
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_PROFILE, true);
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(true);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(false);
        when(mUtils.getManagedProfile(mActivity))
                .thenReturn(MANAGED_PROFILE_USER_HANDLE);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN the user provisioning state should be marked as initially done
        verify(mHelper).markUserProvisioningStateInitiallyDone(params);

        // THEN the service which starts the DPC is started.
        verifySendDpcServiceStarted(true);

        // THEN deferred metrics are not written
        verifyZeroInteractions(mDeferredMetricsReader);
    }

    @SmallTest
    public void testManagedProfileDuringSuw() {
        // GIVEN that deviceManagementEstablished has never been called
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(true);
        // GIVEN that we've provisioned a managed profile after SUW
        final ProvisioningParams params = createProvisioningParams(
                ACTION_PROVISION_MANAGED_PROFILE, true);
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(false);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(true);
        when(mUtils.getManagedProfile(mActivity))
                .thenReturn(MANAGED_PROFILE_USER_HANDLE);

        // WHEN calling deviceManagementEstablished
        mPreFinalizationController.deviceManagementEstablished(params);

        // THEN the user provisioning state should be marked as initially done
        verify(mHelper).markUserProvisioningStateInitiallyDone(params);
        // THEN the provisioning params have been stored and will be read in provisioningFinalized

        // GIVEN that SUW is now complete
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(true);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(false);

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // THEN the service which starts the DPC, is started.
        verifySendDpcServiceStarted(true);

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics are written
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);
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

        // THEN the managed profile DPC has been granted access to device IDs.
        verify(mockProfileDpm).markProfileOwnerOnOrganizationOwnedDevice(TEST_MDM_ADMIN);

        // THEN the user restriction for removing a managed profile has been applied.
        verify(mockUserManager).setUserRestriction(UserManager.DISALLOW_REMOVE_MANAGED_PROFILE,
                true);

        // GIVEN that SUW is now complete
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(true);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(false);

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN the user provisioning state is not yet finalized
        verify(mHelper, never()).markUserProvisioningStateFinalized(params);

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics are written
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);

        // THEN account migration is triggered
        verify(mUtils).removeAccountAsync(eq(mActivity), eq(TEST_ACCOUNT), any());
    }

    @SmallTest
    public void testDeviceOwner() {
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

        // GIVEN that SUW is now complete
        when(mSettingsFacade.isUserSetupCompleted(mActivity)).thenReturn(true);
        when(mSettingsFacade.isDuringSetupWizard(mActivity)).thenReturn(false);

        // GIVEN that the provisioning state is now incomplete
        when(mHelper.isStateUnmanagedOrFinalized()).thenReturn(false);

        // WHEN calling provisioningFinalized
        mFinalizationController.provisioningFinalized();

        // THEN provisioning successful intent should be sent to the dpc.
        verifyDpcLaunchedForUser(UserHandle.of(UserHandle.myUserId()));

        // WHEN the provisioning state changes are now committed
        mFinalizationController.commitFinalizedState();

        // THEN deferred metrics are written
        verify(mDeferredMetricsReader).scheduleDumpMetrics(any(Context.class));
        verifyNoMoreInteractions(mDeferredMetricsReader);

        // THEN the user provisioning state is finalized
        verify(mHelper).markUserProvisioningStateFinalized(params);

        // THEN a broadcast was sent to the primary user
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).sendBroadcast(intentCaptor.capture());

        verify(mNotificationHelper).showPrivacyReminderNotification(eq(mActivity), anyInt());

        // THEN the intent should be ACTION_PROFILE_PROVISIONING_COMPLETE
        assertEquals(ACTION_PROFILE_PROVISIONING_COMPLETE, intentCaptor.getValue().getAction());
        // THEN the intent should be sent to the admin receiver
        assertEquals(TEST_MDM_ADMIN, intentCaptor.getValue().getComponent());
        // THEN the admin extras bundle should contain mdm extras
        assertExtras(intentCaptor.getValue());
    }

    private void verifyDpcLaunchedForUser(UserHandle userHandle) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).startActivityAsUser(intentCaptor.capture(), eq(userHandle));
        final String intentAction = intentCaptor.getValue().getAction();
        // THEN the intent should be ACTION_PROVISIONING_SUCCESSFUL
        assertEquals(ACTION_PROVISIONING_SUCCESSFUL, intentAction);
        // THEN the intent should only be sent to the dpc
        assertEquals(TEST_MDM_PACKAGE_NAME, intentCaptor.getValue().getPackage());
        // THEN the admin extras bundle should contain mdm extras
        assertExtras(intentCaptor.getValue());
        // THEN a metric should be logged
        verify(mProvisioningAnalyticsTracker).logDpcSetupStarted(eq(mActivity), eq(intentAction));
    }

    private void verifySendDpcServiceStarted(boolean migrateAccount) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).startService(intentCaptor.capture());
        // THEN the intent should launch the SendDpcBroadcastService
        assertEquals(SendDpcBroadcastService.class.getName(),
                intentCaptor.getValue().getComponent().getClassName());
        // THEN the service extras should contain mdm extras
        assertSendDpcBroadcastServiceParams(intentCaptor.getValue(), migrateAccount);
    }

    private void assertExtras(Intent intent) {
        assertTrue(TestUtils.bundleEquals(TEST_MDM_EXTRA_BUNDLE,
                (PersistableBundle) intent.getExtra(EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE)));
    }

    private void assertSendDpcBroadcastServiceParams(Intent intent, boolean migrateAccount) {
        final ProvisioningParams expectedParams =
                createProvisioningParams(ACTION_PROVISION_MANAGED_PROFILE, migrateAccount);
        final ProvisioningParams actualParams =
                intent.getParcelableExtra(EXTRA_PROVISIONING_PARAMS);
        assertThat(actualParams).isEqualTo(expectedParams);
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
}
