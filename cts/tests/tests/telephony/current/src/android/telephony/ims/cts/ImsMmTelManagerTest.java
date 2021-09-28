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

package android.telephony.ims.cts;

import static junit.framework.TestCase.assertEquals;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.PersistableBundle;
import android.telephony.AccessNetworkConstants;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsManager;
import android.telephony.ims.ImsMmTelManager;
import android.telephony.ims.feature.MmTelFeature;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class ImsMmTelManagerTest {

    // Copied from CarrierConfigManager, since these keys is inappropriately marked as @hide
    private static final String KEY_CARRIER_VOLTE_OVERRIDE_WFC_PROVISIONING_BOOL =
            "carrier_volte_override_wfc_provisioning_bool";
    private static final String KEY_EDITABLE_WFC_MODE_BOOL = "editable_wfc_mode_bool";
    private static final String KEY_USE_WFC_HOME_NETWORK_MODE_IN_ROAMING_NETWORK_BOOL =
            "use_wfc_home_network_mode_in_roaming_network_bool";
    private static final String KEY_EDITABLE_WFC_ROAMING_MODE_BOOL =
            "editable_wfc_roaming_mode_bool";

    private static int sTestSub = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    private static Handler sHandler;
    private static CarrierConfigReceiver sReceiver;

    private static class CarrierConfigReceiver extends BroadcastReceiver {
        private CountDownLatch mLatch = new CountDownLatch(1);
        private final int mSubId;

        CarrierConfigReceiver(int subId) {
            mSubId = subId;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
                int subId = intent.getIntExtra(CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX, -1);
                if (mSubId == subId) {
                    mLatch.countDown();
                }
            }
        }

        void clearQueue() {
            mLatch = new CountDownLatch(1);
        }

        void waitForCarrierConfigChanged() throws Exception {
            mLatch.await(5000, TimeUnit.MILLISECONDS);
        }
    }

    @BeforeClass
    public static void beforeAllTests() {
        // assumeTrue() in @BeforeClass is not supported by our test runner.
        // Resort to the early exit.
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        sTestSub = ImsUtils.getPreferredActiveSubId();

        if (Looper.getMainLooper() == null) {
            Looper.prepareMainLooper();
        }
        sHandler = new Handler(Looper.getMainLooper());

        sReceiver = new CarrierConfigReceiver(sTestSub);
        IntentFilter filter = new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        // ACTION_CARRIER_CONFIG_CHANGED is sticky, so we will get a callback right away.
        getContext().registerReceiver(sReceiver, filter);
    }

    @AfterClass
    public static void afterAllTests() {
        // assumeTrue() in @AfterClass is not supported by our test runner.
        // Resort to the early exit.
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        if (sReceiver != null) {
            getContext().unregisterReceiver(sReceiver);
            sReceiver = null;
        }
    }

    @Before
    public void beforeTest() {
        assumeTrue(ImsUtils.shouldTestImsService());

        if (!SubscriptionManager.isValidSubscriptionId(sTestSub)) {
            fail("This test requires that there is a SIM in the device!");
        }
    }

    @Test
    public void testGetVoWiFiSetting_noPermission() {
        try {
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
            boolean isEnabled = mMmTelManager.isVoWiFiSettingEnabled();
            fail("Expected SecurityException for missing permissions");
        } catch (SecurityException ex) {
            /* Expected */
        }
    }

    /**
     * Given the advanced calling setting is editable and not hidden
     * (see {@link CarrierConfigManager#KEY_EDITABLE_ENHANCED_4G_LTE_BOOL}, and
     * {@link CarrierConfigManager#KEY_HIDE_ENHANCED_4G_LTE_BOOL}), set the advanced
     * calling setting and ensure the correct calling setting is returned. Also ensure the
     * ContentObserver is triggered properly.
     */
    @Test
    public void testAdvancedCallingSetting() throws Exception {
        // Ensure advanced calling setting is editable.
        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(CarrierConfigManager.KEY_EDITABLE_ENHANCED_4G_LTE_BOOL, true);
        bundle.putBoolean(CarrierConfigManager.KEY_HIDE_ENHANCED_4G_LTE_BOOL, false);
        overrideCarrierConfig(bundle);
        // Register Observer
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.ADVANCED_CALLING_ENABLED_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        boolean isEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isAdvancedCallingSettingEnabled);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setAdvancedCallingSettingEnabled(!isEnabled));

        waitForLatch(contentObservedLatch, observer);
        boolean isEnabledResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isAdvancedCallingSettingEnabled);
        assertEquals("isAdvancedCallingSettingEnabled does not reflect the new value set by "
                        + "setAdvancedCallingSettingEnabled", !isEnabled, isEnabledResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setAdvancedCallingSettingEnabled(isEnabled));
        // restore original carrier config.
        overrideCarrierConfig(null);
    }

    /**
     * Set the VT setting and ensure it is queried successfully. Also ensure the ContentObserver
     * is triggered properly.
     */
    @Test
    public void testVtSetting() throws Exception {
        // Register Observer
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.VT_ENABLED_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        boolean isEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVtSettingEnabled);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVtSettingEnabled(!isEnabled));

        waitForLatch(contentObservedLatch, observer);
        boolean isEnabledResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVtSettingEnabled);
        assertEquals("isVtSettingEnabled does not match the value set by setVtSettingEnabled",
                !isEnabled, isEnabledResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVtSettingEnabled(isEnabled));
    }

    /**
     * Set the VoWiFi setting and ensure it is queried successfully. Also ensure the ContentObserver
     * is triggered properly.
     */
    @Test
    public void testVoWiFiSetting() throws Exception {
        PersistableBundle bundle = new PersistableBundle();
        // Do not worry about provisioning for this test
        bundle.putBoolean(KEY_CARRIER_VOLTE_OVERRIDE_WFC_PROVISIONING_BOOL, false);
        bundle.putBoolean(CarrierConfigManager.KEY_CARRIER_VOLTE_PROVISIONING_REQUIRED_BOOL, false);
        overrideCarrierConfig(bundle);
        // Register Observer
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.WFC_ENABLED_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);

        boolean isEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVoWiFiSettingEnabled);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiSettingEnabled(!isEnabled));

        waitForLatch(contentObservedLatch, observer);
        boolean isEnabledResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVoWiFiSettingEnabled);
        assertEquals("isVoWiFiSettingEnabled did not match value set by setVoWiFiSettingEnabled",
                !isEnabled, isEnabledResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiSettingEnabled(isEnabled));
        overrideCarrierConfig(null);
    }

    /**
     * Set the VoWiFi roaming setting and ensure it is queried successfully. Also ensure the
     * ContentObserver is triggered properly.
     */
    @Test
    public void testVoWiFiRoamingSetting() throws Exception {
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.WFC_ROAMING_ENABLED_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        boolean isEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVoWiFiRoamingSettingEnabled);
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiRoamingSettingEnabled(!isEnabled));

        waitForLatch(contentObservedLatch, observer);
        boolean isEnabledResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::isVoWiFiRoamingSettingEnabled);
        assertEquals("isVoWiFiRoamingSettingEnabled result does not match the value set by "
                + "setVoWiFiRoamingSettingEnabled", !isEnabled, isEnabledResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiRoamingSettingEnabled(isEnabled));
    }

    /**
     * Expect to fail when Set the VoWiFi Mode setting withour proper permission
     */
    @Test
    public void testGetVoWiFiModeSetting_noPermission() throws Exception {
        try {
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
            int oldMode = mMmTelManager.getVoWiFiModeSetting();
            fail("Expected SecurityException for missing permissoins");
        } catch (SecurityException ex) {
            /* Expected */
        }

    }

    /**
     * Expect to fail when Set the VoWiFi Mode setting withour proper permission
     */
    @Test
    public void testGetVoWiFiRoamingModeSetting_noPermission() throws Exception {
        try {
            ImsManager imsManager = getContext().getSystemService(ImsManager.class);
            ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
            int oldMode = mMmTelManager.getVoWiFiRoamingModeSetting();
            fail("Expected SecurityException for missing permissoins");
        } catch (SecurityException ex) {
            /* Expected */
        }

    }


    /**
     * Set the VoWiFi Mode setting and ensure the ContentResolver is triggered as well.
     */
    @Test
    public void testVoWiFiModeSetting() throws Exception {
        PersistableBundle bundle = new PersistableBundle();
        bundle.putBoolean(KEY_EDITABLE_WFC_MODE_BOOL, true);
        overrideCarrierConfig(bundle);
        // Register Observer
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.WFC_MODE_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        int oldMode = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::getVoWiFiModeSetting);
        // Keep the mode in the bounds 0-2
        int newMode = (oldMode + 1) % 3;
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiModeSetting(newMode));

        waitForLatch(contentObservedLatch, observer);
        int newModeResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::getVoWiFiModeSetting);
        assertEquals(newMode, newModeResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiModeSetting(oldMode));
        overrideCarrierConfig(null);
    }

    /**
     * Set the VoWiFi Mode setting and ensure the ContentResolver is triggered as well.
     */
    @Test
    public void testVoWiFiRoamingModeSetting() throws Exception {
        PersistableBundle bundle = new PersistableBundle();
        // Ensure the WFC roaming mode will be changed properly
        bundle.putBoolean(KEY_USE_WFC_HOME_NETWORK_MODE_IN_ROAMING_NETWORK_BOOL, false);
        bundle.putBoolean(KEY_EDITABLE_WFC_ROAMING_MODE_BOOL, true);
        overrideCarrierConfig(bundle);
        // Register Observer
        Uri callingUri = Uri.withAppendedPath(
                SubscriptionManager.WFC_ROAMING_MODE_CONTENT_URI, "" + sTestSub);
        CountDownLatch contentObservedLatch = new CountDownLatch(1);
        ContentObserver observer = createObserver(callingUri, contentObservedLatch);

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        int oldMode = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::getVoWiFiRoamingModeSetting);
        // Keep the mode in the bounds 0-2
        int newMode = (oldMode + 1) % 3;
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiRoamingModeSetting(newMode));

        waitForLatch(contentObservedLatch, observer);
        int newModeResult = ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                ImsMmTelManager::getVoWiFiRoamingModeSetting);
        assertEquals("getVoWiFiRoamingModeSetting was not set to value set by"
                + "setVoWiFiRoamingModeSetting", newMode, newModeResult);

        // Set back to default
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                (m) -> m.setVoWiFiRoamingModeSetting(oldMode));
        overrideCarrierConfig(null);
    }

    /**
     * Test Permissions on various APIs.
     */
    @Test
    public void testMethodPermissions() throws Exception {
        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        ImsMmTelManager mMmTelManager = imsManager.getImsMmTelManager(sTestSub);
        // setRttCapabilitySetting
        try {
            mMmTelManager.setRttCapabilitySetting(false);
            fail("setRttCapabilitySetting requires MODIFY_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                    (m) -> m.setRttCapabilitySetting(false),
                    "android.permission.MODIFY_PHONE_STATE");
        } catch (SecurityException e) {
            fail("setRttCapabilitySetting requires MODIFY_PHONE_STATE permission.");
        }
        // setVoWiFiNonPersistent
        try {
            mMmTelManager.setVoWiFiNonPersistent(true,
                    ImsMmTelManager.WIFI_MODE_CELLULAR_PREFERRED);
            fail("setVoWiFiNonPersistent requires MODIFY_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mMmTelManager,
                    (m) -> m.setVoWiFiNonPersistent(true,
                            ImsMmTelManager.WIFI_MODE_CELLULAR_PREFERRED),
                    "android.permission.MODIFY_PHONE_STATE");
        } catch (SecurityException e) {
            fail("setVoWiFiNonPersistent requires MODIFY_PHONE_STATE permission.");
        }

        try {
            mMmTelManager.isVtSettingEnabled();
            fail("isVtSettingEnabled requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            mMmTelManager.isAdvancedCallingSettingEnabled();
            fail("isAdvancedCallingSettingEnabled requires READ_PRECISE_PHONE_STATE.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            mMmTelManager.isVoWiFiRoamingSettingEnabled();
            fail("isVoWiFiRoamingSettingEnabled requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            mMmTelManager.isVoWiFiSettingEnabled();
            fail("isVoWiFiSettingEnabled requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            mMmTelManager.isTtyOverVolteEnabled();
            fail("isTtyOverVolteEnabled requires READ_PRIVILEGED_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
        try {
            mMmTelManager.isSupported(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                    AccessNetworkConstants.TRANSPORT_TYPE_WWAN, Runnable::run, (result) -> { });
            fail("isSupported requires READ_PRIVILEGED_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
        try {
            mMmTelManager.getRegistrationState(Runnable::run, (result) -> { });
            fail("getRegistrationState requires READ_PRECISE_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }
        try {
            mMmTelManager.getRegistrationTransportType(Runnable::run, (result) -> { });
            fail("getRegistrationTransportType requires READ_PRIVILEGED_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            mMmTelManager.isSupported(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                    AccessNetworkConstants.TRANSPORT_TYPE_WWAN, Runnable::run, (result) -> { });
            fail("isSupported requires READ_PRIVILEGED_PHONE_STATE permission.");
        } catch (SecurityException e) {
            //expected
        }

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(mMmTelManager,
                    ImsMmTelManager::isTtyOverVolteEnabled,
                    "android.permission.READ_PRIVILEGED_PHONE_STATE");
        } catch (SecurityException e) {
            fail("isTtyOverVolteEnabled requires READ_PRIVILEGED_PHONE_STATE permission.");
        }
        try {
            LinkedBlockingQueue<Boolean> resultQueue = new LinkedBlockingQueue<>(1);
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mMmTelManager,
                    (m) -> m.isSupported(MmTelFeature.MmTelCapabilities.CAPABILITY_TYPE_VOICE,
                            AccessNetworkConstants.TRANSPORT_TYPE_WWAN,
                            // Run on the binder thread.
                            Runnable::run,
                            resultQueue::offer), ImsException.class,
                    "android.permission.READ_PRIVILEGED_PHONE_STATE");
            assertNotNull(resultQueue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (SecurityException e) {
            fail("isSupported requires READ_PRIVILEGED_PHONE_STATE permission.");
        }
        try {
            LinkedBlockingQueue<Integer> resultQueue = new LinkedBlockingQueue<>(1);
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mMmTelManager,
                    (m) -> m.getRegistrationState(Runnable::run, resultQueue::offer),
                    ImsException.class, "android.permission.READ_PRIVILEGED_PHONE_STATE");
            assertNotNull(resultQueue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (SecurityException e) {
            fail("getRegistrationState requires READ_PRIVILEGED_PHONE_STATE permission.");
        }
        try {
            LinkedBlockingQueue<Integer> resultQueue = new LinkedBlockingQueue<>(1);
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mMmTelManager,
                    (m) -> m.getRegistrationTransportType(Runnable::run, resultQueue::offer),
                    ImsException.class, "android.permission.READ_PRIVILEGED_PHONE_STATE");
            assertNotNull(resultQueue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (SecurityException e) {
            fail("getRegistrationTransportType requires READ_PRIVILEGED_PHONE_STATE permission.");
        }
    }

    private void overrideCarrierConfig(PersistableBundle bundle) throws Exception {
        CarrierConfigManager carrierConfigManager = getContext().getSystemService(
                CarrierConfigManager.class);
        sReceiver.clearQueue();
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(carrierConfigManager,
                (m) -> m.overrideConfig(sTestSub, bundle));
        sReceiver.waitForCarrierConfigChanged();
    }

    private ContentObserver createObserver(Uri observerUri, CountDownLatch latch) {
        ContentObserver observer = new ContentObserver(sHandler) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                if (observerUri.equals(uri)) {
                    latch.countDown();
                }
            }
        };
        getContext().getContentResolver().registerContentObserver(observerUri, true, observer);
        return observer;
    }

    private void waitForLatch(CountDownLatch latch, ContentObserver observer) {
        try {
            // Wait for the ContentObserver to fire signalling the change.
            latch.await(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail("Interrupted Exception waiting for latch countdown:" + e.getMessage());
        } finally {
            getContext().getContentResolver().unregisterContentObserver(observer);
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }
}
