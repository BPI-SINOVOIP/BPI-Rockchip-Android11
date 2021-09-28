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

package com.android.systemui.plugin.globalactions.wallet;

import static android.service.quickaccesswallet.QuickAccessWalletService.SERVICE_INTERFACE;

import static com.google.common.truth.Truth.assertThat;

import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.provider.Settings;
import android.service.quickaccesswallet.QuickAccessWalletClient;

import androidx.test.core.app.ApplicationProvider;

import com.android.internal.widget.LockPatternUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowLog;

/**
 * Ensures compatibility between the {@link QuickAccessWalletClient} and the plugin
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R, shadows = {
        QuickAccessWalletClientTest.ShadowActivityManager.class,
        QuickAccessWalletClientTest.ShadowLockPatternUtils.class})
public class QuickAccessWalletClientTest {

    private final Context mContext = ApplicationProvider.getApplicationContext();
    private QuickAccessWalletClient mWalletClient;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;
    }

    @Test
    public void isWalletServiceAvailable_serviceAvailable_returnsTrue() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletServiceAvailable()).isTrue();
    }

    @Test
    public void isWalletServiceAvailable_returnsFalseIfServiceUnavailable() {
        setDefaultPaymentApp(mContext.getPackageName());

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletServiceAvailable()).isFalse();
    }

    @Test
    public void isWalletServiceAvailable_returnsFalseIfNfcPaymentAppIsAnotherApp() {
        setDefaultPaymentApp("foo.bar");
        registerWalletService();

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletServiceAvailable()).isFalse();
    }

    @Test
    public void isWalletFeatureAvailable_happyCase() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();
        WalletPluginService.updateSettingsFeatureAvailability(mContext, true);
        ContentResolver cr = mContext.getContentResolver();
        Settings.Secure.putInt(cr, Settings.Secure.USER_SETUP_COMPLETE, 1);

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletFeatureAvailable()).isTrue();
    }

    @Test
    public void isWalletFeatureAvailable_wrongUser() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();
        WalletPluginService.updateSettingsFeatureAvailability(mContext, true);
        ContentResolver cr = mContext.getContentResolver();
        Settings.Secure.putInt(cr, Settings.Secure.USER_SETUP_COMPLETE, 1);
        ShadowActivityManager.setCurrentUser(11);

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletFeatureAvailable()).isFalse();
    }

    @Test
    public void isWalletFeatureAvailable_userSetupIncomplete() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();
        WalletPluginService.updateSettingsFeatureAvailability(mContext, true);
        // do not set user setup complete

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletFeatureAvailable()).isFalse();
    }

    @Test
    public void isWalletFeatureAvailable_globalActionsPanelDisabled() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();
        WalletPluginService.updateSettingsFeatureAvailability(mContext, true);
        ContentResolver cr = mContext.getContentResolver();
        Settings.Secure.putInt(cr,
                Settings.Secure.GLOBAL_ACTIONS_PANEL_ENABLED, 0);
        Settings.Secure.putInt(cr, Settings.Secure.USER_SETUP_COMPLETE, 1);

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletFeatureAvailable()).isFalse();
    }

    @Test
    public void isWalletFeatureAvailable_userInLockdown() {
        setDefaultPaymentApp(mContext.getPackageName());
        registerWalletService();
        WalletPluginService.updateSettingsFeatureAvailability(mContext, true);
        ContentResolver cr = mContext.getContentResolver();
        Settings.Secure.putInt(cr, Settings.Secure.USER_SETUP_COMPLETE, 1);
        ShadowLockPatternUtils.sIsUserInLockdown = true;

        mWalletClient = QuickAccessWalletClient.create(mContext);

        assertThat(mWalletClient.isWalletFeatureAvailable()).isFalse();
    }

    @Test
    public void isWalletFeatureAvailableWhenDeviceLocked() {
        ContentResolver cr = mContext.getContentResolver();
        mWalletClient = QuickAccessWalletClient.create(mContext);

        Settings.Secure.putInt(cr, Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT, 1);

        assertThat(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).isTrue();

        Settings.Secure.putInt(cr, Settings.Secure.POWER_MENU_LOCKED_SHOW_CONTENT, 0);

        assertThat(mWalletClient.isWalletFeatureAvailableWhenDeviceLocked()).isFalse();
    }

    private void setDefaultPaymentApp(String pkg) {
        ComponentName nfcComponent = new ComponentName(pkg, "WalletService");
        Settings.Secure.putString(mContext.getContentResolver(), "nfc_payment_default_component",
                nfcComponent.flattenToString());
    }

    private void registerWalletService() {
        Intent queryIntent = new Intent(SERVICE_INTERFACE).setPackage(mContext.getPackageName());
        ServiceInfo serviceInfo = new ServiceInfo();
        serviceInfo.exported = true;
        serviceInfo.applicationInfo = new ApplicationInfo();
        serviceInfo.applicationInfo.packageName = mContext.getPackageName();
        serviceInfo.name = "QuickAccessWalletService";
        serviceInfo.permission = Manifest.permission.BIND_QUICK_ACCESS_WALLET_SERVICE;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.serviceInfo = serviceInfo;
        PackageManager rpm = mContext.getPackageManager();
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = mContext.getPackageName();
        packageInfo.services = new ServiceInfo[]{serviceInfo};
        shadowOf(rpm).addResolveInfoForIntent(queryIntent, resolveInfo);
    }

    @Implements(LockPatternUtils.class)
    public static class ShadowLockPatternUtils {
        private static boolean sIsUserInLockdown = false;

        @Implementation
        public boolean isUserInLockdown(int userId) {
            return sIsUserInLockdown;
        }
    }

    @Implements(ActivityManager.class)
    public static class ShadowActivityManager {
        private static int sCurrentUserId = 0;

        @Resetter
        public void reset() {
            sCurrentUserId = 0;
        }

        @Implementation
        protected static int getCurrentUser() {
            return sCurrentUserId;
        }

        public static void setCurrentUser(int userId) {
            sCurrentUserId = userId;
        }

        public static ShadowActivityManager getShadow() {
            return (ShadowActivityManager) Shadow.extract(
                    RuntimeEnvironment.application.getSystemService(ActivityManager.class));
        }
    }

}
