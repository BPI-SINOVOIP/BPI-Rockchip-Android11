/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.telephony.cts;

import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_IGNORED;
import static android.app.AppOpsManager.OPSTR_READ_PHONE_STATE;
import static android.telephony.CarrierConfigManager.KEY_CARRIER_NAME_OVERRIDE_BOOL;
import static android.telephony.CarrierConfigManager.KEY_CARRIER_NAME_STRING;
import static android.telephony.CarrierConfigManager.KEY_CARRIER_VOLTE_PROVISIONED_BOOL;
import static android.telephony.CarrierConfigManager.KEY_FORCE_HOME_NETWORK_BOOL;
import static android.telephony.ServiceState.STATE_IN_SERVICE;

import static androidx.test.InstrumentationRegistry.getContext;
import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.AppOpsUtils.setOpMode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;


import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.os.Looper;
import android.os.PersistableBundle;
import android.platform.test.annotations.SecurityTest;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionManager.OnSubscriptionsChangedListener;
import android.telephony.TelephonyManager;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.TestThread;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class CarrierConfigManagerTest {

    private static final String CARRIER_NAME_OVERRIDE = "carrier_a";
    private CarrierConfigManager mConfigManager;
    private TelephonyManager mTelephonyManager;
    private SubscriptionManager mSubscriptionManager;
    private PackageManager mPackageManager;

    // Use a long timeout to accommodate devices with lower amounts of memory, as it will take
    // longer for these devices to receive the broadcast (b/161963269). It is expected that all
    // devices can receive the broadcast in under 5s (most should receive it well before then).
    private static final int BROADCAST_TIMEOUT_MILLIS = 5000;
    private static final CountDownLatch COUNT_DOWN_LATCH = new CountDownLatch(1);

    @Before
    public void setUp() throws Exception {
        mTelephonyManager = (TelephonyManager)
                getContext().getSystemService(Context.TELEPHONY_SERVICE);
        mConfigManager = (CarrierConfigManager)
                getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        mSubscriptionManager =
                (SubscriptionManager)
                        getContext().getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        mPackageManager = getContext().getPackageManager();
    }

    @After
    public void tearDown() throws Exception {
        try {
            setOpMode("--uid android.telephony.cts", OPSTR_READ_PHONE_STATE, MODE_ALLOWED);
        } catch (IOException e) {
            fail();
        }
    }

    /**
     * Checks whether the telephony stack should be running on this device.
     *
     * Note: "Telephony" means only SMS/MMS and voice calls in some contexts, but we also care if
     * the device supports cellular data.
     */
    private boolean hasTelephony() {
        ConnectivityManager mgr =
                (ConnectivityManager) getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        return mgr.isNetworkSupported(ConnectivityManager.TYPE_MOBILE);
    }

    private boolean isSimCardPresent() {
        return mTelephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_NONE &&
                mTelephonyManager.getSimState() != TelephonyManager.SIM_STATE_UNKNOWN &&
                mTelephonyManager.getSimState() != TelephonyManager.SIM_STATE_ABSENT;
    }

    private boolean isSimCardAbsent() {
        return mTelephonyManager.getSimState() == TelephonyManager.SIM_STATE_ABSENT;
    }

    private void checkConfig(PersistableBundle config) {
        if (config == null) {
            assertFalse(
                    "Config should only be null when telephony is not running.", hasTelephony());
            return;
        }
        assertNotNull("CarrierConfigManager should not return null config", config);
        if (isSimCardAbsent()) {
            // Static default in CarrierConfigManager will be returned when no sim card present.
            assertEquals("Config doesn't match static default.",
                    config.getBoolean(CarrierConfigManager.KEY_ADDITIONAL_CALL_SETTING_BOOL), true);

            assertEquals("KEY_VVM_DESTINATION_NUMBER_STRING doesn't match static default.",
                config.getString(CarrierConfigManager.KEY_VVM_DESTINATION_NUMBER_STRING), "");
            assertEquals("KEY_VVM_PORT_NUMBER_INT doesn't match static default.",
                config.getInt(CarrierConfigManager.KEY_VVM_PORT_NUMBER_INT), 0);
            assertEquals("KEY_VVM_TYPE_STRING doesn't match static default.",
                config.getString(CarrierConfigManager.KEY_VVM_TYPE_STRING), "");
            assertEquals("KEY_VVM_CELLULAR_DATA_REQUIRED_BOOLEAN doesn't match static default.",
                config.getBoolean(CarrierConfigManager.KEY_VVM_CELLULAR_DATA_REQUIRED_BOOL),
                false);
            assertEquals("KEY_VVM_PREFETCH_BOOLEAN doesn't match static default.",
                config.getBoolean(CarrierConfigManager.KEY_VVM_PREFETCH_BOOL), true);
            assertEquals("KEY_CARRIER_VVM_PACKAGE_NAME_STRING doesn't match static default.",
                config.getString(CarrierConfigManager.KEY_CARRIER_VVM_PACKAGE_NAME_STRING), "");
            assertFalse(CarrierConfigManager.isConfigForIdentifiedCarrier(config));

            // Check default value matching
            assertEquals("KEY_DATA_LIMIT_NOTIFICATION_BOOL doesn't match static default.",
                    config.getBoolean(CarrierConfigManager.KEY_DATA_LIMIT_NOTIFICATION_BOOL),
                            true);
            assertEquals("KEY_DATA_RAPID_NOTIFICATION_BOOL doesn't match static default.",
                    config.getBoolean(CarrierConfigManager.KEY_DATA_RAPID_NOTIFICATION_BOOL),
                            true);
            assertEquals("KEY_DATA_WARNING_NOTIFICATION_BOOL doesn't match static default.",
                    config.getBoolean(CarrierConfigManager.KEY_DATA_WARNING_NOTIFICATION_BOOL),
                            true);
            assertEquals("Gps.KEY_PERSIST_LPP_MODE_BOOL doesn't match static default.",
                    config.getBoolean(CarrierConfigManager.Gps.KEY_PERSIST_LPP_MODE_BOOL),
                            true);
            assertEquals("KEY_MONTHLY_DATA_CYCLE_DAY_INT doesn't match static default.",
                    config.getInt(CarrierConfigManager.KEY_MONTHLY_DATA_CYCLE_DAY_INT),
                            CarrierConfigManager.DATA_CYCLE_USE_PLATFORM_DEFAULT);
        }

        // These key should return default values if not customized.
        assertNotNull(config.getIntArray(
                CarrierConfigManager.KEY_5G_NR_SSRSRP_THRESHOLDS_INT_ARRAY));
        assertNotNull(config.getIntArray(
                CarrierConfigManager.KEY_5G_NR_SSRSRQ_THRESHOLDS_INT_ARRAY));
        assertNotNull(config.getIntArray(
                CarrierConfigManager.KEY_5G_NR_SSSINR_THRESHOLDS_INT_ARRAY));
        assertNotNull(config.getIntArray(
                CarrierConfigManager.KEY_LTE_RSRQ_THRESHOLDS_INT_ARRAY));
        assertNotNull(config.getIntArray(
                CarrierConfigManager.KEY_LTE_RSSNR_THRESHOLDS_INT_ARRAY));

        // Check the GPS key prefix
        assertTrue("Gps.KEY_PREFIX doesn't match the prefix of the name of "
                + "Gps.KEY_PERSIST_LPP_MODE_BOOL",
                        CarrierConfigManager.Gps.KEY_PERSIST_LPP_MODE_BOOL.startsWith(
                                CarrierConfigManager.Gps.KEY_PREFIX));
    }

    @Test
    public void testGetConfig() {
        PersistableBundle config = mConfigManager.getConfig();
        checkConfig(config);
    }

    @SecurityTest
    @Test
    public void testRevokePermission() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        PersistableBundle config;

        try {
            setOpMode("--uid android.telephony.cts", OPSTR_READ_PHONE_STATE, MODE_IGNORED);
        } catch (IOException e) {
            fail();
        }

        config = mConfigManager.getConfig();
        assertTrue(config.isEmptyParcel());

        try {
            setOpMode("--uid android.telephony.cts", OPSTR_READ_PHONE_STATE, MODE_ALLOWED);
        } catch (IOException e) {
            fail();
        }

        config = mConfigManager.getConfig();
        checkConfig(config);
    }

    @Test
    public void testGetConfigForSubId() {
        PersistableBundle config =
                mConfigManager.getConfigForSubId(SubscriptionManager.getDefaultSubscriptionId());
        checkConfig(config);
    }

    /**
     * Tests the CarrierConfigManager.notifyConfigChangedForSubId() API. This makes a call to
     * notifyConfigChangedForSubId() API and expects a SecurityException since the test apk is not signed
     * by certificate on the SIM.
     */
    @Test
    public void testNotifyConfigChangedForSubId() {
        try {
            if (isSimCardPresent()) {
                mConfigManager.notifyConfigChangedForSubId(
                        SubscriptionManager.getDefaultSubscriptionId());
                fail("Expected SecurityException. App doesn't have carrier privileges.");
            }
        } catch (SecurityException expected) {
        }
    }

    /**
     * The following methods may return any value depending on the state of the device. Simply
     * call them to make sure they do not throw any exceptions.
     */
    @Test
    public void testCarrierConfigManagerResultDependentApi() {
        assertNotNull(ShellIdentityUtils.invokeMethodWithShellPermissions(mConfigManager,
                (cm) -> cm.getDefaultCarrierServicePackageName()));
    }

    /**
     * This checks that {@link CarrierConfigManager#overrideConfig(int, PersistableBundle)}
     * correctly overrides the Carrier Name (SPN) string.
     */
    @Test
    public void testCarrierConfigNameOverride() throws Exception {
        if (!isSimCardPresent()
                || mTelephonyManager.getServiceState().getState() != STATE_IN_SERVICE) {
            return;
        }

        // Adopt shell permission so the required permission (android.permission.MODIFY_PHONE_STATE)
        // is granted.
        UiAutomation ui = getInstrumentation().getUiAutomation();
        ui.adoptShellPermissionIdentity();

        int subId = SubscriptionManager.getDefaultSubscriptionId();
        TestThread t = new TestThread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();

                OnSubscriptionsChangedListener listener =
                        new OnSubscriptionsChangedListener() {
                            @Override
                            public void onSubscriptionsChanged() {
                                if (CARRIER_NAME_OVERRIDE.equals(
                                        mTelephonyManager.getSimOperatorName())) {
                                    COUNT_DOWN_LATCH.countDown();
                                }
                            }
                        };
                mSubscriptionManager.addOnSubscriptionsChangedListener(listener);

                PersistableBundle carrierNameOverride = new PersistableBundle(3);
                carrierNameOverride.putBoolean(KEY_CARRIER_NAME_OVERRIDE_BOOL, true);
                carrierNameOverride.putBoolean(KEY_FORCE_HOME_NETWORK_BOOL, true);
                carrierNameOverride.putString(KEY_CARRIER_NAME_STRING, CARRIER_NAME_OVERRIDE);
                mConfigManager.overrideConfig(subId, carrierNameOverride);

                Looper.loop();
            }
        });

        try {
            t.start();
            boolean didCarrierNameUpdate =
                    COUNT_DOWN_LATCH.await(BROADCAST_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
            if (!didCarrierNameUpdate) {
                fail("CarrierName not overridden in " + BROADCAST_TIMEOUT_MILLIS + " ms");
            }
        } finally {
            mConfigManager.overrideConfig(subId, null);
            ui.dropShellPermissionIdentity();
        }
    }

    @Test
    public void testGetConfigByComponentForSubId() {
        PersistableBundle config =
                mConfigManager.getConfigByComponentForSubId(
                        CarrierConfigManager.Wifi.KEY_PREFIX,
                        SubscriptionManager.getDefaultSubscriptionId());
        if (config != null) {
            assertTrue(config.containsKey(CarrierConfigManager.Wifi.KEY_HOTSPOT_MAX_CLIENT_COUNT));
            assertFalse(config.containsKey(KEY_CARRIER_VOLTE_PROVISIONED_BOOL));
            assertFalse(config.containsKey(CarrierConfigManager.Gps.KEY_SUPL_ES_STRING));
        }
    }
}
