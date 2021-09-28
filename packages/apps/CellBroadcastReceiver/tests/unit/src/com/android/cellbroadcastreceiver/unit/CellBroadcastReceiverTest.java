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

package com.android.cellbroadcastreceiver.unit;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.ContentResolver;
import android.content.IContentProvider;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.media.AudioDeviceInfo;
import android.os.RemoteException;
import android.os.UserManager;
import android.provider.Telephony;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.cdma.CdmaSmsCbProgramData;

import com.android.cellbroadcastreceiver.CellBroadcastAlertService;
import com.android.cellbroadcastreceiver.CellBroadcastListActivity;
import com.android.cellbroadcastreceiver.CellBroadcastReceiver;
import com.android.cellbroadcastreceiver.CellBroadcastSettings;
import com.android.cellbroadcastreceiver.R;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

public class CellBroadcastReceiverTest extends CellBroadcastTest {
    private static final long MAX_INIT_WAIT_MS = 5000;

    CellBroadcastReceiver mCellBroadcastReceiver;
    String mPackageName = "testPackageName";

    @Mock
    UserManager mUserManager;
    @Mock
    SharedPreferences mSharedPreferences;
    @Mock
    private SharedPreferences.Editor mEditor;
    @Mock
    SharedPreferences mDefaultSharedPreferences;
    @Mock
    Intent mIntent;
    @Mock
    PackageManager mPackageManager;
    @Mock
    PackageInfo mPackageInfo;
    @Mock
    ContentResolver mContentResolver;
    @Mock
    IContentProvider mContentProviderClient;

    private Configuration mConfiguration = new Configuration();
    private AudioDeviceInfo[] mDevices = new AudioDeviceInfo[0];
    private Object mLock = new Object();
    private boolean mReady;

    protected void waitUntilReady() {
        synchronized (mLock) {
            if (!mReady) {
                try {
                    mLock.wait(MAX_INIT_WAIT_MS);
                } catch (InterruptedException ie) {
                }

                if (!mReady) {
                    Assert.fail("Telephony tests failed to initialize");
                }
            }
        }
    }

    protected void setReady(boolean ready) {
        synchronized (mLock) {
            mReady = ready;
            mLock.notifyAll();
        }
    }

    @Before
    public void setUp() throws Exception {
        super.setUp(this.getClass().getSimpleName());
        MockitoAnnotations.initMocks(this);
        doReturn(mConfiguration).when(mResources).getConfiguration();
        mCellBroadcastReceiver = spy(new CellBroadcastReceiver());
        doReturn(mResources).when(mCellBroadcastReceiver).getResourcesMethod();
        doNothing().when(mCellBroadcastReceiver).startConfigService();
        doReturn(mContext).when(mContext).getApplicationContext();
        doReturn(mPackageName).when(mContext).getPackageName();
    }

