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

package com.android.car.settings.applications;

import static com.android.car.settings.applications.ApplicationActionButtonsPreferenceController.DISABLE_CONFIRM_DIALOG_TAG;
import static com.android.car.settings.applications.ApplicationActionButtonsPreferenceController.FORCE_STOP_CONFIRM_DIALOG_TAG;
import static com.android.car.settings.applications.ApplicationActionButtonsPreferenceController.UNINSTALL_REQUEST_CODE;
import static com.android.car.settings.common.ActionButtonsPreference.ActionButtons;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.Manifest;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.admin.DevicePolicyManager;
import android.app.role.RoleManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.content.res.Resources;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.lifecycle.LifecycleOwner;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.common.ActionButtonInfo;
import com.android.car.settings.common.ActionButtonsPreference;
import com.android.car.settings.common.ActivityResultCallback;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceControllerTestUtil;
import com.android.car.settings.testutils.ResourceTestUtils;
import com.android.car.settings.testutils.TestLifecycleOwner;
import com.android.settingslib.applications.ApplicationsState;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class ApplicationActionButtonsPreferenceControllerTest {

    private static final String PACKAGE_NAME = "Test Package Name";

    private Context mContext = spy(ApplicationProvider.getApplicationContext());
    private LifecycleOwner mLifecycleOwner;
    private ActionButtonsPreference mActionButtonsPreference;
    private ApplicationActionButtonsPreferenceController mPreferenceController;
    private CarUxRestrictions mCarUxRestrictions;
    private ApplicationInfo mApplicationInfo;
    private PackageInfo mPackageInfo;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private ApplicationsState mMockAppState;
    @Mock
    private ApplicationsState.AppEntry mMockAppEntry;
    @Mock
    private DevicePolicyManager mMockDpm;
    @Mock
    private PackageManager mMockPm;
    @Mock
    private ActivityManager mMockActivityManager;
    @Mock
    private UserManager mMockUserManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLifecycleOwner = new TestLifecycleOwner();
        setMocks();

        mCarUxRestrictions = new CarUxRestrictions.Builder(/* reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

        mActionButtonsPreference = new ActionButtonsPreference(mContext);
        mPreferenceController = new ApplicationActionButtonsPreferenceController(mContext,
                /* preferenceKey= */ "key", mFragmentController, mCarUxRestrictions);
    }

    @Test
    public void testCheckInitialized_noAppState_throwException() {
        mPreferenceController.setAppEntry(mMockAppEntry).setPackageName(PACKAGE_NAME);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void testCheckInitialized_noAppEntry_throwException() {
        mPreferenceController.setAppState(mMockAppState).setPackageName(PACKAGE_NAME);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void testCheckInitialized_noPackageNameEntry_throwException() {
        mPreferenceController.setAppEntry(mMockAppEntry).setAppState(mMockAppState);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void onCreate_bundledApp_enabled_showDisableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "disable_text"));
    }

    @Test
    public void onCreate_bundledApp_disabled_showEnableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ false, /* system= */ true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "enable_text"));
    }

    @Test
    public void onCreate_bundledApp_enabled_disableUntilUsed_showsEnableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);
        mApplicationInfo.enabledSetting =
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED;
        mMockAppEntry.info = mApplicationInfo;

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "enable_text"));
    }

    @Test
    public void onCreate_bundledApp_homePackage_disablesDisableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);

        ResolveInfo homeActivity = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE_NAME;
        homeActivity.activityInfo = activityInfo;

        when(mMockPm.getHomeActivities(anyList())).then(invocation -> {
            Object[] args = invocation.getArguments();
            ((List<ResolveInfo>) args[0]).add(homeActivity);
            return null;
        });

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_bundledApp_systemPackage_disablesDisableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);
        mPackageInfo.signatures = new Signature[]{new Signature(PACKAGE_NAME.getBytes())};

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_bundledApp_enabledApp_keepEnabledPackage_disablesDisableButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);

        ResolveInfo phoneActivity = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE_NAME;
        activityInfo.permission = Manifest.permission.BROADCAST_SMS;
        phoneActivity.activityInfo = activityInfo;

        RoleManager mockRoleManager = mock(RoleManager.class);
        when(mContext.getSystemService(RoleManager.class)).thenReturn(mockRoleManager);
        when(mockRoleManager.getRoleHoldersAsUser(eq(RoleManager.ROLE_DIALER),
                any(UserHandle.class))).thenReturn(Collections.singletonList(PACKAGE_NAME));

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getDisableButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_notSystemApp_showUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "uninstall_text"));
    }

    @Test
    public void onCreate_packageHasActiveAdmins_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        when(mMockDpm.packageHasActiveAdmins(PACKAGE_NAME)).thenReturn(true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_deviceProvisioningPackage_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        Resources resources = mock(Resources.class);
        when(mContext.getResources()).thenReturn(resources);
        when(resources.getString(com.android.internal.R.string.config_deviceProvisioningPackage))
                .thenReturn(PACKAGE_NAME);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onStart_uninstallQueued_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        when(mMockDpm.isUninstallInQueue(PACKAGE_NAME)).thenReturn(true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onStart_noDefaultHome_onlyHomeApp_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        ResolveInfo homeActivity = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE_NAME;
        homeActivity.activityInfo = activityInfo;

        when(mMockPm.getHomeActivities(anyList())).then(invocation -> {
            Object[] args = invocation.getArguments();
            ((List<ResolveInfo>) args[0]).add(homeActivity);
            return null;
        });

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onStart_noDefaultHome_multipleHomeApps_enablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        ResolveInfo homeActivity = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE_NAME;
        homeActivity.activityInfo = activityInfo;

        ResolveInfo altHomeActivity = new ResolveInfo();
        ActivityInfo altActivityInfo = new ActivityInfo();
        altActivityInfo.packageName = PACKAGE_NAME + ".Someotherhome";
        altHomeActivity.activityInfo = altActivityInfo;

        when(mMockPm.getHomeActivities(anyList())).then(invocation -> {
            Object[] args = invocation.getArguments();
            ((List<ResolveInfo>) args[0]).addAll(Arrays.asList(homeActivity, altHomeActivity));
            return null;
        });

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isTrue();
    }

    @Test
    public void onStart_defaultHomeApp_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        ResolveInfo homeActivity = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = PACKAGE_NAME;
        homeActivity.activityInfo = activityInfo;

        ResolveInfo altHomeActivity = new ResolveInfo();
        ActivityInfo altActivityInfo = new ActivityInfo();
        altActivityInfo.packageName = PACKAGE_NAME + ".Someotherhome";
        altHomeActivity.activityInfo = altActivityInfo;

        when(mMockPm.getHomeActivities(anyList())).then(invocation -> {
            Object[] args = invocation.getArguments();
            ((List<ResolveInfo>) args[0]).addAll(Arrays.asList(homeActivity, altHomeActivity));
            return new ComponentName(PACKAGE_NAME, "SomeClass");
        });

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onStart_appsControlUserRestriction_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);
        when(mMockUserManager.hasUserRestriction(UserManager.DISALLOW_APPS_CONTROL)).thenReturn(
                true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onStart_uninstallAppsRestriction_disablesUninstallButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);
        when(mMockUserManager.hasUserRestriction(UserManager.DISALLOW_UNINSTALL_APPS)).thenReturn(
                true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getUninstallButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_forceStopButtonInitialized() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getForceStopButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "force_stop"));
    }

    @Test
    public void onCreate_notStopped_enablesForceStopButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getForceStopButton().isEnabled()).isTrue();
    }

    @Test
    public void onCreate_stopped_disablesForceStopButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ true, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getForceStopButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_packageHasActiveAdmins_disablesForceStopButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);
        when(mMockDpm.packageHasActiveAdmins(PACKAGE_NAME)).thenReturn(true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getForceStopButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_appsControlUserRestriction_disablesForceStopButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);
        when(mMockUserManager.hasUserRestriction(UserManager.DISALLOW_APPS_CONTROL)).thenReturn(
                true);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getForceStopButton().isEnabled()).isFalse();
    }

    @Test
    public void onCreate_packageExplicitlyStopped_queriesPackageRestart() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ true, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mContext).sendOrderedBroadcastAsUser(any(Intent.class),
                eq(UserHandle.CURRENT),
                /* receiverPermission= */ isNull(),
                any(BroadcastReceiver.class),
                /* scheduler= */ isNull(),
                eq(Activity.RESULT_CANCELED),
                /* initialData= */ isNull(),
                /* initialExtras= */ isNull());
    }

    @Test
    public void onCreate_packageExplicitlyStopped_success_enablesForceStopButton() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ true, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        mPreferenceController.mCheckKillProcessesReceiver.setPendingResult(
                new BroadcastReceiver.PendingResult(Activity.RESULT_OK,
                        "resultData",
                        /* resultExtras= */ null,
                        BroadcastReceiver.PendingResult.TYPE_UNREGISTERED,
                        /* ordered= */ true,
                        /* sticky= */ false,
                        /* token= */ null,
                        UserHandle.myUserId(),
                        /* flags= */ 0));

        mPreferenceController.mCheckKillProcessesReceiver.onReceive(mContext, /* intent= */ null);

        assertThat(getForceStopButton().isEnabled()).isTrue();
    }

    @Test
    public void onCreate_packageExplicitlyStopped_failure_disablesForceStopButton() {

        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ true, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        mPreferenceController.mCheckKillProcessesReceiver.setPendingResult(
                new BroadcastReceiver.PendingResult(Activity.RESULT_CANCELED,
                        "resultData",
                        /* resultExtras= */ null,
                        BroadcastReceiver.PendingResult.TYPE_UNREGISTERED,
                        /* ordered= */ true,
                        /* sticky= */ false,
                        /* token= */ null,
                        UserHandle.myUserId(),
                        /* flags= */ 0));

        mPreferenceController.mCheckKillProcessesReceiver.onReceive(mContext, /* intent= */ null);

        assertThat(getForceStopButton().isEnabled()).isFalse();
    }

    @Test
    public void forceStopClicked_showsDialog() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        getForceStopButton().getOnClickListener().onClick(/* view= */ null);

        verify(mFragmentController).showDialog(any(ConfirmationDialogFragment.class),
                eq(FORCE_STOP_CONFIRM_DIALOG_TAG));
    }

    @Test
    public void forceStopDialogConfirmed_forceStopsPackage() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        mPreferenceController.mForceStopConfirmListener.onConfirm(/* arguments= */ null);

        verify(mMockActivityManager).forceStopPackage(PACKAGE_NAME);
        verify(mMockAppState).invalidatePackage(eq(PACKAGE_NAME), anyInt());
    }

    @Test
    public void disableClicked_showsDialog() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);

        mPreferenceController.onCreate(mLifecycleOwner);

        getDisableButton().getOnClickListener().onClick(/* view= */ null);

        verify(mFragmentController).showDialog(any(ConfirmationDialogFragment.class),
                eq(DISABLE_CONFIRM_DIALOG_TAG));
    }


    @Test
    public void disableDialogConfirmed_disablesPackage() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ true);

        mPreferenceController.onCreate(mLifecycleOwner);

        mPreferenceController.mDisableConfirmListener.onConfirm(/* arguments= */ null);

        verify(mMockPm).setApplicationEnabledSetting(PACKAGE_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED_USER, /* flags= */ 0);
    }

    @Test
    public void enableClicked_enablesPackage() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ false, /* system= */ true);

        mPreferenceController.onCreate(mLifecycleOwner);

        getDisableButton().getOnClickListener().onClick(/* view= */ null);

        verify(mMockPm).setApplicationEnabledSetting(PACKAGE_NAME,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, /* flags= */ 0);
    }

    @Test
    public void uninstallClicked_startsUninstallActivity() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);

        getUninstallButton().getOnClickListener().onClick(/* view= */ null);

        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(
                Intent.class);

        verify(mFragmentController).startActivityForResult(intentArgumentCaptor.capture(),
                eq(UNINSTALL_REQUEST_CODE), any(ActivityResultCallback.class));

        Intent intent = intentArgumentCaptor.getValue();
        assertThat(intent.getAction()).isEqualTo(Intent.ACTION_UNINSTALL_PACKAGE);
        assertThat(intent.getBooleanExtra(Intent.EXTRA_RETURN_RESULT, false)).isTrue();
        assertThat(intent.getData().toString()).isEqualTo("package:" + PACKAGE_NAME);
    }

    @Test
    public void processActivityResult_uninstall_resultOk_goesBack() {
        setupAndAssignPreference();
        setApplicationInfo(/* stopped= */ false, /* enabled= */ true, /* system= */ false);

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.processActivityResult(UNINSTALL_REQUEST_CODE,
                Activity.RESULT_OK, /* data= */ null);

        verify(mFragmentController).goBack();
    }

    private void setMocks() {
        mPackageInfo = new PackageInfo();
        mPackageInfo.packageName = PACKAGE_NAME;

        when(mMockAppState.getEntry(eq(PACKAGE_NAME), anyInt())).thenReturn(mMockAppEntry);
        when(mContext.getPackageManager()).thenReturn(mMockPm);
        when(mContext.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mMockActivityManager);
        when(mContext.getSystemService(Context.DEVICE_POLICY_SERVICE)).thenReturn(mMockDpm);
        when(mContext.getSystemService(Context.USER_SERVICE)).thenReturn(mMockUserManager);

        PackageInfo systemPackage = new PackageInfo();
        systemPackage.packageName = "android";
        systemPackage.signatures = new Signature[]{new Signature(PACKAGE_NAME.getBytes())};
        ApplicationInfo systemApplicationInfo = new ApplicationInfo();
        systemApplicationInfo.packageName = "android";
        systemPackage.applicationInfo = systemApplicationInfo;
        try {
            when(mMockPm.getPackageInfo(eq(PACKAGE_NAME), anyInt())).thenReturn(mPackageInfo);
            when(mMockPm.getPackageInfo("android", PackageManager.GET_SIGNATURES))
                    .thenReturn(systemPackage);
        } catch (PackageManager.NameNotFoundException e) {
            // no-op - don't catch exception inside test
        }
    }

    private void setupAndAssignPreference() {
        mPreferenceController.setAppEntry(mMockAppEntry).setAppState(mMockAppState).setPackageName(
                PACKAGE_NAME);
        PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                mActionButtonsPreference);
    }

    private void setApplicationInfo(boolean stopped, boolean enabled, boolean system) {
        mApplicationInfo = new ApplicationInfo();
        if (stopped) {
            mApplicationInfo.flags |= ApplicationInfo.FLAG_STOPPED;
        }
        if (!enabled) {
            mApplicationInfo.enabled = false;
        }
        if (system) {
            mApplicationInfo.flags |= ApplicationInfo.FLAG_SYSTEM;
        }
        mMockAppEntry.info = mApplicationInfo;
    }

    private ActionButtonInfo getForceStopButton() {
        return mActionButtonsPreference.getButton(ActionButtons.BUTTON2);
    }

    private ActionButtonInfo getDisableButton() {
        // Same button is used with a different handler. This method is purely for readability.
        return getUninstallButton();
    }

    private ActionButtonInfo getUninstallButton() {
        return mActionButtonsPreference.getButton(ActionButtons.BUTTON1);
    }
}
