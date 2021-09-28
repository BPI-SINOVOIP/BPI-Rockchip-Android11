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

package com.android.managedprovisioning.common;

import static android.app.admin.DevicePolicyManager.ACTION_ADMIN_POLICY_COMPLIANCE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE;

import static com.android.managedprovisioning.TestUtils.createTestAdminExtras;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.os.UserHandle;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.managedprovisioning.TestUtils;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class StartDpcInsideSuwServiceConnectionTest extends AndroidTestCase {
    private static final String TEST_MDM_PACKAGE_NAME = "mdm.package.name";
    private static final String TEST_MDM_ADMIN_RECEIVER = TEST_MDM_PACKAGE_NAME + ".AdminReceiver";
    private static final ComponentName TEST_MDM_ADMIN = new ComponentName(TEST_MDM_PACKAGE_NAME,
            TEST_MDM_ADMIN_RECEIVER);
    private static final PersistableBundle TEST_MDM_EXTRA_BUNDLE = createTestAdminExtras();
    private static final String TEST_DPC_INTENT_CATEGORY = "test_category";
    private static final int TEST_REQUEST_CODE = 3;
    private static final String TEST_SUW_PACKAGE_NAME = "suw.package.name";
    private static final String TEST_SUW_SERVICE_CLASS = TEST_SUW_PACKAGE_NAME + ".TestService";
    private static final ComponentName TEST_SUW_COMPONENT_NAME = new ComponentName(
            TEST_SUW_PACKAGE_NAME, TEST_SUW_SERVICE_CLASS);

    @Mock private Activity mActivity;
    @Mock private Activity mRestoredActivity;
    @Mock private Utils mUtils;
    @Mock private ProvisioningAnalyticsTracker mProvisioningAnalyticsTracker;

    private StartDpcInsideSuwServiceConnection mStartDpcInsideSuwServiceConnection;
    private Runnable mDpcIntentSender;
    private ProvisioningParams mParams;

    @Override
    public void setUp() throws Exception {
        System.setProperty("dexmaker.dexcache", getContext().getCacheDir().toString());
        MockitoAnnotations.initMocks(this);
        when(mUtils.canResolveIntentAsUser(any(Context.class), any(Intent.class), anyInt()))
                .thenReturn(true);

        final PolicyComplianceUtils policyComplianceUtils = new PolicyComplianceUtils();
        mParams = new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(TEST_MDM_ADMIN)
                .setProvisioningAction(ACTION_PROVISION_MANAGED_DEVICE)
                .setAdminExtrasBundle(TEST_MDM_EXTRA_BUNDLE)
                .build();

        mStartDpcInsideSuwServiceConnection = new StartDpcInsideSuwServiceConnection();
        mDpcIntentSender = () ->
                policyComplianceUtils.startPolicyComplianceActivityForResultIfResolved(
                        mActivity, mParams, TEST_DPC_INTENT_CATEGORY, TEST_REQUEST_CODE, mUtils,
                        mProvisioningAnalyticsTracker);
    }

    @SmallTest
    public void testBindingSucceeds_serviceConnects() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is established
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN an intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_instanceStateSavedAndRestoredBeforeServiceConnected() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN saving state and calling unbind before a service connection was established
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // GIVEN that a restored activity can also bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we bind to the SUW NetworkInterceptService again
        verifyBindServiceCalled(mRestoredActivity, restoredServiceConnection);

        // WHEN connection to the NetworkInterceptService is now established
        restoredServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mRestoredActivity);

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mRestoredActivity).unbindService(eq(restoredServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_instanceStateSavedAndRestoredAfterServiceConnected() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is now established
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN saving state and calling unbind after a service connection was established
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // GIVEN that a restored activity can also bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we bind to the SUW NetworkInterceptService again
        verifyBindServiceCalled(mRestoredActivity, restoredServiceConnection);

        // WHEN connection to the NetworkInterceptService is now established
        restoredServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN no new intent is sent to the DPC
        verify(mRestoredActivity, never()).startActivityForResultAsUser(any(Intent.class), anyInt(),
                any(UserHandle.class));

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mRestoredActivity).unbindService(eq(restoredServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_instanceStateSavedAndRestoredAfterDpcFinished() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is now established
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // WHEN calling saveInstanceState() after we've unbound from the service
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);

        // GIVEN that a restored activity can also bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we do not bind to the SUW NetworkInterceptService again
        verify(mRestoredActivity, never()).bindService(any(Intent.class),
                eq(restoredServiceConnection), anyInt());

        // THEN no new intent is sent to the DPC
        verify(mRestoredActivity, never()).startActivityForResultAsUser(any(Intent.class), anyInt(),
                any(UserHandle.class));

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we do not unbind from the NetworkInterceptService
        verify(mRestoredActivity, never()).unbindService(eq(restoredServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_serviceConnectsAndDisconnects() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is established and then lost
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);
        mStartDpcInsideSuwServiceConnection.onServiceDisconnected(TEST_SUW_COMPONENT_NAME);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_serviceConnectsAndDisconnects_instanceStateSavedAndRestored() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is established and then lost
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);
        mStartDpcInsideSuwServiceConnection.onServiceDisconnected(TEST_SUW_COMPONENT_NAME);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN saving state and calling unbind after a service connection was established
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // GIVEN that a restored activity can also bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we bind to the SUW NetworkInterceptService again
        verifyBindServiceCalled(mRestoredActivity, restoredServiceConnection);

        // WHEN connection to the NetworkInterceptService is now established
        restoredServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN no new intent is sent to the DPC
        verify(mRestoredActivity, never()).startActivityForResultAsUser(any(Intent.class), anyInt(),
                any(UserHandle.class));

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mRestoredActivity).unbindService(eq(restoredServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_serviceConnectsTwice() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is established, lost, and then
        // re-established
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);
        mStartDpcInsideSuwServiceConnection.onServiceDisconnected(TEST_SUW_COMPONENT_NAME);
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_serviceDies() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN connection to the NetworkInterceptService is established, but then dies
        mStartDpcInsideSuwServiceConnection.onServiceConnected(TEST_SUW_COMPONENT_NAME, null);
        mStartDpcInsideSuwServiceConnection.onBindingDied(TEST_SUW_COMPONENT_NAME);

        // THEN only one intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_nullBinding() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN the NetworkInterceptService returns a null binding
        mStartDpcInsideSuwServiceConnection.onNullBinding(TEST_SUW_COMPONENT_NAME);

        // THEN an intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingSucceeds_nullBinding_instanceStateSavedAndRestored() {
        // GIVEN that we can bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(true);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // WHEN the NetworkInterceptService returns a null binding
        mStartDpcInsideSuwServiceConnection.onNullBinding(TEST_SUW_COMPONENT_NAME);

        // THEN an intent is sent to the DPC
        verifyDpcLaunched(mActivity);

        // WHEN saving state and calling unbind after the null binding
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mActivity).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // GIVEN that a restored activity can bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we bind to the SUW NetworkInterceptService again
        verifyBindServiceCalled(mRestoredActivity, restoredServiceConnection);

        // WHEN the NetworkInterceptService returns a null binding again
        restoredServiceConnection.onNullBinding(TEST_SUW_COMPONENT_NAME);

        // THEN no new intent is sent to the DPC
        verify(mRestoredActivity, never()).startActivityForResultAsUser(any(Intent.class), anyInt(),
                any(UserHandle.class));

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we unbind from the NetworkInterceptService
        verify(mRestoredActivity).unbindService(eq(restoredServiceConnection));
    }

    @SmallTest
    public void testBindingFails() {
        // GIVEN that we can't bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(false);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we attempt to bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // THEN an intent is sent to the DPC, even though the service binding failed
        verifyDpcLaunched(mActivity);

        // WHEN calling dpcFinished and unbind
        mStartDpcInsideSuwServiceConnection.dpcFinished();
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we don't unbind from the NetworkInterceptService
        verify(mActivity, never()).unbindService(eq(mStartDpcInsideSuwServiceConnection));
    }

    @SmallTest
    public void testBindingFails_instanceStateSavedAndRestored() {
        // GIVEN that we can't bind to the SUW NetworkInterceptService
        when(mActivity.bindService(any(Intent.class), any(ServiceConnection.class), anyInt()))
                .thenReturn(false);

        // WHEN calling triggerDpcStart()
        mStartDpcInsideSuwServiceConnection.triggerDpcStart(mActivity, mDpcIntentSender);

        // THEN we attempt to bind to the SUW NetworkInterceptService
        verifyBindServiceCalled(mActivity, mStartDpcInsideSuwServiceConnection);

        // THEN an intent is sent to the DPC, even though the service binding failed
        verifyDpcLaunched(mActivity);

        // WHEN saving state and calling unbind after the binding failed
        final Bundle savedInstanceState = new Bundle();
        mStartDpcInsideSuwServiceConnection.saveInstanceState(savedInstanceState);
        mStartDpcInsideSuwServiceConnection.unbind(mActivity);

        // THEN we do not unbind from the NetworkInterceptService
        verify(mActivity, never()).unbindService(eq(mStartDpcInsideSuwServiceConnection));

        // GIVEN that a restored activity could now bind to the SUW NetworkInterceptService
        when(mRestoredActivity.bindService(
                any(Intent.class), any(ServiceConnection.class), anyInt())).thenReturn(true);

        // WHEN we restore the service connection from the saved state
        final StartDpcInsideSuwServiceConnection restoredServiceConnection =
                getRestoredServiceConnection(savedInstanceState);

        // THEN we still do not bind to the SUW NetworkInterceptService
        verify(mRestoredActivity, never()).bindService(any(Intent.class),
                eq(restoredServiceConnection), anyInt());

        // THEN no new intent is sent to the DPC
        verify(mRestoredActivity, never()).startActivityForResultAsUser(any(Intent.class), anyInt(),
                any(UserHandle.class));

        // WHEN calling dpcFinished and unbind
        restoredServiceConnection.dpcFinished();
        restoredServiceConnection.unbind(mRestoredActivity);

        // THEN we do not unbind from the NetworkInterceptService
        verify(mRestoredActivity, never()).unbindService(eq(restoredServiceConnection));
    }

    private void verifyDpcLaunched(Activity activity) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(activity).startActivityForResultAsUser(intentCaptor.capture(), anyInt(),
                any(UserHandle.class));
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
        verify(mProvisioningAnalyticsTracker).logDpcSetupStarted(eq(activity), eq(intentAction));
    }

    private void verifyBindServiceCalled(Activity activity,
            StartDpcInsideSuwServiceConnection serviceConnection) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(activity).bindService(intentCaptor.capture(), eq(serviceConnection), anyInt());
        // THEN the intent should be NETWORK_INTERCEPT_SERVICE_ACTION
        assertEquals(StartDpcInsideSuwServiceConnection.NETWORK_INTERCEPT_SERVICE_ACTION,
                intentCaptor.getValue().getAction());
        // THEN the intent should be sent to Setup Wizard
        assertEquals(StartDpcInsideSuwServiceConnection.SETUP_WIZARD_PACKAGE_NAME,
                intentCaptor.getValue().getPackage());
    }

    private void assertExtras(Intent intent) {
        assertTrue(TestUtils.bundleEquals(TEST_MDM_EXTRA_BUNDLE,
                (PersistableBundle) intent.getExtra(EXTRA_PROVISIONING_ADMIN_EXTRAS_BUNDLE)));
    }

    private StartDpcInsideSuwServiceConnection getRestoredServiceConnection(
            Bundle savedInstanceState) {
        final PolicyComplianceUtils policyComplianceUtils = new PolicyComplianceUtils();
        final Runnable dpcIntentSenderForRestoredActivity = () ->
                policyComplianceUtils.startPolicyComplianceActivityForResultIfResolved(
                        mRestoredActivity, mParams, TEST_DPC_INTENT_CATEGORY, TEST_REQUEST_CODE,
                        mUtils, mProvisioningAnalyticsTracker);

        return new StartDpcInsideSuwServiceConnection(mRestoredActivity, savedInstanceState,
                dpcIntentSenderForRestoredActivity);
    }
}