    @Test
    public void testOnReceive_actionCarrierConfigChanged() {
        doReturn(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED).when(mIntent).getAction();
        doNothing().when(mCellBroadcastReceiver).initializeSharedPreference();
        doNothing().when(mCellBroadcastReceiver).enableLauncher();
        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mCellBroadcastReceiver).initializeSharedPreference();
        verify(mCellBroadcastReceiver).startConfigService();
        verify(mCellBroadcastReceiver).enableLauncher();
    }

    @Test
    public void testOnReceive_cellbroadcastStartConfigAction() {
        doReturn(CellBroadcastReceiver.CELLBROADCAST_START_CONFIG_ACTION).when(mIntent).getAction();
        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mCellBroadcastReceiver, never()).initializeSharedPreference();
        verify(mCellBroadcastReceiver).startConfigService();
    }

    @Test
    public void testOnReceive_actionDefaultSmsSubscriptionChanged() {
        doReturn(SubscriptionManager.ACTION_DEFAULT_SMS_SUBSCRIPTION_CHANGED)
                .when(mIntent).getAction();
        doReturn(mUserManager).when(mContext).getSystemService(anyString());
        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mCellBroadcastReceiver, never()).initializeSharedPreference();
        verify(mCellBroadcastReceiver).startConfigService();
    }

    @Test
    public void testOnReceive_actionSmsEmergencyCbReceived() {
        doReturn(Telephony.Sms.Intents.ACTION_SMS_EMERGENCY_CB_RECEIVED).when(mIntent).getAction();
        doReturn(mIntent).when(mIntent).setClass(mContext, CellBroadcastAlertService.class);
        doReturn(null).when(mContext).startService(mIntent);

        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mIntent).setClass(mContext, CellBroadcastAlertService.class);
        verify(mContext).startService(mIntent);
    }

    @Test
    public void testOnReceive_smsCbReceivedAction() {
        doReturn(Telephony.Sms.Intents.SMS_CB_RECEIVED_ACTION).when(mIntent).getAction();
        doReturn(mIntent).when(mIntent).setClass(mContext, CellBroadcastAlertService.class);
        doReturn(null).when(mContext).startService(any());

        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mIntent).setClass(mContext, CellBroadcastAlertService.class);
        verify(mContext).startService(mIntent);
    }

    @Test
    public void testOnReceive_smsServiceCategoryProgramDataReceivedAction() {
        doReturn(Telephony.Sms.Intents.SMS_SERVICE_CATEGORY_PROGRAM_DATA_RECEIVED_ACTION)
                .when(mIntent).getAction();
        doReturn(null).when(mIntent).getParcelableArrayListExtra(anyString());

        mCellBroadcastReceiver.onReceive(mContext, mIntent);
        verify(mIntent).getParcelableArrayListExtra(anyString());
    }

    @Test
    public void testInitializeSharedPreference_ifSystemUser() {
        doReturn("An invalid action").when(mIntent).getAction();
        doReturn(true).when(mCellBroadcastReceiver).isSystemUser();
        doReturn(mDefaultSharedPreferences).when(mCellBroadcastReceiver)
                .getDefaultSharedPreferences();
        doReturn(true).when(mCellBroadcastReceiver).sharedPrefsHaveDefaultValues();
        doNothing().when(mCellBroadcastReceiver).adjustReminderInterval();

        mCellBroadcastReceiver.initializeSharedPreference();
        verify(mCellBroadcastReceiver).getDefaultSharedPreferences();
    }

    @Test
    public void testInitializeSharedPreference_ifNotSystemUser() {
        doReturn("An invalid action").when(mIntent).getAction();
        doReturn(false).when(mCellBroadcastReceiver).isSystemUser();

        mCellBroadcastReceiver.initializeSharedPreference();
        verify(mSharedPreferences, never()).getBoolean(anyString(), anyBoolean());
    }

    @Test
    public void testMigrateSharedPreferenceFromLegacyWhenNoLegacyProvider() {
        setContext();
        doReturn(mContentResolver).when(mContext).getContentResolver();
        doReturn(null).when(mContentResolver).acquireContentProviderClient(
                Telephony.CellBroadcasts.AUTHORITY_LEGACY);

        mCellBroadcastReceiver.migrateSharedPreferenceFromLegacy();
        verify(mContext, never()).getSharedPreferences(anyString(), anyInt());
    }

    @Test
    public void testMigrateSharedPreferenceFromLegacyWhenBundleNull() throws RemoteException {
        setContext();
        doReturn(mContentResolver).when(mContext).getContentResolver();
        doReturn(mContentProviderClient).when(mContentResolver).acquireContentProviderClient(
                Telephony.CellBroadcasts.AUTHORITY_LEGACY);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(null).when(mContentProviderClient).call(
                anyString(), anyString(), anyString(), any());
        doNothing().when(mEditor).apply();

        mCellBroadcastReceiver.migrateSharedPreferenceFromLegacy();
        verify(mContext).getSharedPreferences(anyString(), anyInt());
        verify(mEditor, never()).putBoolean(anyString(), anyBoolean());
    }

    @Test
    public void testSetTestingMode() {
        boolean isTestingMode = true;
        setContext();
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mEditor).when(mEditor).putBoolean(anyString(), anyBoolean());
        doReturn(true).when(mEditor).commit();

        mCellBroadcastReceiver.setTestingMode(isTestingMode);
        verify(mEditor).putBoolean(CellBroadcastReceiver.TESTING_MODE, isTestingMode);
    }

    @Test
    public void testIsTestingMode() {
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());

        mCellBroadcastReceiver.isTestingMode(mContext);
        verify(mSharedPreferences).getBoolean("testing_mode", false);
    }

    @Test
    public void testAdjustReminderInterval() {
        setContext();
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());
        doReturn("currentInterval").when(mSharedPreferences).getString(
                CellBroadcastReceiver.CURRENT_INTERVAL_DEFAULT, "0");
        doReturn(mResources).when(mContext).getResources();
        doReturn("newInterval").when(mResources).getString(
                R.string.alert_reminder_interval_in_min_default);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mEditor).when(mEditor).putBoolean(anyString(), anyBoolean());
        doReturn(true).when(mEditor).commit();

        mCellBroadcastReceiver.adjustReminderInterval();
        verify(mEditor).putString(CellBroadcastReceiver.CURRENT_INTERVAL_DEFAULT, "newInterval");
    }

    @Test
    public void testEnableLauncherIfNoLauncherActivity() throws
            PackageManager.NameNotFoundException {
        setContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(anyString(), anyInt());

        ActivityInfo activityInfo = new ActivityInfo();
        String activityInfoName = "";
        activityInfo.targetActivity = CellBroadcastListActivity.class.getName();
        activityInfo.name = activityInfoName;
        ActivityInfo[] activityInfos = new ActivityInfo[1];
        activityInfos[0] = activityInfo;
        mPackageInfo.activities = activityInfos;

        mCellBroadcastReceiver.enableLauncher();
        verify(mPackageManager, never()).setComponentEnabledSetting(any(), anyInt(), anyInt());
    }

    @Test
    public void testEnableLauncherIfEnableTrue() throws PackageManager.NameNotFoundException {
        setContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(anyString(), anyInt());
        doReturn(true).when(mResources)
                .getBoolean(R.bool.show_message_history_in_launcher);

        ActivityInfo activityInfo = new ActivityInfo();
        String activityInfoName = "testName";
        activityInfo.targetActivity = CellBroadcastListActivity.class.getName();
        activityInfo.name = activityInfoName;
        ActivityInfo[] activityInfos = new ActivityInfo[1];
        activityInfos[0] = activityInfo;
        mPackageInfo.activities = activityInfos;

        mCellBroadcastReceiver.enableLauncher();
        verify(mPackageManager).setComponentEnabledSetting(any(),
                eq(PackageManager.COMPONENT_ENABLED_STATE_ENABLED), anyInt());
    }

    @Test
    public void testEnableLauncherIfEnableFalse() throws PackageManager.NameNotFoundException {
        setContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(anyString(), anyInt());
        doReturn(false).when(mResources)
                .getBoolean(R.bool.show_message_history_in_launcher);

        ActivityInfo activityInfo = new ActivityInfo();
        String activityInfoName = "testName";
        activityInfo.targetActivity = CellBroadcastListActivity.class.getName();
        activityInfo.name = activityInfoName;
        ActivityInfo[] activityInfos = new ActivityInfo[1];
        activityInfos[0] = activityInfo;
        mPackageInfo.activities = activityInfos;

        mCellBroadcastReceiver.enableLauncher();
        verify(mPackageManager).setComponentEnabledSetting(any(),
                eq(PackageManager.COMPONENT_ENABLED_STATE_DISABLED), anyInt());
    }

    @Test
    public void testTryCdmaSetCatergory() {
        boolean enable = true;
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mEditor).when(mEditor).putBoolean(anyString(), anyBoolean());

        mCellBroadcastReceiver.tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT, enable);
        verify(mEditor).putBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_EXTREME_THREAT_ALERTS, enable);

        mCellBroadcastReceiver.tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT, enable);
        verify(mEditor).putBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_SEVERE_THREAT_ALERTS, enable);

        mCellBroadcastReceiver.tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY, enable);
        verify(mEditor).putBoolean(
                CellBroadcastSettings.KEY_ENABLE_CMAS_AMBER_ALERTS, enable);

        mCellBroadcastReceiver.tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE, enable);
        verify(mEditor).putBoolean(
                CellBroadcastSettings.KEY_ENABLE_TEST_ALERTS, enable);
    }

    @Test
    public void testHandleCdmaSmsCbProgramDataOperationAddAndDelete() {
        setContext();
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mEditor).when(mEditor).putBoolean(anyString(), anyBoolean());

        CdmaSmsCbProgramData programData = new CdmaSmsCbProgramData(
                CdmaSmsCbProgramData.OPERATION_ADD_CATEGORY,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT,
                1, 1, 1, "catergoryName");
        mCellBroadcastReceiver.handleCdmaSmsCbProgramData(new ArrayList<>(List.of(programData)));
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT, true);

        programData = new CdmaSmsCbProgramData(CdmaSmsCbProgramData.OPERATION_DELETE_CATEGORY,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT,
                1, 1, 1, "catergoryName");
        mCellBroadcastReceiver.handleCdmaSmsCbProgramData(new ArrayList<>(List.of(programData)));
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT, false);
    }

    @Test
    public void testHandleCdmaSmsCbProgramDataOprationClear() {
        setContext();
        doReturn(mSharedPreferences).when(mContext).getSharedPreferences(anyString(), anyInt());
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mEditor).when(mEditor).putBoolean(anyString(), anyBoolean());

        CdmaSmsCbProgramData programData = new CdmaSmsCbProgramData(
                CdmaSmsCbProgramData.OPERATION_CLEAR_CATEGORIES,
                CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT,
                1, 1, 1, "catergoryName");
        mCellBroadcastReceiver.handleCdmaSmsCbProgramData(new ArrayList<>(List.of(programData)));
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_EXTREME_THREAT, false);
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_SEVERE_THREAT, false);
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY, false);
        verify(mCellBroadcastReceiver).tryCdmaSetCategory(mContext,
                CdmaSmsCbProgramData.CATEGORY_CMAS_TEST_MESSAGE, false);
    }

    //this method is just to assign mContext to the spied instance mCellBroadcastReceiver
    private void setContext() {
        doReturn("dummy action").when(mIntent).getAction();
        doNothing().when(mCellBroadcastReceiver).getCellBroadcastTask(anyLong());

        mCellBroadcastReceiver.onReceive(mContext, mIntent);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }
}
