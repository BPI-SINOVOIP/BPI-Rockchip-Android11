/*
 * Copyright (C) 2009 The Android Open Source Project
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

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.Manifest.permission;
import android.app.UiAutomation;
import android.bluetooth.BluetoothAdapter;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PersistableBundle;
import android.os.Process;
import android.os.UserManager;
import android.provider.Settings;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telephony.AccessNetworkConstants;
import android.telephony.Annotation.RadioPowerState;
import android.telephony.AvailableNetworkInfo;
import android.telephony.CallAttributes;
import android.telephony.CallQuality;
import android.telephony.CarrierConfigManager;
import android.telephony.CellLocation;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.PhoneStateListener;
import android.telephony.PreciseCallState;
import android.telephony.RadioAccessFamily;
import android.telephony.ServiceState;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.UiccCardInfo;
import android.telephony.UiccSlotInfo;
import android.telephony.data.ApnSetting;
import android.telephony.emergency.EmergencyNumber;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;
import com.android.compatibility.common.util.TestThread;
import com.android.internal.telephony.uicc.IccUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.regex.Pattern;
import java.util.stream.Collectors;


/**
 * Build, install and run the tests by running the commands below:
 *  make cts -j64
 *  cts-tradefed run cts -m CtsTelephonyTestCases --test android.telephony.cts.TelephonyManagerTest
 */
public class TelephonyManagerTest {
    private TelephonyManager mTelephonyManager;
    private SubscriptionManager mSubscriptionManager;
    private PackageManager mPackageManager;
    private boolean mOnCellLocationChangedCalled = false;
    private boolean mServiceStateChangedCalled = false;
    private boolean mRadioRebootTriggered = false;
    private boolean mHasRadioPowerOff = false;
    private ServiceState mServiceState;
    private final Object mLock = new Object();

    private CarrierConfigManager mCarrierConfigManager;
    private String mSelfPackageName;
    private String mSelfCertHash;

    private static final int TOLERANCE = 1000;
    private PhoneStateListener mListener;
    private static ConnectivityManager mCm;
    private static final String TAG = "TelephonyManagerTest";
    private static final List<Integer> ROAMING_TYPES = Arrays.asList(
            ServiceState.ROAMING_TYPE_DOMESTIC,
            ServiceState.ROAMING_TYPE_INTERNATIONAL,
            ServiceState.ROAMING_TYPE_NOT_ROAMING,
            ServiceState.ROAMING_TYPE_UNKNOWN);
    private static final List<Integer> NETWORK_TYPES = Arrays.asList(
            TelephonyManager.NETWORK_TYPE_UNKNOWN,
            TelephonyManager.NETWORK_TYPE_GPRS,
            TelephonyManager.NETWORK_TYPE_EDGE,
            TelephonyManager.NETWORK_TYPE_UMTS,
            TelephonyManager.NETWORK_TYPE_CDMA,
            TelephonyManager.NETWORK_TYPE_EVDO_0,
            TelephonyManager.NETWORK_TYPE_EVDO_A,
            TelephonyManager.NETWORK_TYPE_1xRTT,
            TelephonyManager.NETWORK_TYPE_HSDPA,
            TelephonyManager.NETWORK_TYPE_HSUPA,
            TelephonyManager.NETWORK_TYPE_HSPA,
            TelephonyManager.NETWORK_TYPE_IDEN,
            TelephonyManager.NETWORK_TYPE_EVDO_B,
            TelephonyManager.NETWORK_TYPE_LTE,
            TelephonyManager.NETWORK_TYPE_EHRPD,
            TelephonyManager.NETWORK_TYPE_HSPAP,
            TelephonyManager.NETWORK_TYPE_GSM,
            TelephonyManager.NETWORK_TYPE_TD_SCDMA,
            TelephonyManager.NETWORK_TYPE_IWLAN,
            TelephonyManager.NETWORK_TYPE_LTE_CA,
            TelephonyManager.NETWORK_TYPE_NR);

    private static final int EMERGENCY_NUMBER_SOURCE_RIL_ECCLIST = 0;
    private static final Set<Integer> EMERGENCY_NUMBER_SOURCE_SET;

    private static final String PLMN_A = "123456";
    private static final String PLMN_B = "78901";
    private static final List<String> FPLMN_TEST = Arrays.asList(PLMN_A, PLMN_B);
    private static final int MAX_FPLMN_NUM = 100;
    private static final int MIN_FPLMN_NUM = 3;

    private static final String TEST_FORWARD_NUMBER = "54321";
    private static final String TESTING_PLMN = "12345";

    private static final int RADIO_HAL_VERSION_1_3 = makeRadioVersion(1, 3);

    static {
        EMERGENCY_NUMBER_SOURCE_SET = new HashSet<Integer>();
        EMERGENCY_NUMBER_SOURCE_SET.add(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_NETWORK_SIGNALING);
        EMERGENCY_NUMBER_SOURCE_SET.add(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_SIM);
        EMERGENCY_NUMBER_SOURCE_SET.add(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_DATABASE);
        EMERGENCY_NUMBER_SOURCE_SET.add(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_MODEM_CONFIG);
        EMERGENCY_NUMBER_SOURCE_SET.add(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_DEFAULT);
    }

