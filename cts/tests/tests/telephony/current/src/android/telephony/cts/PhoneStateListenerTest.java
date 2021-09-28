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

import static androidx.test.InstrumentationRegistry.getContext;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.net.ConnectivityManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.telephony.Annotation.RadioPowerState;
import android.telephony.Annotation.SimActivationState;
import android.telephony.BarringInfo;
import android.telephony.CellIdentity;
import android.telephony.CellInfo;
import android.telephony.CellLocation;
import android.telephony.PhoneStateListener;
import android.telephony.PreciseCallState;
import android.telephony.PreciseDataConnectionState;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.SmsManager;
import android.telephony.TelephonyDisplayInfo;
import android.telephony.TelephonyManager;
import android.telephony.emergency.EmergencyNumber;
import android.telephony.ims.ImsReasonInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executor;

public class PhoneStateListenerTest {

    public static final long WAIT_TIME = 1000;

    private static final String TEST_EMERGENCY_NUMBER = "998877665544332211";

    private boolean mOnActiveDataSubscriptionIdChanged;
    private boolean mOnCallForwardingIndicatorChangedCalled;
    private boolean mOnCallStateChangedCalled;
    private boolean mOnCellLocationChangedCalled;
    private boolean mOnUserMobileDataStateChanged;
    private boolean mOnDataActivityCalled;
    private boolean mOnDataConnectionStateChangedCalled;
    private boolean mOnDataConnectionStateChangedWithNetworkTypeCalled;
    private boolean mOnMessageWaitingIndicatorChangedCalled;
    private boolean mOnCellInfoChangedCalled;
    private boolean mOnServiceStateChangedCalled;
    private boolean mOnSignalStrengthChangedCalled;
    private boolean mOnPreciseCallStateChangedCalled;
    private boolean mOnCallDisconnectCauseChangedCalled;
    private boolean mOnImsCallDisconnectCauseChangedCalled;
    private EmergencyNumber mOnOutgoingSmsEmergencyNumberChanged;
    private boolean mOnPreciseDataConnectionStateChanged;
    private boolean mOnRadioPowerStateChangedCalled;
    private boolean mVoiceActivationStateChangedCalled;
    private boolean mSrvccStateChangedCalled;
    private boolean mOnBarringInfoChangedCalled;
    private boolean mSecurityExceptionThrown;
    private boolean mOnRegistrationFailedCalled;
    private boolean mOnTelephonyDisplayInfoChanged;
    @RadioPowerState private int mRadioPowerState;
    @SimActivationState private int mVoiceActivationState;
    private BarringInfo mBarringInfo;
    private PreciseDataConnectionState mPreciseDataConnectionState;
    private PreciseCallState mPreciseCallState;
    private SignalStrength mSignalStrength;
    private TelephonyManager mTelephonyManager;
    private PhoneStateListener mListener;
    private final Object mLock = new Object();
    private static final String TAG = "android.telephony.cts.PhoneStateListenerTest";
    private static ConnectivityManager mCm;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private static final List<Integer> DATA_CONNECTION_STATE = Arrays.asList(
            TelephonyManager.DATA_CONNECTED,
            TelephonyManager.DATA_DISCONNECTED,
            TelephonyManager.DATA_CONNECTING,
            TelephonyManager.DATA_UNKNOWN,
            TelephonyManager.DATA_SUSPENDED
    );
    private static final List<Integer> PRECISE_CALL_STATE = Arrays.asList(
            PreciseCallState.PRECISE_CALL_STATE_ACTIVE,
            PreciseCallState.PRECISE_CALL_STATE_ALERTING,
            PreciseCallState.PRECISE_CALL_STATE_DIALING,
            PreciseCallState.PRECISE_CALL_STATE_DISCONNECTED,
            PreciseCallState.PRECISE_CALL_STATE_DISCONNECTING,
            PreciseCallState.PRECISE_CALL_STATE_HOLDING,
            PreciseCallState.PRECISE_CALL_STATE_IDLE,
            PreciseCallState.PRECISE_CALL_STATE_INCOMING,
            PreciseCallState.PRECISE_CALL_STATE_NOT_VALID,
            PreciseCallState.PRECISE_CALL_STATE_WAITING
    );