    private static final Set<Integer> EMERGENCY_SERVICE_CATEGORY_SET;
    static {
        EMERGENCY_SERVICE_CATEGORY_SET = new HashSet<Integer>();
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_POLICE);
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AMBULANCE);
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_FIRE_BRIGADE);
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MARINE_GUARD);
        EMERGENCY_SERVICE_CATEGORY_SET.add(
                EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MOUNTAIN_RESCUE);
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MIEC);
        EMERGENCY_SERVICE_CATEGORY_SET.add(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AIEC);
    }

    private int mTestSub;
    private TelephonyManagerTest.CarrierConfigReceiver mReceiver;
    private int mRadioVersion;

    private static class CarrierConfigReceiver extends BroadcastReceiver {
        private CountDownLatch mLatch = new CountDownLatch(1);
        private final int mSubId;

        CarrierConfigReceiver(int subId) {
            mSubId = subId;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(intent.getAction())) {
                int subId = intent.getIntExtra(CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX,
                        SubscriptionManager.INVALID_SUBSCRIPTION_ID);
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

    @Before
    public void setUp() throws Exception {
        mCm = getContext().getSystemService(ConnectivityManager.class);
        mSubscriptionManager = getContext().getSystemService(SubscriptionManager.class);
        mPackageManager = getContext().getPackageManager();
        mCarrierConfigManager = getContext().getSystemService(CarrierConfigManager.class);
        mSelfPackageName = getContext().getPackageName();
        mSelfCertHash = getCertHash(mSelfPackageName);
        mTestSub = SubscriptionManager.getDefaultSubscriptionId();
        mTelephonyManager = getContext().getSystemService(TelephonyManager.class)
                .createForSubscriptionId(mTestSub);
        mReceiver = new CarrierConfigReceiver(mTestSub);
        Pair<Integer, Integer> radioVersion = mTelephonyManager.getRadioHalVersion();
        mRadioVersion = makeRadioVersion(radioVersion.first, radioVersion.second);
        IntentFilter filter = new IntentFilter(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        // ACTION_CARRIER_CONFIG_CHANGED is sticky, so we will get a callback right away.
        getContext().registerReceiver(mReceiver, filter);
    }

    @After
    public void tearDown() throws Exception {
        if (mListener != null) {
            // unregister the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        }
        if (mReceiver != null) {
            getContext().unregisterReceiver(mReceiver);
            mReceiver = null;
        }
    }

    private String getCertHash(String pkgName) throws Exception {
        try {
            PackageInfo pInfo = mPackageManager.getPackageInfo(pkgName,
                    PackageManager.GET_SIGNATURES
                            | PackageManager.GET_DISABLED_UNTIL_USED_COMPONENTS);
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            return IccUtils.bytesToHexString(md.digest(pInfo.signatures[0].toByteArray()));
        } catch (PackageManager.NameNotFoundException ex) {
            Log.e(TAG, pkgName + " not found", ex);
            throw ex;
        } catch (NoSuchAlgorithmException ex) {
            Log.e(TAG, "Algorithm SHA1 is not found.");
            throw ex;
        }
    }

    /** Checks whether the cellular stack should be running on this device. */
    private boolean hasCellular() {
        return mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)
                && mTelephonyManager.getPhoneCount() > 0;
    }

    @Test
    public void testHasCarrierPrivilegesViaCarrierConfigs() throws Exception {
        if (!hasCellular()) return;
        PersistableBundle carrierConfig = mCarrierConfigManager.getConfigForSubId(mTestSub);

        try {
            assertNotNull("CarrierConfigManager#getConfigForSubId() returned null",
                    carrierConfig);
            assertFalse("CarrierConfigManager#getConfigForSubId() returned empty bundle",
                    carrierConfig.isEmpty());

            // purge the certs in carrierConfigs first
            carrierConfig.putStringArray(
                    CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY, new String[]{});
            overrideCarrierConfig(carrierConfig);
            // verify we don't have privilege through carrierConfigs or Uicc
            assertFalse(mTelephonyManager.hasCarrierPrivileges());

            carrierConfig.putStringArray(
                    CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY,
                    new String[]{mSelfCertHash});

            // verify we now have privilege after adding certificate to carrierConfigs
            overrideCarrierConfig(carrierConfig);
            assertTrue(mTelephonyManager.hasCarrierPrivileges());
        } finally {
            // purge the newly added certificate
            carrierConfig.putStringArray(
                    CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY, new String[]{});
            // carrierConfig.remove(CarrierConfigManager.KEY_CARRIER_CERTIFICATE_STRING_ARRAY);
            overrideCarrierConfig(carrierConfig);

            // verify we no longer have privilege after removing certificate
            assertFalse(mTelephonyManager.hasCarrierPrivileges());
        }
    }

    private void overrideCarrierConfig(PersistableBundle bundle) throws Exception {
        mReceiver.clearQueue();
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mCarrierConfigManager,
                (cm) -> cm.overrideConfig(mTestSub, bundle));
        mReceiver.waitForCarrierConfigChanged();
    }

    public static void grantLocationPermissions() {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        String packageName = getContext().getPackageName();
        uiAutomation.grantRuntimePermission(packageName, permission.ACCESS_COARSE_LOCATION);
        uiAutomation.grantRuntimePermission(packageName, permission.ACCESS_FINE_LOCATION);
        uiAutomation.grantRuntimePermission(packageName, permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testDevicePolicyApn() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "skipping test on device without FEATURE_TELEPHONY present");
            return;
        }
        // These methods aren't accessible to anything except system and phone by design, so we just
        // look for security exceptions here.
        try {
            List<ApnSetting> apns = mTelephonyManager.getDevicePolicyOverrideApns(getContext());
            fail("SecurityException expected");
        } catch (SecurityException e) {
            // expected
        }

        try {
            ApnSetting.Builder builder = new ApnSetting.Builder();

            ApnSetting setting = builder
                    .setEntryName("asdf")
                    .setApnName("asdf")
                    .setApnTypeBitmask(ApnSetting.TYPE_DEFAULT)
                    .build();
            int id = mTelephonyManager.addDevicePolicyOverrideApn(getContext(), setting);
            fail("SecurityException expected");
        } catch (SecurityException e) {
            // expected
        }

        try {
            ApnSetting.Builder builder = new ApnSetting.Builder();

            ApnSetting setting = builder
                    .setEntryName("asdf")
                    .setApnName("asdf")
                    .setApnTypeBitmask(ApnSetting.TYPE_DEFAULT)
                    .build();
            boolean success = mTelephonyManager.modifyDevicePolicyOverrideApn(
                    getContext(), 0, setting);
            fail("SecurityException expected");
        } catch (SecurityException e) {
            // expected
        }
    }
    @Test
    public void testListen() throws Throwable {
        if (!InstrumentationRegistry.getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires PackageManager.FEATURE_TELEPHONY");
            return;
        }

        if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
            // TODO: temp workaround, need to adjust test to for CDMA
            return;
        }

        grantLocationPermissions();

        TestThread t = new TestThread(new Runnable() {
            public void run() {
                Looper.prepare();
                mListener = new PhoneStateListener() {
                    @Override
                    public void onCellLocationChanged(CellLocation location) {
                        if(!mOnCellLocationChangedCalled) {
                            synchronized (mLock) {
                                mOnCellLocationChangedCalled = true;
                                mLock.notify();
                            }
                        }
                    }
                };

                synchronized (mLock) {
                    mLock.notify(); // mListener is ready
                }

                Looper.loop();
            }
        });

        synchronized (mLock) {
            t.start();
            mLock.wait(TOLERANCE); // wait for mListener
        }

        // Test register
        synchronized (mLock) {
            // .listen generates an onCellLocationChanged event
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_CELL_LOCATION);
            mLock.wait(TOLERANCE);

            assertTrue("Test register, mOnCellLocationChangedCalled should be true.",
                    mOnCellLocationChangedCalled);
        }

        synchronized (mLock) {
            mOnCellLocationChangedCalled = false;
            CellLocation.requestLocationUpdate();
            mLock.wait(TOLERANCE);

            assertTrue("Test register, mOnCellLocationChangedCalled should be true.",
                    mOnCellLocationChangedCalled);
        }

        // unregister the listener
        mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        Thread.sleep(TOLERANCE);

        // Test unregister
        synchronized (mLock) {
            mOnCellLocationChangedCalled = false;
            // unregister again, to make sure doing so does not call the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
            CellLocation.requestLocationUpdate();
            mLock.wait(TOLERANCE);

            assertFalse("Test unregister, mOnCellLocationChangedCalled should be false.",
                    mOnCellLocationChangedCalled);
        }
    }

    /**
     * The getter methods here are all related to the information about the telephony.
     * These getters are related to concrete location, phone, service provider company, so
     * it's no need to get details of these information, just make sure they are in right
     * condition(>0 or not null).
     */
    @Test
    public void testTelephonyManager() {
        assertTrue(mTelephonyManager.getNetworkType() >= TelephonyManager.NETWORK_TYPE_UNKNOWN);
        assertTrue(mTelephonyManager.getPhoneType() >= TelephonyManager.PHONE_TYPE_NONE);
        assertTrue(mTelephonyManager.getSimState() >= TelephonyManager.SIM_STATE_UNKNOWN);
        assertTrue(mTelephonyManager.getDataActivity() >= TelephonyManager.DATA_ACTIVITY_NONE);
        assertTrue(mTelephonyManager.getDataState() >= TelephonyManager.DATA_DISCONNECTED);
        assertTrue(mTelephonyManager.getCallState() >= TelephonyManager.CALL_STATE_IDLE);

        for (int i = 0; i < mTelephonyManager.getPhoneCount(); ++i) {
            assertTrue(mTelephonyManager.getSimState(i) >= TelephonyManager.SIM_STATE_UNKNOWN);
        }

        // Make sure devices without MMS service won't fail on this
        if (InstrumentationRegistry.getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY)
                && (mTelephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_NONE)) {
            assertFalse(mTelephonyManager.getMmsUserAgent().isEmpty());
            assertFalse(mTelephonyManager.getMmsUAProfUrl().isEmpty());
        }

        // The following methods may return any value depending on the state of the device. Simply
        // call them to make sure they do not throw any exceptions.
        mTelephonyManager.getVoiceMailNumber();
        mTelephonyManager.getSimOperatorName();
        mTelephonyManager.getNetworkCountryIso();
        mTelephonyManager.getCellLocation();
        mTelephonyManager.getSimCarrierId();
        mTelephonyManager.getSimCarrierIdName();
        mTelephonyManager.getSimSpecificCarrierId();
        mTelephonyManager.getSimSpecificCarrierIdName();
        mTelephonyManager.getCarrierIdFromSimMccMnc();
        mTelephonyManager.isDataRoamingEnabled();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getSimSerialNumber());
        mTelephonyManager.getSimOperator();
        mTelephonyManager.getSignalStrength();
        mTelephonyManager.getNetworkOperatorName();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getSubscriberId());
        mTelephonyManager.getLine1Number();
        mTelephonyManager.getNetworkOperator();
        mTelephonyManager.getSimCountryIso();
        mTelephonyManager.getVoiceMailAlphaTag();
        mTelephonyManager.isNetworkRoaming();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getDeviceId());
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getDeviceId(mTelephonyManager.getSlotIndex()));
        mTelephonyManager.getDeviceSoftwareVersion();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getDeviceSoftwareVersion(mTelephonyManager.getSlotIndex()));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei());
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei(mTelephonyManager.getSlotIndex()));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.isManualNetworkSelectionAllowed());
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getManualNetworkSelectionPlmn());

        mTelephonyManager.getPhoneCount();
        mTelephonyManager.getDataEnabled();
        mTelephonyManager.getNetworkSpecifier();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager, (tm) -> tm.getNai());
        TelecomManager telecomManager = getContext().getSystemService(TelecomManager.class);
        PhoneAccountHandle defaultAccount = telecomManager
                .getDefaultOutgoingPhoneAccount(PhoneAccount.SCHEME_TEL);
        mTelephonyManager.getVoicemailRingtoneUri(defaultAccount);
        mTelephonyManager.isVoicemailVibrationEnabled(defaultAccount);
        mTelephonyManager.getSubscriptionId(defaultAccount);
        mTelephonyManager.getCarrierConfig();
        mTelephonyManager.isVoiceCapable();
        mTelephonyManager.isSmsCapable();
        mTelephonyManager.isLteCdmaEvdoGsmWcdmaEnabled();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.isDataConnectionAllowed());
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.isAnyRadioPoweredOn());
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.resetIms(tm.getSlotIndex()));

        // Verify TelephonyManager.getCarrierPrivilegeStatus
        List<Integer> validCarrierPrivilegeStatus = new ArrayList<>();
        validCarrierPrivilegeStatus.add(TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS);
        validCarrierPrivilegeStatus.add(TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS);
        validCarrierPrivilegeStatus.add(
                TelephonyManager.CARRIER_PRIVILEGE_STATUS_RULES_NOT_LOADED);
        validCarrierPrivilegeStatus.add(
                TelephonyManager.CARRIER_PRIVILEGE_STATUS_ERROR_LOADING_RULES);
        int carrierPrivilegeStatusResult = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> tm.getCarrierPrivilegeStatus(Process.myUid()));
        assertTrue(validCarrierPrivilegeStatus.contains(carrierPrivilegeStatusResult));

        // Verify TelephonyManager.getCarrierPrivilegedPackagesForAllActiveSubscriptions
        List<String> resultForGetCarrierPrivilegedApis =
                ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                        (tm) -> tm.getCarrierPrivilegedPackagesForAllActiveSubscriptions());
        assertNotNull(resultForGetCarrierPrivilegedApis);
        for (String result : resultForGetCarrierPrivilegedApis) {
            assertFalse(TextUtils.isEmpty(result));
        }

        mTelephonyManager.getDefaultRespondViaMessageApplication();
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                TelephonyManager::getAndUpdateDefaultRespondViaMessageApplication);
    }

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    @Test
    public void testGetCallForwarding() {
        List<Integer> callForwardingReasons = new ArrayList<>();
        callForwardingReasons.add(CallForwardingInfo.REASON_UNCONDITIONAL);
        callForwardingReasons.add(CallForwardingInfo.REASON_BUSY);
        callForwardingReasons.add(CallForwardingInfo.REASON_NO_REPLY);
        callForwardingReasons.add(CallForwardingInfo.REASON_NOT_REACHABLE);
        callForwardingReasons.add(CallForwardingInfo.REASON_ALL);
        callForwardingReasons.add(CallForwardingInfo.REASON_ALL_CONDITIONAL);

        Set<Integer> callForwardingStatus = new HashSet<Integer>();
        callForwardingStatus.add(CallForwardingInfo.STATUS_INACTIVE);
        callForwardingStatus.add(CallForwardingInfo.STATUS_ACTIVE);
        callForwardingStatus.add(CallForwardingInfo.STATUS_FDN_CHECK_FAILURE);
        callForwardingStatus.add(CallForwardingInfo.STATUS_UNKNOWN_ERROR);
        callForwardingStatus.add(CallForwardingInfo.STATUS_NOT_SUPPORTED);

        for (int callForwardingReasonToGet : callForwardingReasons) {
            Log.d(TAG, "[testGetCallForwarding] callForwardingReasonToGet: "
                    + callForwardingReasonToGet);
            CallForwardingInfo callForwardingInfo =
                    ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                            (tm) -> tm.getCallForwarding(callForwardingReasonToGet));

            assertNotNull(callForwardingInfo);
            assertTrue(callForwardingStatus.contains(callForwardingInfo.getStatus()));
            assertTrue(callForwardingReasons.contains(callForwardingInfo.getReason()));
            callForwardingInfo.getNumber();
            assertTrue(callForwardingInfo.getTimeoutSeconds() >= 0);
        }
    }
     */

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    @Test
    public void testSetCallForwarding() {
        List<Integer> callForwardingReasons = new ArrayList<>();
        callForwardingReasons.add(CallForwardingInfo.REASON_UNCONDITIONAL);
        callForwardingReasons.add(CallForwardingInfo.REASON_BUSY);
        callForwardingReasons.add(CallForwardingInfo.REASON_NO_REPLY);
        callForwardingReasons.add(CallForwardingInfo.REASON_NOT_REACHABLE);
        callForwardingReasons.add(CallForwardingInfo.REASON_ALL);
        callForwardingReasons.add(CallForwardingInfo.REASON_ALL_CONDITIONAL);

        // Enable Call Forwarding
        for (int callForwardingReasonToEnable : callForwardingReasons) {
            final CallForwardingInfo callForwardingInfoToEnable = new CallForwardingInfo(
                    CallForwardingInfo.STATUS_ACTIVE,
                    callForwardingReasonToEnable,
                    TEST_FORWARD_NUMBER,
                    // time seconds
                    1);
            Log.d(TAG, "[testSetCallForwarding] Enable Call Forwarding. Status: "
                    + CallForwardingInfo.STATUS_ACTIVE + " Reason: "
                    + callForwardingReasonToEnable + " Number: " + TEST_FORWARD_NUMBER
                    + " Time Seconds: 1");
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.setCallForwarding(callForwardingInfoToEnable));
        }

        // Disable Call Forwarding
        for (int callForwardingReasonToDisable : callForwardingReasons) {
            final CallForwardingInfo callForwardingInfoToDisable = new CallForwardingInfo(
                    CallForwardingInfo.STATUS_INACTIVE,
                    callForwardingReasonToDisable,
                    TEST_FORWARD_NUMBER,
                    // time seconds
                    1);
            Log.d(TAG, "[testSetCallForwarding] Disable Call Forwarding. Status: "
                    + CallForwardingInfo.STATUS_INACTIVE + " Reason: "
                    + callForwardingReasonToDisable + " Number: " + TEST_FORWARD_NUMBER
                    + " Time Seconds: 1");
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.setCallForwarding(callForwardingInfoToDisable));
        }
    }
    */

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    @Test
    public void testGetCallWaitingStatus() {
        Set<Integer> callWaitingStatus = new HashSet<Integer>();
        callWaitingStatus.add(TelephonyManager.CALL_WAITING_STATUS_ACTIVE);
        callWaitingStatus.add(TelephonyManager.CALL_WAITING_STATUS_INACTIVE);
        callWaitingStatus.add(TelephonyManager.CALL_WAITING_STATUS_UNKNOWN_ERROR);
        callWaitingStatus.add(TelephonyManager.CALL_WAITING_STATUS_NOT_SUPPORTED);

        int status = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> tm.getCallWaitingStatus());
        assertTrue(callWaitingStatus.contains(status));
    }
     */

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    @Test
    public void testSetCallWaitingStatus() {
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.setCallWaitingStatus(true));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.setCallWaitingStatus(false));
    }
     */

    @Test
    public void testGetRadioHalVersion() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG,"skipping test on device without FEATURE_TELEPHONY present");
            return;
        }

        Pair<Integer, Integer> version = mTelephonyManager.getRadioHalVersion();

        // The version must be valid, and the versions start with 1.0
        assertFalse("Invalid Radio HAL Version: " + version,
                version.first < 1 || version.second < 0);
    }

    @Test
    public void testCreateForPhoneAccountHandle() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        if (!mTelephonyManager.isVoiceCapable()) {
            Log.d(TAG, "Skipping test that requires config_voice_capable is true");
            return;
        }
        TelecomManager telecomManager = getContext().getSystemService(TelecomManager.class);
        PhoneAccountHandle handle =
                telecomManager.getDefaultOutgoingPhoneAccount(PhoneAccount.SCHEME_TEL);
        TelephonyManager telephonyManager = mTelephonyManager.createForPhoneAccountHandle(handle);
        String globalSubscriberId = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> tm.getSubscriberId());
        String localSubscriberId = ShellIdentityUtils.invokeMethodWithShellPermissions(
                telephonyManager, (tm) -> tm.getSubscriberId());
        assertEquals(globalSubscriberId, localSubscriberId);
    }

    @Test
    public void testCreateForPhoneAccountHandle_InvalidHandle(){
        PhoneAccountHandle handle =
                new PhoneAccountHandle(new ComponentName("com.example.foo", "bar"), "baz");
        assertNull(mTelephonyManager.createForPhoneAccountHandle(handle));
    }

    /**
     * Tests that the phone count returned is valid.
     */
    @Test
    public void testGetPhoneCount() {
        int phoneCount = mTelephonyManager.getPhoneCount();
        int phoneType = mTelephonyManager.getPhoneType();
        switch (phoneType) {
            case TelephonyManager.PHONE_TYPE_GSM:
            case TelephonyManager.PHONE_TYPE_CDMA:
                assertTrue("Phone count should be > 0", phoneCount > 0);
                break;
            case TelephonyManager.PHONE_TYPE_NONE:
                assertTrue("Phone count should be 0", phoneCount == 0 || phoneCount == 1);
                break;
            default:
                throw new IllegalArgumentException("Did you add a new phone type? " + phoneType);
        }
    }

    /**
     * Tests that the device properly reports either a valid IMEI, MEID/ESN, or a valid MAC address
     * if only a WiFi device. At least one of them must be valid.
     */
    @Test
    public void testGetDeviceId() {
        String deviceId = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getDeviceId());
        verifyDeviceId(deviceId);
    }

    /**
     * Tests the max number of active SIMs method
     */
    @Test
    public void testGetMaxNumberOfSimultaneouslyActiveSims() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }

        int maxNum = mTelephonyManager.getMaxNumberOfSimultaneouslyActiveSims();
        assertTrue(maxNum >= 1);
    }

    /**
     * Tests that the device properly reports either a valid IMEI, MEID/ESN, or a valid MAC address
     * if only a WiFi device. At least one of them must be valid.
     */
    @Test
    public void testGetDeviceIdForSlot() {
        String deviceId = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getDeviceId(mTelephonyManager.getSlotIndex()));
        verifyDeviceId(deviceId);
        // Also verify that no exception is thrown for any slot index (including invalid ones)
        for (int i = -1; i <= mTelephonyManager.getPhoneCount(); i++) {
            // The compiler error 'local variables referenced from a lambda expression must be final
            // or effectively final' is reported when using i, so assign it to a final variable.
            final int currI = i;
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getDeviceId(currI));
        }
    }

    private void verifyDeviceId(String deviceId) {
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            // Either IMEI or MEID need to be valid.
            try {
                assertImei(deviceId);
            } catch (AssertionError e) {
                assertMeidEsn(deviceId);
            }
        } else if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_WIFI)) {
            assertSerialNumber();
            assertMacAddress(getWifiMacAddress());
        } else if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH)) {
            assertSerialNumber();
            assertMacAddress(getBluetoothMacAddress());
        } else if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_ETHERNET)) {
            assertTrue(mCm.getNetworkInfo(ConnectivityManager.TYPE_ETHERNET) != null);
        }
    }

    private static void assertImei(String id) {
        assertFalse("Imei should not be empty or null", TextUtils.isEmpty(id));
        // IMEI may include the check digit
        String imeiPattern = "[0-9]{14,15}";
        String invalidPattern = "[0]{14,15}";
        assertTrue("IMEI " + id + " does not match pattern " + imeiPattern,
                Pattern.matches(imeiPattern, id));
        assertFalse("IMEI " + id + " must not be a zero sequence" + invalidPattern,
                Pattern.matches(invalidPattern, id));
        if (id.length() == 15) {
            // if the ID is 15 digits, the 15th must be a check digit.
            assertImeiCheckDigit(id);
        }
    }

    private static void assertImeiCheckDigit(String deviceId) {
        int expectedCheckDigit = getLuhnCheckDigit(deviceId.substring(0, 14));
        int actualCheckDigit = Character.digit(deviceId.charAt(14), 10);
        assertEquals("Incorrect check digit for " + deviceId, expectedCheckDigit, actualCheckDigit);
    }

    /**
     * Use decimal value (0-9) to index into array to get sum of its digits
     * needed by Lunh check.
     *
     * Example: DOUBLE_DIGIT_SUM[6] = 3 because 6 * 2 = 12 => 1 + 2 = 3
     */
    private static final int[] DOUBLE_DIGIT_SUM = {0, 2, 4, 6, 8, 1, 3, 5, 7, 9};

    /**
     * Calculate the check digit by starting from the right, doubling every
     * each digit, summing all the digits including the doubled ones, and
     * finding a number to make the sum divisible by 10.
     *
     * @param deviceId not including the check digit
     * @return the check digit
     */
    private static int getLuhnCheckDigit(String deviceId) {
        int sum = 0;
        int dontDoubleModulus = deviceId.length() % 2;
        for (int i = deviceId.length() - 1; i >= 0; --i) {
            int digit = Character.digit(deviceId.charAt(i), 10);
            if (i % 2 == dontDoubleModulus) {
                sum += digit;
            } else {
                sum += DOUBLE_DIGIT_SUM[digit];
            }
        }
        sum %= 10;
        return sum == 0 ? 0 : 10 - sum;
    }

    private static void assertMeidEsn(String id) {
        // CDMA device IDs may either be a 14-hex-digit MEID or an
        // 8-hex-digit ESN.  If it's an ESN, it may not be a
        // pseudo-ESN.
        assertFalse("Meid ESN should not be empty or null", TextUtils.isEmpty(id));
        if (id.length() == 14) {
            assertMeidFormat(id);
        } else if (id.length() == 8) {
            assertHexadecimalEsnFormat(id);
        } else {
            fail("device id on CDMA must be 14-digit hex MEID or 8-digit hex ESN.");
        }
    }

    private static void assertHexadecimalEsnFormat(String deviceId) {
        String esnPattern = "[0-9a-fA-F]{8}";
        String invalidPattern = "[0]{8}";
        assertTrue("ESN hex device id " + deviceId + " does not match pattern " + esnPattern,
                Pattern.matches(esnPattern, deviceId));
        assertFalse("ESN hex device id " + deviceId + " must not be a pseudo-ESN",
                "80".equals(deviceId.substring(0, 2)));
        assertFalse("ESN hex device id " + deviceId + "must not be a zero sequence",
                Pattern.matches(invalidPattern, deviceId));
    }

    private static void assertMeidFormat(String deviceId) {
        // MEID must NOT include the check digit.
        String meidPattern = "[0-9a-fA-F]{14}";
        String invalidPattern = "[0]{14}";
        assertTrue("MEID device id " + deviceId + " does not match pattern "
                + meidPattern, Pattern.matches(meidPattern, deviceId));
        assertFalse("MEID device id " + deviceId + "must not be a zero sequence",
                Pattern.matches(invalidPattern, deviceId));
    }

    private void assertSerialNumber() {
        String serial = ShellIdentityUtils.invokeStaticMethodWithShellPermissions(
                Build::getSerial);
        assertNotNull("Non-telephony devices must have a Build.getSerial() number.",
                serial);
        assertTrue("Hardware id must be no longer than 20 characters.",
                serial.length() <= 20);
        assertTrue("Hardware id must be alphanumeric.",
                Pattern.matches("[0-9A-Za-z]+", serial));
    }

    private void assertMacAddress(String macAddress) {
        String macPattern = "([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}";
        assertTrue("MAC Address " + macAddress + " does not match pattern " + macPattern,
                Pattern.matches(macPattern, macAddress));
    }

    /** @return mac address which requires the WiFi system to be enabled */
    private String getWifiMacAddress() {
        WifiManager wifiManager = getContext().getSystemService(WifiManager.class);

        if (wifiManager.isWifiEnabled()) {
            return wifiManager.getConnectionInfo().getMacAddress();
        } else {
            try {
                runWithShellPermissionIdentity(() -> wifiManager.setWifiEnabled(true));

                return wifiManager.getConnectionInfo().getMacAddress();

            } finally {
                runWithShellPermissionIdentity(() -> wifiManager.setWifiEnabled(false));
            }
        }
    }

    private String getBluetoothMacAddress() {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            return "";
        }

        return adapter.getAddress();
    }

    private static final String ISO_COUNTRY_CODE_PATTERN = "[a-z]{2}";

    @Test
    public void testGetNetworkCountryIso() {
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            String countryCode = mTelephonyManager.getNetworkCountryIso();
            assertTrue("Country code '" + countryCode + "' did not match "
                            + ISO_COUNTRY_CODE_PATTERN,
                    Pattern.matches(ISO_COUNTRY_CODE_PATTERN, countryCode));

            for (int i = 0; i < mTelephonyManager.getPhoneCount(); i++) {
                countryCode = mTelephonyManager.getNetworkCountryIso(i);

                assertTrue("Country code '" + countryCode + "' did not match "
                                + ISO_COUNTRY_CODE_PATTERN + " for slot " + i,
                        Pattern.matches(ISO_COUNTRY_CODE_PATTERN, countryCode));
            }
        } else {
            // Non-telephony may still have the property defined if it has a SIM.
        }
    }

    @Test
    public void testSetSystemSelectionChannels() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "skipping test on device without FEATURE_TELEPHONY present");
            return;
        }
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue<>(1);
        final UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation().getUiAutomation();
        try {
            uiAutomation.adoptShellPermissionIdentity();
            // This is a oneway binder call, meaning we may return before the permission check
            // happens. Hold shell permissions until we get a response.
            mTelephonyManager.setSystemSelectionChannels(Collections.emptyList(),
                    getContext().getMainExecutor(), queue::offer);
            Boolean result = queue.poll(1000, TimeUnit.MILLISECONDS);
            // Ensure we get a result
            assertNotNull(result);
            // Only verify the result for supported devices on IRadio 1.3+
            if (mRadioVersion >= RADIO_HAL_VERSION_1_3) {
                assertTrue(result);
            }
        } catch (InterruptedException e) {
            fail("interrupted");
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }

        // Try calling the API that doesn't provide feedback. We have no way of knowing if it
        // succeeds, so just make sure nothing crashes.
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                tp -> tp.setSystemSelectionChannels(Collections.emptyList()));
    }

    @Test
    public void testGetSimCountryIso() {
        String countryCode = mTelephonyManager.getSimCountryIso();
        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            assertTrue("Country code '" + countryCode + "' did not match "
                            + ISO_COUNTRY_CODE_PATTERN,
                    Pattern.matches(ISO_COUNTRY_CODE_PATTERN, countryCode));
        } else {
            // Non-telephony may still have the property defined if it has a SIM.
        }
    }

    @Test
    public void testResetSettings() throws Exception {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "skipping test on device without FEATURE_TELEPHONY present");
            return;
        }

        UserManager userManager = getContext().getSystemService(UserManager.class);

        boolean canChangeMobileNetworkSettings = userManager != null
                && !userManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_MOBILE_NETWORKS);
        assertTrue("Primary user must be able to configure mobile networks to pass this test",
                canChangeMobileNetworkSettings);

        //First check permissions are correct
        try {
            mTelephonyManager.resetSettings();
            fail("TelephonyManager#resetSettings requires the"
                    + " android.Manifest.permission.NETWORK_SETTINGS permission");
        } catch (SecurityException e) {
            //expected
        }
        // and then do a reset to move data to default.
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    TelephonyManager::resetSettings, "android.permission.NETWORK_SETTINGS");
        } catch (SecurityException e) {
            fail("TelephonyManager#resetSettings requires the"
                    + " android.Manifest.permission.NETWORK_SETTINGS permission");
        }

        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue<>(2);
        final ContentObserver mobileDataChangeObserver = new ContentObserver(
                new Handler(Looper.getMainLooper())) {
            @Override
            public void onChange(boolean selfChange) {
                queue.offer(isDataEnabled());
            }
        };

        getContext().getContentResolver().registerContentObserver(
                getObservableDataEnabledUri(mTestSub), /* notifyForDescendants= */ false,
                mobileDataChangeObserver);
        boolean defaultDataSetting = isDataEnabled();

        // set data to not the default!
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                tm -> tm.setDataEnabled(!defaultDataSetting));
        Boolean dataChangedResult = queue.poll(TOLERANCE, TimeUnit.MILLISECONDS);
        assertNotNull("Data setting was not changed", dataChangedResult);
        assertEquals("Data enable change didn't work", !defaultDataSetting,
                dataChangedResult);

        // and then do a reset to move data to default again.
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    TelephonyManager::resetSettings, "android.permission.NETWORK_SETTINGS");
        } catch (SecurityException e) {
            fail("TelephonyManager#resetSettings requires the"
                    + " android.Manifest.permission.NETWORK_SETTINGS permission");
        }
        dataChangedResult = queue.poll(TOLERANCE, TimeUnit.MILLISECONDS);
        assertNotNull("Data setting was not changed", dataChangedResult);
        assertEquals("resetSettings did not reset default data", defaultDataSetting,
                dataChangedResult);
    }

    @Test
    public void testGetServiceState() throws InterruptedException {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        TestThread t = new TestThread(new Runnable() {
            public void run() {
                Looper.prepare();

                mListener = new PhoneStateListener() {
                    @Override
                    public void onServiceStateChanged(ServiceState serviceState) {
                        synchronized (mLock) {
                            mServiceState = serviceState;
                            mLock.notify();
                        }
                    }
                };
                mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
                Looper.loop();
            }
        });

        synchronized (mLock) {
            t.start();
            mLock.wait(TOLERANCE);
        }

        assertEquals(mServiceState, mTelephonyManager.getServiceState());
    }

    @Test
    public void testGetSimLocale() throws InterruptedException {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG,"skipping test that requires Telephony");
            return;
        }
        if (SubscriptionManager.getDefaultSubscriptionId()
                == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            fail("Expected SIM inserted");
        }
        Locale locale = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getSimLocale());
        Log.d(TAG, "testGetSimLocale: " + locale);
        assertNotNull(locale);
    }

    /**
     * Tests that a GSM device properly reports either the correct TAC (type allocation code) or
     * null.
     * The TAC should match the first 8 digits of the IMEI.
     */
    @Test
    public void testGetTac() {
        String tac = mTelephonyManager.getTypeAllocationCode();
        String imei = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei());

        if (tac == null || imei == null) {
            return;
        }

        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_GSM) {
                assertEquals(imei.substring(0, 8), tac);
            }
        }
    }

    /**
     * Tests that a CDMA device properly reports either the correct MC (manufacturer code) or null.
     * The MC should match the first 8 digits of the MEID.
     */
    @Test
    public void testGetMc() {
        String mc = mTelephonyManager.getManufacturerCode();
        String meid = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getMeid());

        if (mc == null || meid == null) {
            return;
        }

        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
                assertEquals(meid.substring(0, 8), mc);
            }
        }
    }

    /**
     * Tests that the device properly reports either a valid IMEI or null.
     */
    @Test
    public void testGetImei() {
        String imei = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei());

        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_GSM) {
                assertImei(imei);
            }
        }
    }

    /**
     * Tests that the device properly reports either a valid IMEI or null.
     */
    @Test
    public void testGetImeiForSlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        for (int i = 0; i < mTelephonyManager.getPhoneCount(); i++) {
            // The compiler error 'local variables referenced from a lambda expression must be final
            // or effectively final' is reported when using i, so assign it to a final variable.
            final int currI = i;
            String imei = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getImei(currI));
            if (!TextUtils.isEmpty(imei)) {
                assertImei(imei);
            }
        }

        // Also verify that no exception is thrown for any slot index (including invalid ones)
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei(-1));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getImei(mTelephonyManager.getPhoneCount()));
    }

    /**
     * Verifies that {@link TelephonyManager#getRadioPowerState()} does not throw any exception
     * and returns radio on.
     */
    @Test
    public void testGetRadioPowerState() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        // Also verify that no exception is thrown.
        assertThat(mTelephonyManager.getRadioPowerState()).isEqualTo(
                TelephonyManager.RADIO_POWER_ON);
    }

    /**
     * Verifies that {@link TelephonyManager#setCarrierDataEnabled(boolean)} does not throw any
     * exception. TODO enhance later if we have an API to get data enabled state.
     */
    @Test
    public void testSetCarrierDataEnabled() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // Also verify that no exception is thrown.
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.setCarrierDataEnabled(false));
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.setCarrierDataEnabled(true));
    }

    /**
     * Verifies that {@link TelephonyManager#rebootRadio()} does not throw any exception
     * and final radio state is radio power on.
     */
    @Test
    public void testRebootRadio() throws Throwable {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        assertEquals(mTelephonyManager.getServiceState().getState(), ServiceState.STATE_IN_SERVICE);

        TestThread t = new TestThread(new Runnable() {
            public void run() {
                Looper.prepare();

                mListener = new PhoneStateListener() {
                    @Override
                    public void onRadioPowerStateChanged(
                            @RadioPowerState int state) {
                        synchronized (mLock) {
                            if (state == TelephonyManager.RADIO_POWER_ON && mHasRadioPowerOff) {
                                mRadioRebootTriggered = true;
                                mLock.notify();
                            } else if (state == TelephonyManager.RADIO_POWER_OFF) {
                                // reboot must go to power off
                                mHasRadioPowerOff = true;
                            }
                        }
                    }
                };
                ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                        (tm) -> tm.listen(mListener,
                                PhoneStateListener.LISTEN_RADIO_POWER_STATE_CHANGED));
                Looper.loop();
            }
        });

        assertThat(mTelephonyManager.getRadioPowerState()).isEqualTo(
                TelephonyManager.RADIO_POWER_ON);
        assertThat(mRadioRebootTriggered).isFalse();
        assertThat(mHasRadioPowerOff).isFalse();
        boolean success = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.rebootRadio());
        //skip this test if not supported or unsuccessful (success=false)
        if(!success) {
            return;
        }

        t.start();
        synchronized (mLock) {
            // reboot takes longer time
            if (!mRadioRebootTriggered) {
                mLock.wait(10000);
            }
        }
        assertThat(mTelephonyManager.getRadioPowerState()).isEqualTo(
                TelephonyManager.RADIO_POWER_ON);
        assertThat(mRadioRebootTriggered).isTrue();

        // note, other telephony states might not resumes properly at this point. e.g, service state
        // might still in the transition from OOS to In service. Thus we need to wait for in
        // service state before running next tests.
        t = new TestThread(new Runnable() {
            public void run() {
                Looper.prepare();

                mListener = new PhoneStateListener() {
                    @Override
                    public void onServiceStateChanged(ServiceState serviceState) {
                        synchronized (mLock) {
                            if (serviceState.getState() == ServiceState.STATE_IN_SERVICE) {
                                mServiceStateChangedCalled = true;
                                mLock.notify();
                            }
                        }
                    }
                };
                ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                        (tm) -> tm.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE));
                Looper.loop();
            }
        });

        synchronized (mLock) {
            t.start();
            if (!mServiceStateChangedCalled) {
                mLock.wait(60000);
            }
        }
        assertThat(mTelephonyManager.getServiceState().getState()).isEqualTo(
                ServiceState.STATE_IN_SERVICE);
    }

    /**
     * Verifies that {@link TelephonyManager#getAidForAppType(int)} does not throw any exception
     * for all supported subscription app type.
     */
    @Test
    public void testGetAidForAppType() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getAidForAppType(TelephonyManager.APPTYPE_SIM));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getAidForAppType(TelephonyManager.APPTYPE_CSIM));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getAidForAppType(TelephonyManager.APPTYPE_RUIM));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getAidForAppType(TelephonyManager.APPTYPE_ISIM));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getAidForAppType(TelephonyManager.APPTYPE_USIM));
    }

    /**
     * Verifies that {@link TelephonyManager#getIsimDomain()} does not throw any exception
     */
    @Test
    public void testGetIsimDomain() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getIsimDomain());
    }

    /**
     * Verifies that {@link TelephonyManager#getIsimImpu()} does not throw any exception when called
     * and has the correct permissions.
     */
    @Ignore("API moved back to @hide for Android R.")
    @Test
    public void testGetIsimImpu() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                TelephonyManager::getIsimImpu);
        // Try without the correct permissions and ensure it fails.
        try {
            mTelephonyManager.getIsimImpu();
            fail();
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Basic test to ensure {@link NetworkRegistrationInfo#getRegisteredPlmn()} provides valid
     * information.
     */
    @Test
    public void testNetworkRegistrationInfoRegisteredPlmn() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // get NetworkRegistration object
        ServiceState ss = mTelephonyManager.getServiceState();
        assertNotNull(ss);

        boolean hasRegistered = false;
        for (NetworkRegistrationInfo nwReg : ss.getNetworkRegistrationInfoList()) {
            if (nwReg.isRegistered()
                        && nwReg.getTransportType() == AccessNetworkConstants.TRANSPORT_TYPE_WWAN) {
                hasRegistered = true;
                String plmnId = nwReg.getRegisteredPlmn();
                // CDMA doesn't have PLMN IDs. Rather than put CID|NID here, instead it will be
                // empty. It's a case that's becoming less important over time, but for now a
                // device that's only registered on CDMA needs to pass this test.
                if (nwReg.getCellIdentity() instanceof android.telephony.CellIdentityCdma) {
                    assertTrue(TextUtils.isEmpty(plmnId));
                } else {
                    assertFalse(TextUtils.isEmpty(plmnId));
                    assertTrue("PlmnId() out of range [00000 - 999999], PLMN ID=" + plmnId,
                            plmnId.matches("^[0-9]{5,6}$"));
                }
            }
        }
        assertTrue(hasRegistered);
    }

    /**
     * Basic test to ensure {@link NetworkRegistrationInfo#isRoaming()} does not throw any
     * exception.
     */
    @Test
    public void testNetworkRegistrationInfoIsRoaming() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // get NetworkRegistration object
        NetworkRegistrationInfo nwReg = mTelephonyManager.getServiceState()
                .getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_CS,
                        AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        assertThat(nwReg).isNotNull();
        nwReg.isRoaming();
    }

    /**
     * Basic test to ensure {@link NetworkRegistrationInfo#getRoamingType()} ()} does not throw any
     * exception and returns valid result
     * @see ServiceState.RoamingType
     */
    @Test
    public void testNetworkRegistrationInfoGetRoamingType() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // get NetworkRegistration object for voice
        NetworkRegistrationInfo nwReg = mTelephonyManager.getServiceState()
                .getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_CS,
                        AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        assertNotNull(nwReg);
        assertThat(nwReg.getRoamingType()).isIn(ROAMING_TYPES);

        // getNetworkRegistration object for data
        // get NetworkRegistration object for voice
        nwReg = mTelephonyManager.getServiceState()
                .getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_PS,
                        AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        assertThat(nwReg).isNotNull();
        assertThat(nwReg.getRoamingType()).isIn(ROAMING_TYPES);
    }

    /**
     * Basic test to ensure {@link NetworkRegistrationInfo#getAccessNetworkTechnology()} not
     * throw any exception and returns valid result
     * @see android.telephony.Annotation.NetworkType
     */
    @Test
    public void testNetworkRegistationStateGetAccessNetworkTechnology() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // get NetworkRegistration object for voice
        NetworkRegistrationInfo nwReg = mTelephonyManager.getServiceState()
                .getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_CS,
                        AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        assertThat(nwReg).isNotNull();
        assertThat(nwReg.getAccessNetworkTechnology()).isIn(NETWORK_TYPES);

        // get NetworkRegistation object for data
        nwReg = mTelephonyManager.getServiceState()
                .getNetworkRegistrationInfo(NetworkRegistrationInfo.DOMAIN_PS,
                        AccessNetworkConstants.TRANSPORT_TYPE_WWAN);
        assertThat(nwReg).isNotNull();
        assertThat(nwReg.getAccessNetworkTechnology()).isIn(NETWORK_TYPES);
    }


    /**
     * Tests that the device properly reports either a valid MEID or null.
     */
    @Test
    public void testGetMeid() {
        String meid = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getMeid());

        if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            if (mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
                assertMeidEsn(meid);
            }
        }
    }

    /**
     * Tests that the device properly reports either a valid MEID or null.
     */
    @Test
    public void testGetMeidForSlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        SubscriptionManager sm = getContext().getSystemService(SubscriptionManager.class);
        List<SubscriptionInfo> subInfos = sm.getActiveSubscriptionInfoList();

        if (subInfos != null) {
            for (SubscriptionInfo subInfo : subInfos) {
                int slotIndex = subInfo.getSimSlotIndex();
                int subId = subInfo.getSubscriptionId();
                TelephonyManager tm = mTelephonyManager.createForSubscriptionId(subId);
                if (tm.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
                    String meid = ShellIdentityUtils.invokeMethodWithShellPermissions(
                            mTelephonyManager,
                            (telephonyManager) -> telephonyManager.getMeid(slotIndex));

                    if (!TextUtils.isEmpty(meid)) {
                        assertMeidEsn(meid);
                    }
                }
            }
        }

        // Also verify that no exception is thrown for any slot index (including invalid ones)
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getMeid(-1));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getMeid(mTelephonyManager.getPhoneCount()));
    }

    /**
     * Tests sendDialerSpecialCode API.
     * Expects a security exception since the caller does not have carrier privileges or is not the
     * current default dialer app.
     */
    @Test
    public void testSendDialerSpecialCode() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        try {
            mTelephonyManager.sendDialerSpecialCode("4636");
            fail("Expected SecurityException. App does not have carrier privileges or is not the "
                    + "default dialer app");
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests that the device properly reports the contents of EF_FPLMN or null
     */
    @Test
    public void testGetForbiddenPlmns() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        String[] plmns = mTelephonyManager.getForbiddenPlmns();

        int phoneType = mTelephonyManager.getPhoneType();
        switch (phoneType) {
            case TelephonyManager.PHONE_TYPE_GSM:
                assertNotNull("Forbidden PLMNs must be valid or an empty list!", plmns);
            case TelephonyManager.PHONE_TYPE_CDMA:
            case TelephonyManager.PHONE_TYPE_NONE:
                if (plmns == null) {
                    return;
                }
        }

        for(String plmn : plmns) {
            assertTrue(
                    "Invalid Length for PLMN-ID, must be 5 or 6! plmn=" + plmn,
                    plmn.length() >= 5 && plmn.length() <= 6);
            assertTrue(
                    "PLMNs must be strings of digits 0-9! plmn=" + plmn,
                    android.text.TextUtils.isDigitsOnly(plmn));
        }
    }

    /**
     * Tests that the device properly sets and pads the contents of EF_FPLMN
     */
    @Test
    public void testSetForbiddenPlmns() {
        if (!supportSetFplmn()) {
            return;
        }
        String[] originalFplmns = mTelephonyManager.getForbiddenPlmns();
        try {
            int numFplmnsSet = mTelephonyManager.setForbiddenPlmns(FPLMN_TEST);
            String[] writtenFplmns = mTelephonyManager.getForbiddenPlmns();
            assertEquals("Wrong return value for setFplmns with less than required fplmns: "
                    + numFplmnsSet, FPLMN_TEST.size(), numFplmnsSet);
            assertEquals("Wrong Fplmns content written", FPLMN_TEST, Arrays.asList(writtenFplmns));
        } finally {
            // Restore
            mTelephonyManager.setForbiddenPlmns(Arrays.asList(originalFplmns));
        }
    }

    /**
     * Tests that the device properly truncates the contents of EF_FPLMN when provided size
     * is too big.
     */
    @Test
    public void testSetForbiddenPlmnsTruncate() {
        if (!supportSetFplmn()) {
            return;
        }
        String[] originalFplmns = mTelephonyManager.getForbiddenPlmns();
        try {
            List<String> targetFplmns = new ArrayList<>();
            for (int i = 0; i < MIN_FPLMN_NUM; i++) {
                targetFplmns.add(PLMN_A);
            }
            for (int i = MIN_FPLMN_NUM; i < MAX_FPLMN_NUM; i++) {
                targetFplmns.add(PLMN_B);
            }
            int numFplmnsSet = mTelephonyManager.setForbiddenPlmns(targetFplmns);
            String[] writtenFplmns = mTelephonyManager.getForbiddenPlmns();
            assertTrue("Wrong return value for setFplmns with overflowing fplmns: " + numFplmnsSet,
                    numFplmnsSet < MAX_FPLMN_NUM);
            assertEquals("Number of Fplmns set does not equal number of Fplmns available",
                    numFplmnsSet, writtenFplmns.length);
            assertEquals("Wrong Fplmns content written", targetFplmns.subList(0, numFplmnsSet),
                    Arrays.asList(writtenFplmns));
        } finally {
            // Restore
            mTelephonyManager.setForbiddenPlmns(Arrays.asList(originalFplmns));
        }
    }

    /**
     * Tests that the device properly deletes the contents of EF_FPLMN
     */
    @Test
    public void testSetForbiddenPlmnsDelete() {
        if (!supportSetFplmn()) {
            return;
        }
        String[] originalFplmns = mTelephonyManager.getForbiddenPlmns();
        try {
            // Support test for empty SIM
            List<String> targetDummyFplmns = new ArrayList<>();
            for (int i = 0; i < MIN_FPLMN_NUM; i++) {
                targetDummyFplmns.add(PLMN_A);
            }
            mTelephonyManager.setForbiddenPlmns(targetDummyFplmns);
            String[] writtenDummyFplmns = mTelephonyManager.getForbiddenPlmns();
            assertEquals(targetDummyFplmns, Arrays.asList(writtenDummyFplmns));

            List<String> targetFplmns = new ArrayList<>();
            int numFplmnsSet = mTelephonyManager.setForbiddenPlmns(targetFplmns);
            String[] writtenFplmns = mTelephonyManager.getForbiddenPlmns();
            assertEquals("Wrong return value for setFplmns with empty list", 0, numFplmnsSet);
            assertEquals("Wrong number of Fplmns written", 0, writtenFplmns.length);
            // TODO wait for 10 minutes or so for the FPLMNS list to grow back
        } finally {
            // Restore
            mTelephonyManager.setForbiddenPlmns(Arrays.asList(originalFplmns));
        }
    }


    /**
     * Tests that setForbiddenPlmns properly handles null input
     */
    @Test
    public void testSetForbiddenPlmnsVoid() {
        if (!supportSetFplmn()) {
            return;
        }
        String[] originalFplmns = mTelephonyManager.getForbiddenPlmns();
        try {
            mTelephonyManager.setForbiddenPlmns(null);
            fail("Expected IllegalArgumentException. Null input is not allowed");
        } catch (IllegalArgumentException expected) {
        } finally {
            // Restore
            mTelephonyManager.setForbiddenPlmns(Arrays.asList(originalFplmns));
        }
    }

    /**
     * Tests that the device properly reports the contents of ManualNetworkSelectionPlmn
     * The setting is not persisted selection
     */
    @Test
    public void testGetManualNetworkSelectionPlmnNonPersisted() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeManual(
                     TESTING_PLMN/* operatorNumeric */, false /* persistSelection */));
            String plmn = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                     (tm) -> tm.getManualNetworkSelectionPlmn());
            assertEquals(TESTING_PLMN, plmn);
        } finally {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeAutomatic());
        }
    }

    /**
     * Tests that the device properly reports the contents of ManualNetworkSelectionPlmn
     * The setting is persisted selection
     */
    @Test
    public void testGetManualNetworkSelectionPlmnPersisted() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeManual(
                     TESTING_PLMN/* operatorNumeric */, true /* persistSelection */));
            String plmn = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                     (tm) -> tm.getManualNetworkSelectionPlmn());
            assertEquals(TESTING_PLMN, plmn);
        } finally {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeAutomatic());
        }
    }

    /**
     * Verify that TelephonyManager.getCardIdForDefaultEuicc returns a positive value or either
     * UNINITIALIZED_CARD_ID or UNSUPPORTED_CARD_ID.
     */
    @Test
    public void testGetCardIdForDefaultEuicc() {
        int cardId = mTelephonyManager.getCardIdForDefaultEuicc();
        assertTrue("Card ID for default EUICC is not a valid value",
                cardId == TelephonyManager.UNSUPPORTED_CARD_ID
                        || cardId == TelephonyManager.UNINITIALIZED_CARD_ID
                        || cardId >= 0);
    }

    /**
     * Tests that a SecurityException is thrown when trying to access UiccCardsInfo.
     */
    @Test
    public void testGetUiccCardsInfoException() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "skipping test on device without FEATURE_TELEPHONY present");
            return;
        }
        try {
            // Requires READ_PRIVILEGED_PHONE_STATE or carrier privileges
            List<UiccCardInfo> infos = mTelephonyManager.getUiccCardsInfo();
            fail("Expected SecurityException. App does not have carrier privileges");
        } catch (SecurityException e) {
        }
    }

    /**
     * Tests that UiccCardsInfo methods don't crash.
     */
    @Test
    public void testGetUiccCardsInfo() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        // Requires READ_PRIVILEGED_PHONE_STATE or carrier privileges
        List<UiccCardInfo> infos =
                ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getUiccCardsInfo());
        // test that these methods don't crash
        if (infos.size() > 0) {
            UiccCardInfo info = infos.get(0);
            info.getIccId();
            info.getEid();
            info.isRemovable();
            info.isEuicc();
            info.getCardId();
            info.getSlotIndex();
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getContext();
    }

    /**
     * Tests that the device properly reports the contents of NetworkSelectionMode
     */
    @Test
    public void testGetNetworkSelectionMode() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setNetworkSelectionModeAutomatic());
        } catch (Exception e) {
        }

        int networkMode = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getNetworkSelectionMode());

        assertEquals(TelephonyManager.NETWORK_SELECTION_MODE_AUTO, networkMode);
    }

    /**
     * Tests that the device properly sets the network selection mode to automatic.
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testSetNetworkSelectionModeAutomatic() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        try {
            mTelephonyManager.setNetworkSelectionModeAutomatic();
            fail("Expected SecurityException. App does not have carrier privileges.");
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests that the device properly asks the radio to connect to the input network and change
     * selection mode to manual.
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testSetNetworkSelectionModeManual() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        try {
            mTelephonyManager.setNetworkSelectionModeManual(
                    "" /* operatorNumeric */, false /* persistSelection */);
            fail("Expected SecurityException. App does not have carrier privileges.");
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests that the device properly check whether selection mode was manual.
     */
    @Test
    public void testIsManualNetworkSelectionAllowed() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            Log.d(TAG, "Skipping test that requires FEATURE_TELEPHONY");
            return;
        }
        assertTrue(ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.isManualNetworkSelectionAllowed()));
    }

    /**
     * Construct a CallAttributes object and test getters.
     */
    @Test
    public void testCallAttributes() {
        CallQuality cq = new CallQuality();
        PreciseCallState pcs = new PreciseCallState();
        CallAttributes ca = new CallAttributes(pcs, TelephonyManager.NETWORK_TYPE_UNKNOWN, cq);
        assertEquals(pcs, ca.getPreciseCallState());
        assertEquals(TelephonyManager.NETWORK_TYPE_UNKNOWN, ca.getNetworkType());
        assertEquals(cq, ca.getCallQuality());
    }

    /**
     * Checks that a zeroed-out default CallQuality object can be created
     */
    @Test
    public void testCallQuality() {
        CallQuality cq = new CallQuality();
        assertEquals(0, cq.getDownlinkCallQualityLevel());
        assertEquals(0, cq.getUplinkCallQualityLevel());
        assertEquals(0, cq.getCallDuration());
        assertEquals(0, cq.getNumRtpPacketsTransmitted());
        assertEquals(0, cq.getNumRtpPacketsReceived());
        assertEquals(0, cq.getNumRtpPacketsTransmittedLost());
        assertEquals(0, cq.getNumRtpPacketsNotReceived());
        assertEquals(0, cq.getAverageRelativeJitter());
        assertEquals(0, cq.getMaxRelativeJitter());
        assertEquals(0, cq.getAverageRoundTripTime());
        assertEquals(0, cq.getCodecType());
        assertEquals(false, cq.isRtpInactivityDetected());
        assertEquals(false, cq.isIncomingSilenceDetectedAtCallSetup());
        assertEquals(false, cq.isOutgoingSilenceDetectedAtCallSetup());
    }


    // Reference: packages/services/Telephony/ecc/input/eccdata.txt
    private static final Map<String, String> EMERGENCY_NUMBERS_FOR_COUNTRIES =
            new HashMap<String, String>() {{
                put("au", "000");
                put("ca", "911");
                put("de", "112");
                put("gb", "999");
                put("in", "112");
                put("jp", "110");
                put("sg", "999");
                put("tw", "110");
                put("us", "911");
            }};

    /**
     * Tests TelephonyManager.getEmergencyNumberList.
     *
     * Also enforce country-specific emergency number in CTS.
     */
    @Test
    public void testGetEmergencyNumberList() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        Map<Integer, List<EmergencyNumber>> emergencyNumberList =
                mTelephonyManager.getEmergencyNumberList();

        assertFalse(emergencyNumberList == null);

        checkEmergencyNumberFormat(emergencyNumberList);

        int defaultSubId = mSubscriptionManager.getDefaultSubscriptionId();
        for (Map.Entry<String, String> entry : EMERGENCY_NUMBERS_FOR_COUNTRIES.entrySet()) {
            if (mTelephonyManager.getNetworkCountryIso().equals(entry.getKey())) {
                assertTrue(checkIfEmergencyNumberListHasSpecificAddress(
                        emergencyNumberList.get(defaultSubId), entry.getValue()));
            }
        }
    }

    /**
     * Tests TelephonyManager.isEmergencyNumber.
     *
     * Also enforce country-specific emergency number in CTS.
     */
    @Test
    public void testIsEmergencyNumber() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        for (Map.Entry<String, String> entry : EMERGENCY_NUMBERS_FOR_COUNTRIES.entrySet()) {
            if (mTelephonyManager.getNetworkCountryIso().equals(entry.getKey())) {
                assertTrue(mTelephonyManager.isEmergencyNumber(entry.getValue()));
            }
        }
    }

    /**
     * Tests TelephonyManager.isPotentialEmergencyNumber.
     */
    @Test
    public void testIsPotentialEmergencyNumber() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        String countryIso = mTelephonyManager.getNetworkCountryIso();
        String potentialEmergencyAddress = "91112345";
        // According to com.android.i18n.phonenumbers.ShortNumberInfo, in
        // these countries, if extra digits are added to an emergency number,
        // it no longer connects to the emergency service.
        if (countryIso.equals("br") || countryIso.equals("cl") || countryIso.equals("ni")) {
            assertFalse(ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.isPotentialEmergencyNumber(potentialEmergencyAddress)));
        } else {
            assertTrue(ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.isPotentialEmergencyNumber(potentialEmergencyAddress)));
        }
    }

    /**
     * Tests {@link TelephonyManager#getSupportedRadioAccessFamily()}
     */
    @Test
    public void testGetRadioAccessFamily() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        long raf = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getSupportedRadioAccessFamily());
        assertThat(raf).isNotEqualTo(TelephonyManager.NETWORK_TYPE_BITMASK_UNKNOWN);
    }

    private static void assertSetOpportunisticSubSuccess(int value) {
        assertThat(value).isEqualTo(TelephonyManager.SET_OPPORTUNISTIC_SUB_SUCCESS);
    }

    private static void assertSetOpportunisticNoOpportunisticSub(int value) {
        assertThat(value).isEqualTo(
                TelephonyManager.SET_OPPORTUNISTIC_SUB_NO_OPPORTUNISTIC_SUB_AVAILABLE);
    }

    /**
     * Tests {@link TelephonyManager#setPreferredOpportunisticDataSubscription} and
     * {@link TelephonyManager#getPreferredOpportunisticDataSubscription}
     */
    @Test
    public void testPreferredOpportunisticDataSubscription() {
        int randomSubId = 1;
        int activeSubscriptionInfoCount = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mSubscriptionManager, (tm) -> tm.getActiveSubscriptionInfoCount());
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        if (mTelephonyManager.getPhoneCount() == 1) {
            return;
        }
        if (mTelephonyManager.getPhoneCount() == 2 && activeSubscriptionInfoCount != 2) {
            return;
        }
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.setPreferredOpportunisticDataSubscription(
                        SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, false,
                        null, null));
        // wait for the data change to take effect
        waitForMs(500);
        int subId =
                ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                        (tm) -> tm.getPreferredOpportunisticDataSubscription());
        assertThat(subId).isEqualTo(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        List<SubscriptionInfo> subscriptionInfoList =
                ShellIdentityUtils.invokeMethodWithShellPermissions(mSubscriptionManager,
                        (tm) -> tm.getOpportunisticSubscriptions());
        Consumer<Integer> callbackSuccess = TelephonyManagerTest::assertSetOpportunisticSubSuccess;
        Consumer<Integer> callbackNoOpSub =
                TelephonyManagerTest::assertSetOpportunisticNoOpportunisticSub;
        if (subscriptionInfoList == null || subscriptionInfoList.size() == 0) {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setPreferredOpportunisticDataSubscription(randomSubId, false,
                            AsyncTask.SERIAL_EXECUTOR, callbackNoOpSub));
            // wait for the data change to take effect
            waitForMs(500);
            subId = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getPreferredOpportunisticDataSubscription());
            assertThat(subId).isEqualTo(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
        } else {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.setPreferredOpportunisticDataSubscription(
                            subscriptionInfoList.get(0).getSubscriptionId(), false,
                            AsyncTask.SERIAL_EXECUTOR, callbackSuccess));
            // wait for the data change to take effect
            waitForMs(500);
            subId = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getPreferredOpportunisticDataSubscription());
            assertThat(subId).isEqualTo(subscriptionInfoList.get(0).getSubscriptionId());
        }

        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.setPreferredOpportunisticDataSubscription(
                        SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, false,
                        AsyncTask.SERIAL_EXECUTOR, callbackSuccess));
        // wait for the data change to take effect
        waitForMs(500);
        subId = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getPreferredOpportunisticDataSubscription());
        assertThat(subId).isEqualTo(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID);
    }

    private static void assertUpdateAvailableNetworkSuccess(int value) {
        assertThat(value).isEqualTo(TelephonyManager.UPDATE_AVAILABLE_NETWORKS_SUCCESS);
    }

    private static void assertUpdateAvailableNetworkNoOpportunisticSub(int value) {
        assertThat(value).isEqualTo(
                TelephonyManager.UPDATE_AVAILABLE_NETWORKS_NO_OPPORTUNISTIC_SUB_AVAILABLE);
    }

    private static boolean checkIfEmergencyNumberListHasSpecificAddress(
            List<EmergencyNumber> emergencyNumberList, String address) {
        for (EmergencyNumber emergencyNumber : emergencyNumberList) {
            if (address.equals(emergencyNumber.getNumber())) {
                return true;
            }
        }
        return false;
    }

    private static void checkEmergencyNumberFormat(
            Map<Integer, List<EmergencyNumber>> emergencyNumberLists) {
        for (List<EmergencyNumber> emergencyNumberList : emergencyNumberLists.values()) {
            for (EmergencyNumber emergencyNumber : emergencyNumberList) {

                // Validate Emergency number address
                assertTrue(validateEmergencyNumberAddress(emergencyNumber.getNumber()));

                // Validate Emergency number country Iso
                assertTrue(validateEmergencyNumberCountryIso(emergencyNumber.getCountryIso()));

                // Validate Emergency number mnc
                assertTrue(validateEmergencyNumberMnc(emergencyNumber.getMnc()));

                // Validate Emergency service category list
                assertTrue(validateEmergencyServiceCategoryList(
                        emergencyNumber.getEmergencyServiceCategories()));

                // Validate Emergency number source list
                assertTrue(validateEmergencyNumberSourceList(
                        emergencyNumber.getEmergencyNumberSources()));

                // Validate Emergency URN list
                // (just verify it is not null, because the support of this field is optional)
                assertTrue(emergencyNumber.getEmergencyUrns() != null);

                // Validat Emergency call routing
                assertTrue(validateEmergencyCallRouting(
                        emergencyNumber.getEmergencyCallRouting()));

                // Valid the emergency number should be at least in a valid source.
                assertTrue(validateEmergencyNumberFromAnySource(emergencyNumber));

                // Valid the emergency number should be at least in a valid category.
                assertTrue(validateEmergencyNumberInAnyCategory(emergencyNumber));
            }

            // Validate compareTo
            assertTrue(validateEmergencyNumberCompareTo(emergencyNumberList));
        }
    }

    /**
     * Tests {@link TelephonyManager#updateAvailableNetworks}
     */
    @Test
    public void testUpdateAvailableNetworks() {
        int randomSubId = 1;
        int activeSubscriptionInfoCount = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mSubscriptionManager, (tm) -> tm.getActiveSubscriptionInfoCount());
        boolean isOpportunisticNetworkEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> tm.isOpportunisticNetworkEnabled());

        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        if (!isOpportunisticNetworkEnabled) {
            return;
        }
        if (mTelephonyManager.getPhoneCount() == 1) {
            return;
        }
        if (mTelephonyManager.getPhoneCount() == 2 && activeSubscriptionInfoCount != 2) {
            return;
        }

        List<SubscriptionInfo> subscriptionInfoList =
                ShellIdentityUtils.invokeMethodWithShellPermissions(mSubscriptionManager,
                        (tm) -> tm.getOpportunisticSubscriptions());
        List<String> mccMncs = new ArrayList<String>();
        List<Integer> bands = new ArrayList<Integer>();
        List<AvailableNetworkInfo> availableNetworkInfos = new ArrayList<AvailableNetworkInfo>();
        Consumer<Integer> callbackSuccess =
                TelephonyManagerTest::assertUpdateAvailableNetworkSuccess;
        Consumer<Integer> callbackNoOpSub =
                TelephonyManagerTest::assertUpdateAvailableNetworkNoOpportunisticSub;
        if (subscriptionInfoList == null || subscriptionInfoList.size() == 0
                || !mSubscriptionManager.isActiveSubscriptionId(
                subscriptionInfoList.get(0).getSubscriptionId())) {
            AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(randomSubId,
                    AvailableNetworkInfo.PRIORITY_HIGH, mccMncs, bands);
            availableNetworkInfos.add(availableNetworkInfo);
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.updateAvailableNetworks(availableNetworkInfos,
                            AsyncTask.SERIAL_EXECUTOR, callbackNoOpSub));
            // wait for the data change to take effect
            waitForMs(500);
            // clear all the operations at the end of test.
            availableNetworkInfos.clear();
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.updateAvailableNetworks(availableNetworkInfos,
                            AsyncTask.SERIAL_EXECUTOR, callbackNoOpSub));
        } else {
            AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(
                    subscriptionInfoList.get(0).getSubscriptionId(),
                    AvailableNetworkInfo.PRIORITY_HIGH, mccMncs, bands);
            availableNetworkInfos.add(availableNetworkInfo);
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.updateAvailableNetworks(availableNetworkInfos,
                            AsyncTask.SERIAL_EXECUTOR, callbackSuccess));
            // wait for the data change to take effect
            waitForMs(500);
            // clear all the operations at the end of test.
            availableNetworkInfos.clear();
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.updateAvailableNetworks(availableNetworkInfos,
                            AsyncTask.SERIAL_EXECUTOR, callbackSuccess));
        }
    }

    @Test
    public void testSwitchMultiSimConfig() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        try {
            mTelephonyManager.switchMultiSimConfig(mTelephonyManager.getActiveModemCount());
            fail("TelephonyManager#switchMultiSimConfig should require the MODIFY_PHONE_STATE"
                    + " permission to access.");
        } catch (SecurityException e) {
            // expected
        }
        try {
            // This should result in no-op.
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.switchMultiSimConfig(mTelephonyManager.getActiveModemCount()),
                    SecurityException.class, "android.permission.MODIFY_PHONE_STATE");
        } catch (SecurityException e) {
            fail("TelephonyManager#switchMultiSimConfig should require MODIFY_PHONE_STATE"
                    + "permission to access.");
        }
    }

    @Test
    public void testIccOpenLogicalChannelBySlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // just verify no crash
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.iccOpenLogicalChannelBySlot(0, null, 0));
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException is okay, just not SecurityException
        }
    }

    @Test
    public void testIccCloseLogicalChannelBySlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // just verify no crash
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.iccCloseLogicalChannelBySlot(0, 0));
        } catch (IllegalArgumentException e) {
            // IllegalArgumentException is okay, just not SecurityException
        }
    }

    @Test
    public void testIccTransmitApduLogicalChannelBySlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        int slotIndex = getValidSlotIndex();
        String result = ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> tm.iccTransmitApduLogicalChannelBySlot(
                        slotIndex,
                        0 /* channel */,
                        0 /* cla */,
                        0 /* instruction */,
                        0 /* p1 */,
                        0 /* p2 */,
                        0 /* p3 */,
                        null /* data */));
        assertTrue(TextUtils.isEmpty(result));
    }

    @Test
    public void testIccTransmitApduBasicChannelBySlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // just verify no crash
        int slotIndex = getValidSlotIndex();
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.iccTransmitApduBasicChannelBySlot(
                            slotIndex,
                            0 /* cla */,
                            0 /* instruction */,
                            0 /* p1 */,
                            0 /* p2 */,
                            0 /* p3 */,
                            null /* data */));
        } catch (IllegalArgumentException e ) {
            // IllegalArgumentException is okay, just not SecurityException
        }
    }

    @Test
    public void testIsIccLockEnabled() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // verify SecurityException
        try {
            mTelephonyManager.isIccLockEnabled();
            fail("testIsIccLockEnabled: Expected SecurityException on isIccLockEnabled");
        } catch (SecurityException se) {
            // expected
        }

        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.isIccLockEnabled());
        } catch (SecurityException se) {
            fail("testIsIccLockEnabled: SecurityException not expected");
        }
    }

    @Test
    public void testIsDataEnabledForApn() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // verify SecurityException
        try {
            mTelephonyManager.isDataEnabledForApn(ApnSetting.TYPE_MMS);
            fail("testIsDataEnabledForApn: Expected SecurityException on isDataEnabledForApn");
        } catch (SecurityException se) {
            // expected
        }

        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.isDataEnabledForApn(ApnSetting.TYPE_MMS));
        } catch (SecurityException se) {
            fail("testIsDataEnabledForApn: SecurityException not expected");
        }
    }

    @Test
    public void testIsTetheringApnRequired() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // verify SecurityException
        try {
            mTelephonyManager.isTetheringApnRequired();
            fail("testIsTetheringApnRequired: Expected SecurityException on "
                    + "isTetheringApnRequired");
        } catch (SecurityException se) {
            // expected
        }

        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> tm.isTetheringApnRequired());
        } catch (SecurityException se) {
            fail("testIsIccLockEnabled: SecurityException not expected");
        }
    }

    @Test
    public void testGetCarrierInfoForImsiEncryption() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // test without permission: verify SecurityException
        try {
            mTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_EPDG);
            fail("testGetCarrierInfoForImsiEncryption: "
                    + "SecurityException expected on getCarrierInfoForImsiEncryption");
        } catch (SecurityException se) {
            // expected
        }
        try {
            mTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN);
            fail("testGetCarrierInfoForImsiEncryption: "
                    + "SecurityException expected on getCarrierInfoForImsiEncryption");
        } catch (SecurityException se) {
            // expected
        }
        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_EPDG));
        } catch (SecurityException se) {
            fail("testGetCarrierInfoForImsiEncryption: SecurityException not expected");
        } catch (IllegalArgumentException iae) {
            // IllegalArgumentException is okay, just not SecurityException
        }
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                    (tm) -> tm.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN));
        } catch (SecurityException se) {
            fail("testGetCarrierInfoForImsiEncryption: SecurityException not expected");
        } catch (IllegalArgumentException iae) {
            // IllegalArgumentException is okay, just not SecurityException
        }
    }

    @Test
    public void testResetCarrierKeysForImsiEncryption() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // test without permission: verify SecurityException
        try {
            mTelephonyManager.resetCarrierKeysForImsiEncryption();
            fail("testResetCarrierKeysForImsiEncryption: SecurityException expected");
        } catch (SecurityException se) {
            // expected
        }
        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                    mTelephonyManager,
                    (tm) -> tm.resetCarrierKeysForImsiEncryption());
        } catch (SecurityException se) {
            fail("testResetCarrierKeysForImsiEncryption: SecurityException not expected");
        }
    }

    @Test
    public void testIsInEmergencySmsMode() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        // test without permission: verify SecurityException
        try {
            mTelephonyManager.isInEmergencySmsMode();
            fail("testIsInEmergencySmsMode: SecurityException expected");
        } catch (SecurityException se) {
            // expected
        }
        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                    mTelephonyManager,
                    (tm) -> tm.isInEmergencySmsMode());
        } catch (SecurityException se) {
            fail("testIsInEmergencySmsMode: SecurityException not expected");
        }
    }

    @Test
    public void testGetSubscriptionId() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        TelephonyManager tm = mTelephonyManager.createForSubscriptionId(1);
        int subId = tm.getSubscriptionId();
        assertEquals(1, subId);
    }

    @Test
    public void testSetAllowedNetworkTypes() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        // test without permission: verify SecurityException
        long allowedNetworkTypes = TelephonyManager.NETWORK_TYPE_BITMASK_NR;
        try {
            mTelephonyManager.setAllowedNetworkTypes(allowedNetworkTypes);
            fail("testSetPolicyDataEnabled: SecurityException expected");
        } catch (SecurityException se) {
            // expected
        }

        // test with permission
        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                    mTelephonyManager,
                    (tm) -> tm.setAllowedNetworkTypes(allowedNetworkTypes));

            long deviceAllowedNetworkTypes = ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> {
                        return tm.getAllowedNetworkTypes();
                    }
            );
            assertEquals(allowedNetworkTypes, deviceAllowedNetworkTypes);
        } catch (SecurityException se) {
            fail("testSetAllowedNetworkTypes: SecurityException not expected");
        }
    }

    @Test
    public void testDisAllowedNetworkTypes() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        long allowedNetworkTypes = -1 & (~TelephonyManager.NETWORK_TYPE_BITMASK_NR);
        long networkTypeBitmask = TelephonyManager.NETWORK_TYPE_BITMASK_NR
                | TelephonyManager.NETWORK_TYPE_BITMASK_LTE
                | TelephonyManager.NETWORK_TYPE_BITMASK_LTE_CA;

        try {
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                    mTelephonyManager,
                    (tm) -> tm.setAllowedNetworkTypes(allowedNetworkTypes));

            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(
                    mTelephonyManager,
                    (tm) -> tm.setPreferredNetworkTypeBitmask(networkTypeBitmask));

            long modemNetworkTypeBitmask = ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> {
                        return tm.getPreferredNetworkTypeBitmask();
                    }
            );
            long radioAccessFamily = ShellIdentityUtils.invokeMethodWithShellPermissions(
                    mTelephonyManager, (tm) -> {
                        return tm.getSupportedRadioAccessFamily();
                    }
            );

            // RadioAccessFamily won't include all bits of RAFs group, so transfer to preferred
            // network type instead of using bitmask directly
            int modemPreferredNetworkType = RadioAccessFamily.getNetworkTypeFromRaf(
                    (int) modemNetworkTypeBitmask);
            int preferredNetworkType = RadioAccessFamily.getNetworkTypeFromRaf(
                    (int) (networkTypeBitmask & allowedNetworkTypes & radioAccessFamily));
            assertEquals(preferredNetworkType, modemPreferredNetworkType);
        } catch (SecurityException se) {
            fail("testDisAllowedNetworkTypes: SecurityException not expected");
        }
    }

    @Test
    public void testIsApplicationOnUicc() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        // Expect a security exception without permission.
        try {
            mTelephonyManager.isApplicationOnUicc(TelephonyManager.APPTYPE_SIM);
            fail("Expected security exception");
        } catch (SecurityException se1) {
            // Expected
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.READ_PRIVILEGED_PHONE_STATE");
        try {
            mTelephonyManager.isApplicationOnUicc(TelephonyManager.APPTYPE_SIM);
        } catch (SecurityException se) {
            fail("Caller with READ_PRIVILEGED_PHONE_STATE should be able to call API");
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    @Test
    public void testGetSupportedModemCount() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        int supportedModemCount = mTelephonyManager.getSupportedModemCount();
        int activeModemCount = mTelephonyManager.getActiveModemCount();
        assertTrue(activeModemCount >= 0);
        assertTrue(supportedModemCount >= activeModemCount);
    }

    @Test
    public void testIsModemEnabledForSlot() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        int activeModemCount = mTelephonyManager.getActiveModemCount();
        for (int i = 0; i < activeModemCount; i++) {
            // Call isModemEnabledForSlot for each slot and verify no crash.
            mTelephonyManager.isModemEnabledForSlot(i);
        }
    }

    @Test
    public void testOpportunisticNetworkState() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        boolean isEnabled = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                tm -> tm.isOpportunisticNetworkEnabled());
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                tm -> tm.setOpportunisticNetworkState(true));
        assertTrue(ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                tm -> tm.isOpportunisticNetworkEnabled()));
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                tm -> tm.setOpportunisticNetworkState(false));
        assertFalse(ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                tm -> tm.isOpportunisticNetworkEnabled()));
        ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                tm -> tm.setOpportunisticNetworkState(isEnabled));
    }

    @Test
    public void testGetSimApplicationState() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        int simApplicationState = mTelephonyManager.getSimApplicationState();
        assertTrue(Arrays.asList(TelephonyManager.SIM_STATE_UNKNOWN,
                TelephonyManager.SIM_STATE_PIN_REQUIRED,
                TelephonyManager.SIM_STATE_PUK_REQUIRED,
                TelephonyManager.SIM_STATE_NETWORK_LOCKED,
                TelephonyManager.SIM_STATE_NOT_READY,
                TelephonyManager.SIM_STATE_PERM_DISABLED,
                TelephonyManager.SIM_STATE_LOADED).contains(simApplicationState));
    }

    @Test
    public void testGetSimCardState() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }
        int simCardState = mTelephonyManager.getSimCardState();
        assertTrue(Arrays.asList(TelephonyManager.SIM_STATE_UNKNOWN,
                TelephonyManager.SIM_STATE_ABSENT,
                TelephonyManager.SIM_STATE_CARD_IO_ERROR,
                TelephonyManager.SIM_STATE_CARD_RESTRICTED,
                TelephonyManager.SIM_STATE_PRESENT).contains(simCardState));
    }


    private boolean isDataEnabled() {
        return ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                TelephonyManager::isDataEnabled);
    }

    private Uri getObservableDataEnabledUri(int subId) {
        Uri uri = Settings.Global.getUriFor(Settings.Global.MOBILE_DATA);
        if (mTelephonyManager.getActiveModemCount() != 1) {
            uri = Settings.Global.getUriFor(Settings.Global.MOBILE_DATA + subId);
        }
        return uri;
    }

    /**
     * Validate Emergency Number address that only contains the dialable character.
     *
     * @param address Emergency number address to validate
     * @return {@code true} if the address is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberAddress(String address) {
        if (address == null) {
            return false;
        }
        for (char c : address.toCharArray()) {
            if (!isDialable(c)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Validate Emergency Number country Iso
     *
     * @param countryIso Emergency number country iso to validate
     * @return {@code true} if the country iso is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberCountryIso(String countryIso) {
        if (countryIso == null) {
            return false;
        }
        int length = countryIso.length();
        return length >= 0 && length <= 2;
    }

    /**
     * Validate Emergency Number MNC
     *
     * @param mnc Emergency number MNC to validate
     * @return {@code true} if the MNC is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberMnc(String mnc) {
        if (mnc == null) {
            return false;
        }
        int length = mnc.length();
        return length >= 0 && length <= 3;
    }

    /**
     * Validate Emergency service category list
     *
     * @param categories Emergency service category list to validate
     * @return {@code true} if the category list is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyServiceCategoryList(List<Integer> categories) {
        if (categories == null) {
            return false;
        }
        if (categories.contains(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_UNSPECIFIED)) {
            return categories.size() == 1;
        }
        for (int category : categories) {
            if (!EMERGENCY_SERVICE_CATEGORY_SET.contains(category)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Validate Emergency number source list
     *
     * @param categories Emergency number source list to validate
     * @return {@code true} if the source list is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberSourceList(List<Integer> sources) {
        if (sources == null) {
            return false;
        }
        for (int source : sources) {
            if (!EMERGENCY_NUMBER_SOURCE_SET.contains(source)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Validate Emergency call routing.
     *
     * @param routing Emergency call routing to validate
     * @return {@code true} if the emergency call routing is valid; {@code false} otherwise.
     */
    private static boolean validateEmergencyCallRouting(int routing) {
        return routing >= EmergencyNumber.EMERGENCY_CALL_ROUTING_UNKNOWN
                && routing <= (EmergencyNumber.EMERGENCY_CALL_ROUTING_EMERGENCY
                | EmergencyNumber.EMERGENCY_CALL_ROUTING_NORMAL);
    }

    /**
     * Valid the emergency number should be at least from a valid source.
     *
     * @param emergencyNumber Emergency number to verify
     * @return {@code true} if the emergency number is from any source; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberFromAnySource(EmergencyNumber emergencyNumber) {
        boolean isFromAnySource = false;
        for (int possibleSourceValue = EMERGENCY_NUMBER_SOURCE_RIL_ECCLIST;
                possibleSourceValue <= (EmergencyNumber.EMERGENCY_NUMBER_SOURCE_NETWORK_SIGNALING
                        | EmergencyNumber.EMERGENCY_NUMBER_SOURCE_SIM
                        | EmergencyNumber.EMERGENCY_NUMBER_SOURCE_DATABASE
                        | EmergencyNumber.EMERGENCY_NUMBER_SOURCE_MODEM_CONFIG
                        | EmergencyNumber.EMERGENCY_NUMBER_SOURCE_DEFAULT);
                possibleSourceValue++) {
            if (emergencyNumber.isFromSources(possibleSourceValue)) {
                isFromAnySource = true;
                break;
            }
        }
        return isFromAnySource;
    }

    /**
     * Valid the emergency number should be at least in a valid category.
     *
     * @param emergencyNumber Emergency number to verify
     * @return {@code true} if it is in any category; {@code false} otherwise.
     */
    private static boolean validateEmergencyNumberInAnyCategory(EmergencyNumber emergencyNumber) {
        boolean isInAnyCategory = false;
        for (int possibleCategoryValue = EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_UNSPECIFIED;
                possibleCategoryValue <= (EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_POLICE
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AMBULANCE
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_FIRE_BRIGADE
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MARINE_GUARD
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MOUNTAIN_RESCUE
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_MIEC
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AIEC);
                possibleCategoryValue++) {
            if (emergencyNumber.isInEmergencyServiceCategories(possibleCategoryValue)) {
                isInAnyCategory = true;
                break;
            }
        }
        return isInAnyCategory;
    }

    private static boolean validateEmergencyNumberCompareTo(
            List<EmergencyNumber> emergencyNumberList) {
        if (emergencyNumberList == null) {
            return false;
        }
        if (emergencyNumberList.size() > 0) {
            EmergencyNumber emergencyNumber = emergencyNumberList.get(0);
            if (emergencyNumber.compareTo(emergencyNumber) != 0) {
                return false;
            }
        }
        return true;
    }

    private static boolean isDialable(char c) {
        return (c >= '0' && c <= '9') || c == '*' || c == '#' || c == '+' || c == 'N';
    }

    private int getValidSlotIndex() {
        return ShellIdentityUtils.invokeMethodWithShellPermissions(
                mTelephonyManager, (tm) -> {
                    List<UiccCardInfo> cardInfos = mTelephonyManager.getUiccCardsInfo();
                    Set<String> presentCards = Arrays.stream(mTelephonyManager.getUiccSlotsInfo())
                            .filter(UiccSlotInfo::getIsActive)
                            .map(UiccSlotInfo::getCardId)
                            .filter(Objects::nonNull)
                            // hack around getUiccSlotsInfo not stripping trailing F
                            .map(s -> s.endsWith("F") ? s.substring(0, s.length() - 1) : s)
                            .collect(Collectors.toSet());
                    int slotIndex = -1;
                    for (UiccCardInfo cardInfo : cardInfos) {
                        if (presentCards.contains(cardInfo.getIccId())
                                || presentCards.contains(cardInfo.getEid())) {
                            slotIndex = cardInfo.getSlotIndex();
                            break;
                        }
                    }
                    if (slotIndex < 0) {
                        fail("Test must be run with SIM card inserted, presentCards = "
                                + presentCards + "cardinfos = " + cardInfos);
                    }
                    return slotIndex;
                });
    }

    public static void waitForMs(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Log.d(TAG, "InterruptedException while waiting: " + e);
        }
    }

    /**
     * Verify that the phone is supporting the action of setForbiddenPlmn.
     *
     * @return whether to proceed the test
     */
    private boolean supportSetFplmn() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return false;
        }
        return mTelephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_GSM;
    }

    private static int makeRadioVersion(int major, int minor) {
        if (major < 0 || minor < 0) return 0;
        return major * 100 + minor;
    }
}