    private Executor mSimpleExecutor = new Executor() {
        @Override
        public void execute(Runnable r) {
            r.run();
        }
    };

    @Before
    public void setUp() throws Exception {
        mTelephonyManager =
                (TelephonyManager)getContext().getSystemService(Context.TELEPHONY_SERVICE);
        mCm = (ConnectivityManager)getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        mHandlerThread = new HandlerThread("PhoneStateListenerTest");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @After
    public void tearDown() throws Exception {
        if (mListener != null) {
            // unregister the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        }
        if (mHandlerThread != null) {
            mHandlerThread.quitSafely();
        }
    }

    @Test
    public void testPhoneStateListener() {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        Looper.prepare();
        new PhoneStateListener();
    }

    @Test
    public void testOnServiceStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertFalse(mOnServiceStateChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onServiceStateChanged(ServiceState serviceState) {
                    synchronized (mLock) {
                        mOnServiceStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
        });
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnServiceStateChangedCalled);
    }

    @Test
    public void testOnUnRegisterFollowedByRegister() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertFalse(mOnServiceStateChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onServiceStateChanged(ServiceState serviceState) {
                    synchronized (mLock) {
                        mOnServiceStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
        });
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnServiceStateChangedCalled);

        // reset and un-register
        mOnServiceStateChangedCalled = false;
        if (mListener != null) {
            // un-register the listener
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_NONE);
        }
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        assertFalse(mOnServiceStateChangedCalled);

        // re-register the listener
        mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SERVICE_STATE);
        synchronized (mLock) {
            if (!mOnServiceStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnServiceStateChangedCalled);
    }

    @Test
    public void testOnSignalStrengthChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertFalse(mOnSignalStrengthChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onSignalStrengthChanged(int asu) {
                    synchronized (mLock) {
                        mOnSignalStrengthChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SIGNAL_STRENGTH);
        });
        synchronized (mLock) {
            if (!mOnSignalStrengthChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnSignalStrengthChangedCalled);
    }

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
    @Test
    public void testOnAlwaysReportedSignalStrengthChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertTrue(mSignalStrength == null);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onSignalStrengthsChanged(SignalStrength signalStrength) {
                    synchronized (mLock) {
                        mSignalStrength = signalStrength;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                (tm) -> tm.listen(mListener,
                    PhoneStateListener.LISTEN_ALWAYS_REPORTED_SIGNAL_STRENGTH));
        });
        synchronized (mLock) {
            if (mSignalStrength == null) {
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mSignalStrength != null);
        // Call SignalStrength methods to make sure they do not throw any exceptions
        mSignalStrength.getCdmaDbm();
        mSignalStrength.getCdmaEcio();
        mSignalStrength.getEvdoDbm();
        mSignalStrength.getEvdoEcio();
        mSignalStrength.getEvdoSnr();
        mSignalStrength.getGsmBitErrorRate();
        mSignalStrength.getGsmSignalStrength();
        mSignalStrength.isGsm();
        mSignalStrength.getLevel();
    }
    */

    /**
     * Due to the corresponding API is hidden in R and will be public in S, this test
     * is commented and will be un-commented in Android S.
     *
     * Validate that SecurityException should be thrown when listen
     * with LISTEN_ALWAYS_REPORTED_SIGNAL_STRENGTH without LISTEN_ALWAYS_REPORTED_SIGNAL_STRENGTH
     * permission.
     *
    @Test
    public void testOnAlwaysReportedSignalStrengthChangedWithoutPermission() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertTrue(mSignalStrength == null);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onSignalStrengthsChanged(SignalStrength signalStrength) {
                    synchronized (mLock) {
                        mSignalStrength = signalStrength;
                        mLock.notify();
                    }
                }
            };
            try {
                mTelephonyManager.listen(mListener,
                    PhoneStateListener.LISTEN_ALWAYS_REPORTED_SIGNAL_STRENGTH);
            } catch (SecurityException se) {
                synchronized (mLock) {
                    mSecurityExceptionThrown = true;
                    mLock.notify();
                }
            }
        });
        synchronized (mLock) {
            if (!mSecurityExceptionThrown) {
                mLock.wait(WAIT_TIME);
            }
        }

        assertThat(mSecurityExceptionThrown).isTrue();
        assertTrue(mSignalStrength == null);
    }
    */

    @Test
    public void testOnSignalStrengthsChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertTrue(mSignalStrength == null);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onSignalStrengthsChanged(SignalStrength signalStrength) {
                    synchronized (mLock) {
                        mSignalStrength = signalStrength;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
        });
        synchronized (mLock) {
            if (mSignalStrength == null) {
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mSignalStrength != null);
        // Call SignalStrength methods to make sure they do not throw any exceptions
        mSignalStrength.getCdmaDbm();
        mSignalStrength.getCdmaEcio();
        mSignalStrength.getEvdoDbm();
        mSignalStrength.getEvdoEcio();
        mSignalStrength.getEvdoSnr();
        mSignalStrength.getGsmBitErrorRate();
        mSignalStrength.getGsmSignalStrength();
        mSignalStrength.isGsm();
        mSignalStrength.getLevel();
    }

    @Test
    public void testOnMessageWaitingIndicatorChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnMessageWaitingIndicatorChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onMessageWaitingIndicatorChanged(boolean mwi) {
                    synchronized (mLock) {
                        mOnMessageWaitingIndicatorChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(
                    mListener, PhoneStateListener.LISTEN_MESSAGE_WAITING_INDICATOR);
        });
        synchronized (mLock) {
            if (!mOnMessageWaitingIndicatorChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnMessageWaitingIndicatorChangedCalled);
    }

    @Test
    public void testOnPreciseCallStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnPreciseCallStateChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onPreciseCallStateChanged(PreciseCallState preciseCallState) {
                    synchronized (mLock) {
                        mOnPreciseCallStateChangedCalled = true;
                        mPreciseCallState = preciseCallState;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener, PhoneStateListener.LISTEN_PRECISE_CALL_STATE));
        });
        synchronized (mLock) {
            if (!mOnPreciseCallStateChangedCalled) {
                mLock.wait(WAIT_TIME);
            }
        }
        Log.d(TAG, "testOnPreciseCallStateChanged: " + mOnPreciseCallStateChangedCalled);

        assertThat(mOnPreciseCallStateChangedCalled).isTrue();
        assertThat(mPreciseCallState.getForegroundCallState()).isIn(PRECISE_CALL_STATE);
        assertThat(mPreciseCallState.getBackgroundCallState()).isIn(PRECISE_CALL_STATE);
        assertThat(mPreciseCallState.getRingingCallState()).isIn(PRECISE_CALL_STATE);
    }

    @Test
    public void testOnCallDisconnectCauseChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnCallDisconnectCauseChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onCallDisconnectCauseChanged(int disconnectCause,
                        int preciseDisconnectCause) {
                    synchronized (mLock) {
                        mOnCallDisconnectCauseChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_CALL_DISCONNECT_CAUSES));
        });
        synchronized (mLock) {
            if (!mOnCallDisconnectCauseChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertThat(mOnCallDisconnectCauseChangedCalled).isTrue();
    }

    @Test
    public void testOnImsCallDisconnectCauseChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnImsCallDisconnectCauseChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onImsCallDisconnectCauseChanged(ImsReasonInfo imsReason) {
                    synchronized (mLock) {
                        mOnImsCallDisconnectCauseChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_IMS_CALL_DISCONNECT_CAUSES));
        });
        synchronized (mLock) {
            if (!mOnImsCallDisconnectCauseChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertThat(mOnImsCallDisconnectCauseChangedCalled).isTrue();
    }

    @Test
    public void testOnPhoneStateListenerExecutorWithSrvccChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mSrvccStateChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener(mSimpleExecutor) {
                @Override
                public void onSrvccStateChanged(int state) {
                    synchronized (mLock) {
                        mSrvccStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_SRVCC_STATE_CHANGED));
        });
        synchronized (mLock) {
            if (!mSrvccStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        Log.d(TAG, "testOnPhoneStateListenerExecutorWithSrvccChanged");

        assertThat(mSrvccStateChangedCalled).isTrue();
    }

    @Test
    public void testOnRadioPowerStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnRadioPowerStateChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onRadioPowerStateChanged(int state) {
                    synchronized (mLock) {
                        mRadioPowerState = state;
                        mOnRadioPowerStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_RADIO_POWER_STATE_CHANGED));
        });
        synchronized (mLock) {
            if (!mOnRadioPowerStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        Log.d(TAG, "testOnRadioPowerStateChanged: " + mRadioPowerState);

        assertThat(mTelephonyManager.getRadioPowerState()).isEqualTo(mRadioPowerState);
    }

    @Test
    public void testOnVoiceActivationStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mVoiceActivationStateChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onVoiceActivationStateChanged(int state) {
                    synchronized (mLock) {
                        mVoiceActivationState = state;
                        mVoiceActivationStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_VOICE_ACTIVATION_STATE));
        });
        synchronized (mLock) {
            if (!mVoiceActivationStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }
        Log.d(TAG, "onVoiceActivationStateChanged: " + mVoiceActivationState);
        int state = ShellIdentityUtils.invokeMethodWithShellPermissions(mTelephonyManager,
                (tm) -> tm.getVoiceActivationState());

        assertEquals(state, mVoiceActivationState);
    }

    @Test
    public void testOnPreciseDataConnectionStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnCallDisconnectCauseChangedCalled).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onPreciseDataConnectionStateChanged(
                        PreciseDataConnectionState state) {
                    synchronized (mLock) {
                        mOnPreciseDataConnectionStateChanged = true;
                        mPreciseDataConnectionState = state;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_PRECISE_DATA_CONNECTION_STATE));
        });
        synchronized (mLock) {
            if (!mOnPreciseDataConnectionStateChanged){
                mLock.wait(WAIT_TIME);
            }
        }

        assertThat(mOnPreciseDataConnectionStateChanged).isTrue();
        assertThat(mPreciseDataConnectionState.getState())
                .isIn(DATA_CONNECTION_STATE);

        // Ensure that no exceptions are thrown
        mPreciseDataConnectionState.getNetworkType();
        mPreciseDataConnectionState.getLinkProperties();
        mPreciseDataConnectionState.getLastCauseCode();
        mPreciseDataConnectionState.getLinkProperties();
        mPreciseDataConnectionState.getApnSetting();

        // Deprecated in R
        assertEquals(mPreciseDataConnectionState.getDataConnectionState(),
                mPreciseDataConnectionState.getState());
        assertEquals(mPreciseDataConnectionState.getDataConnectionNetworkType(),
                mPreciseDataConnectionState.getNetworkType());
        assertEquals(mPreciseDataConnectionState.getDataConnectionFailCause(),
                mPreciseDataConnectionState.getLastCauseCode());
        assertEquals(mPreciseDataConnectionState.getDataConnectionLinkProperties(),
                mPreciseDataConnectionState.getLinkProperties());

        // Superseded in R by getApnSetting()
        mPreciseDataConnectionState.getDataConnectionApnTypeBitMask();
        mPreciseDataConnectionState.getDataConnectionApn();
    }

    @Test
    public void testOnDisplayInfoChanged() throws Exception {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertThat(mOnTelephonyDisplayInfoChanged).isFalse();

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onDisplayInfoChanged(TelephonyDisplayInfo displayInfo) {
                    synchronized (mLock) {
                        mOnTelephonyDisplayInfoChanged = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_DISPLAY_INFO_CHANGED));
        });

        synchronized (mLock) {
            if (!mOnTelephonyDisplayInfoChanged) {
                mLock.wait(WAIT_TIME);
            }
        }
        assertTrue(mOnTelephonyDisplayInfoChanged);
    }

    @Test
    public void testOnCallForwardingIndicatorChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnCallForwardingIndicatorChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onCallForwardingIndicatorChanged(boolean cfi) {
                    synchronized (mLock) {
                        mOnCallForwardingIndicatorChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(
                    mListener, PhoneStateListener.LISTEN_CALL_FORWARDING_INDICATOR);
        });
        synchronized (mLock) {
            if (!mOnCallForwardingIndicatorChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnCallForwardingIndicatorChangedCalled);
    }

    @Test
    public void testOnCellLocationChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnCellLocationChangedCalled);

        TelephonyManagerTest.grantLocationPermissions();
        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onCellLocationChanged(CellLocation location) {
                    synchronized (mLock) {
                        mOnCellLocationChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_CELL_LOCATION);
        });
        synchronized (mLock) {
            if (!mOnCellLocationChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnCellLocationChangedCalled);
    }

    @Test
    public void testOnCallStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnCallStateChangedCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onCallStateChanged(int state, String incomingNumber) {
                    synchronized (mLock) {
                        mOnCallStateChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_CALL_STATE);
        });
        synchronized (mLock) {
            if (!mOnCallStateChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnCallStateChangedCalled);
    }

    @Test
    public void testOnDataConnectionStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnDataConnectionStateChangedCalled);
        assertFalse(mOnDataConnectionStateChangedWithNetworkTypeCalled);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onDataConnectionStateChanged(int state) {
                    synchronized (mLock) {
                        mOnDataConnectionStateChangedCalled = true;
                        if (mOnDataConnectionStateChangedCalled
                                && mOnDataConnectionStateChangedWithNetworkTypeCalled) {
                            mLock.notify();
                        }
                    }
                }
                @Override
                public void onDataConnectionStateChanged(int state, int networkType) {
                    synchronized (mLock) {
                        mOnDataConnectionStateChangedWithNetworkTypeCalled = true;
                        if (mOnDataConnectionStateChangedCalled
                                && mOnDataConnectionStateChangedWithNetworkTypeCalled) {
                            mLock.notify();
                        }
                    }
                }
            };
            mTelephonyManager.listen(
                    mListener, PhoneStateListener.LISTEN_DATA_CONNECTION_STATE);
        });
        synchronized (mLock) {
            if (!mOnDataConnectionStateChangedCalled ||
                    !mOnDataConnectionStateChangedWithNetworkTypeCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnDataConnectionStateChangedCalled);
        assertTrue(mOnDataConnectionStateChangedWithNetworkTypeCalled);
    }

    @Test
    public void testOnDataActivity() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnDataActivityCalled);

        mHandler.post(() -> {
            mListener =  new PhoneStateListener() {
                @Override
                public void onDataActivity(int direction) {
                    synchronized (mLock) {
                        mOnDataActivityCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_DATA_ACTIVITY);
        });
        synchronized (mLock) {
            if (!mOnDataActivityCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnDataActivityCalled);
    }

    @Test
    public void testOnCellInfoChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnDataActivityCalled);

        TelephonyManagerTest.grantLocationPermissions();
        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onCellInfoChanged(List<CellInfo> cellInfo) {
                    synchronized (mLock) {
                        mOnCellInfoChangedCalled = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(mListener, PhoneStateListener.LISTEN_CELL_INFO);
        });
        synchronized (mLock) {
            if (!mOnCellInfoChangedCalled){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnCellInfoChangedCalled);
    }

    @Test
    public void testOnUserMobileDataStateChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnUserMobileDataStateChanged);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onUserMobileDataStateChanged(boolean state) {
                    synchronized (mLock) {
                        mOnUserMobileDataStateChanged = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(
                    mListener, PhoneStateListener.LISTEN_USER_MOBILE_DATA_STATE);
        });
        synchronized (mLock) {
            if (!mOnUserMobileDataStateChanged){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnUserMobileDataStateChanged);
    }

    @Test
    public void testOnOutgoingSmsEmergencyNumberChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        TelephonyUtils.addTestEmergencyNumber(
                InstrumentationRegistry.getInstrumentation(), TEST_EMERGENCY_NUMBER);

        assertNull(mOnOutgoingSmsEmergencyNumberChanged);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onOutgoingEmergencySms(EmergencyNumber emergencyNumber) {
                    synchronized (mLock) {
                        Log.i(TAG, "onOutgoingEmergencySms: emergencyNumber=" + emergencyNumber);
                        mOnOutgoingSmsEmergencyNumberChanged = emergencyNumber;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_OUTGOING_EMERGENCY_SMS));
            SmsManager.getDefault().sendTextMessage(
                TEST_EMERGENCY_NUMBER, null, "testOutgoingSmsListenerCts", null, null);
        });

        try {
            synchronized (mLock) {
                if (mOnOutgoingSmsEmergencyNumberChanged == null) {
                    mLock.wait(WAIT_TIME);
                }
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Operation interrupted.");
        } finally {
            TelephonyUtils.removeTestEmergencyNumber(
                    InstrumentationRegistry.getInstrumentation(), TEST_EMERGENCY_NUMBER);
        }

        assertNotNull(mOnOutgoingSmsEmergencyNumberChanged);
        assertEquals(mOnOutgoingSmsEmergencyNumberChanged.getNumber(), TEST_EMERGENCY_NUMBER);
    }

    @Test
    public void testOnActiveDataSubscriptionIdChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }
        assertFalse(mOnActiveDataSubscriptionIdChanged);

        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onActiveDataSubscriptionIdChanged(int subId) {
                    synchronized (mLock) {
                        mOnActiveDataSubscriptionIdChanged = true;
                        mLock.notify();
                    }
                }
            };
            mTelephonyManager.listen(
                    mListener, PhoneStateListener.LISTEN_ACTIVE_DATA_SUBSCRIPTION_ID_CHANGE);
        });
        synchronized (mLock) {
            if (!mOnActiveDataSubscriptionIdChanged){
                mLock.wait(WAIT_TIME);
            }
        }

        assertTrue(mOnActiveDataSubscriptionIdChanged);
    }

    @Test
    public void testOnBarringInfoChanged() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertFalse(mOnBarringInfoChangedCalled);
        mHandler.post(() -> {
            mListener = new PhoneStateListener() {
                @Override
                public void onBarringInfoChanged(BarringInfo barringInfo) {
                    synchronized (mLock) {
                        mOnBarringInfoChangedCalled = true;
                        mBarringInfo = barringInfo;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener, PhoneStateListener.LISTEN_BARRING_INFO));
        });

        synchronized (mLock) {
            if (!mOnBarringInfoChangedCalled) {
                mLock.wait(WAIT_TIME);
            }
        }
        assertTrue(mOnBarringInfoChangedCalled);

        assertBarringInfoSane(mBarringInfo);
    }

    private static final int[] sBarringServiceInfoTypes = new int[] {
            BarringInfo.BARRING_SERVICE_TYPE_CS_SERVICE,
            BarringInfo.BARRING_SERVICE_TYPE_PS_SERVICE,
            BarringInfo.BARRING_SERVICE_TYPE_CS_VOICE,
            BarringInfo.BARRING_SERVICE_TYPE_MO_SIGNALLING,
            BarringInfo.BARRING_SERVICE_TYPE_MO_DATA,
            BarringInfo.BARRING_SERVICE_TYPE_CS_FALLBACK,
            BarringInfo.BARRING_SERVICE_TYPE_MMTEL_VOICE,
            BarringInfo.BARRING_SERVICE_TYPE_MMTEL_VIDEO,
            BarringInfo.BARRING_SERVICE_TYPE_EMERGENCY,
            BarringInfo.BARRING_SERVICE_TYPE_SMS
    };

    private static void assertBarringInfoSane(BarringInfo barringInfo) {
        assertNotNull(barringInfo);

        // Flags to track whether we have had unknown and known barring types reported
        boolean hasBarringTypeUnknown = false;
        boolean hasBarringTypeKnown = false;

        for (int bsiType : sBarringServiceInfoTypes) {
            BarringInfo.BarringServiceInfo bsi = barringInfo.getBarringServiceInfo(bsiType);
            assertNotNull(bsi);
            switch (bsi.getBarringType()) {
                case BarringInfo.BarringServiceInfo.BARRING_TYPE_UNKNOWN:
                    hasBarringTypeUnknown = true;
                    assertFalse(bsi.isConditionallyBarred());
                    assertEquals(0, bsi.getConditionalBarringFactor());
                    assertEquals(0, bsi.getConditionalBarringTimeSeconds());
                    assertFalse(bsi.isBarred());
                    break;

                case BarringInfo.BarringServiceInfo.BARRING_TYPE_NONE:
                    hasBarringTypeKnown = true;
                    // Unless conditional barring is active, all conditional barring fields
                    // should be "unset".
                    assertFalse(bsi.isConditionallyBarred());
                    assertEquals(0, bsi.getConditionalBarringFactor());
                    assertEquals(0, bsi.getConditionalBarringTimeSeconds());
                    assertFalse(bsi.isBarred());
                    break;

                case BarringInfo.BarringServiceInfo.BARRING_TYPE_UNCONDITIONAL:
                    hasBarringTypeKnown = true;
                    // Unless conditional barring is active, all conditional barring fields
                    // should be "unset".
                    assertFalse(bsi.isConditionallyBarred());
                    assertEquals(0, bsi.getConditionalBarringFactor());
                    assertEquals(0, bsi.getConditionalBarringTimeSeconds());
                    assertTrue(bsi.isBarred());
                    break;

                case BarringInfo.BarringServiceInfo.BARRING_TYPE_CONDITIONAL:
                    hasBarringTypeKnown = true;
                    // If conditional barring is active, then the barring time and factor must
                    // be known (set), but the device may or may not be barred at the moment,
                    // so isConditionallyBarred() can be either true or false (hence not checked).
                    assertNotEquals(0, bsi.getConditionalBarringFactor());
                    assertNotEquals(0, bsi.getConditionalBarringTimeSeconds());
                    assertEquals(bsi.isBarred(), bsi.isConditionallyBarred());
                    break;
            }
        }
        // If any barring type is unknown, then barring is not supported so all must be
        // unknown. If any type is known, then all that are not reported are assumed to
        // be not barred.
        assertNotEquals(hasBarringTypeUnknown, hasBarringTypeKnown);
    }

    @Test
    public void testOnRegistrationFailed() throws Throwable {
        if (mCm.getNetworkInfo(ConnectivityManager.TYPE_MOBILE) == null) {
            Log.d(TAG, "Skipping test that requires ConnectivityManager.TYPE_MOBILE");
            return;
        }

        assertFalse(mOnBarringInfoChangedCalled);
        mHandler.post(() -> {
            mListener =  new PhoneStateListener() {
                @Override
                public void onRegistrationFailed(CellIdentity cid, String chosenPlmn,
                        int domain, int causeCode, int additionalCauseCode) {
                    synchronized (mLock) {
                        mOnRegistrationFailedCalled = true;
                        mLock.notify();
                    }
                }
            };
            ShellIdentityUtils.invokeMethodWithShellPermissionsNoReturn(mTelephonyManager,
                    (tm) -> tm.listen(mListener,
                            PhoneStateListener.LISTEN_REGISTRATION_FAILURE));
        });

        synchronized (mLock) {
            if (!mOnBarringInfoChangedCalled) {
                mLock.wait(WAIT_TIME);
            }
        }

        // Assert that in the WAIT_TIME interval, the listener wasn't invoked. While this is
        // **technically** a flaky test, in practice this flake should happen approximately never
        // as it would mean that a registered phone is failing to reselect during CTS at this
        // exact moment.
        //
        // What the test is verifying is that there is no "auto" callback for registration
        // failure because unlike other PSL registrants, this one is not called upon registration.
        assertFalse(mOnRegistrationFailedCalled);
    }
}
