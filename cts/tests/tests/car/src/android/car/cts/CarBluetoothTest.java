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

package android.car.cts;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.util.SparseArray;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.RequiredFeatureRule;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Contains the tests to prove compliance with android automotive specific bluetooth requirements.
 */
// TODO(b/146663105): Fix hidden API
//@SmallTest
//@RequiresDevice
//@RunWith(AndroidJUnit4.class)
public class CarBluetoothTest {
    @ClassRule
    public static final RequiredFeatureRule sRequiredFeatureRule = new RequiredFeatureRule(
            PackageManager.FEATURE_AUTOMOTIVE);

    private static final String TAG = "CarBluetoothTest";
    private static final boolean DBG = false;
    private Context mContext;

    // Bluetooth Core objects
    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;

    // Timeout for waiting for an adapter state change
    private static final int BT_ADAPTER_TIMEOUT_MS = 8000; // ms

    // Objects to block until the adapter has reached a desired state
    private ReentrantLock mBluetoothAdapterLock;
    private Condition mConditionAdapterStateReached;
    private int mDesiredState;
    private int mOriginalState;

    /**
     * Handles BluetoothAdapter state changes and signals when we've reached a desired state
     */
    private class BluetoothAdapterReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {

            // Decode the intent
            String action = intent.getAction();

            // Watch for BluetoothAdapter intents only
            if (BluetoothAdapter.ACTION_STATE_CHANGED.equals(action)) {
                int newState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1);
                if (DBG) {
                    Log.d(TAG, "Bluetooth adapter state changed: " + newState);
                }

                // Signal if the state is set to the one we're waiting on. If its not and we got a
                // STATE_OFF event then handle the unexpected off event. Note that we could
                // proactively turn the adapter back on to continue testing. For now we'll just
                // log it
                mBluetoothAdapterLock.lock();
                try {
                    if (mDesiredState == newState) {
                        mConditionAdapterStateReached.signal();
                    } else if (newState == BluetoothAdapter.STATE_OFF) {
                        Log.w(TAG, "Bluetooth turned off unexpectedly while test was running.");
                    }
                } finally {
                    mBluetoothAdapterLock.unlock();
                }
            }
        }
    }
    private BluetoothAdapterReceiver mBluetoothAdapterReceiver;

    private void waitForAdapterOn() {
        if (DBG) {
            Log.d(TAG, "Waiting for adapter to be on...");
        }
        waitForAdapterState(BluetoothAdapter.STATE_ON);
    }

    private void waitForAdapterOff() {
        if (DBG) {
            Log.d(TAG, "Waiting for adapter to be off...");
        }
        waitForAdapterState(BluetoothAdapter.STATE_OFF);
    }

    // Wait for the bluetooth adapter to be in a given state
    private void waitForAdapterState(int desiredState) {
        if (DBG) {
            Log.d(TAG, "Waiting for adapter state " + desiredState);
        }
        mBluetoothAdapterLock.lock();
        try {
            // Update the desired state so that we'll signal when we get there
            mDesiredState = desiredState;
            if (desiredState == BluetoothAdapter.STATE_ON) {
                mBluetoothAdapter.enable();
            } else {
                mBluetoothAdapter.disable();
            }

            // Wait until we're reached that desired state
            while (desiredState != mBluetoothAdapter.getState()) {
                if (!mConditionAdapterStateReached.await(
                    BT_ADAPTER_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                    Log.e(TAG, "Timeout while waiting for Bluetooth adapter state " + desiredState);
                    break;
                }
            }
        } catch (InterruptedException e) {
            Log.w(TAG, "waitForAdapterState(" + desiredState + "): interrupted", e);
        } finally {
            mBluetoothAdapterLock.unlock();
        }
    }

    // Utility class to hold profile information and state
    private static class ProfileInfo {
        final String mName;
        boolean mConnected;

        public ProfileInfo(String name) {
            mName = name;
            mConnected = false;
        }
    }
// TODO(b/146663105): Fix hidden API
/*
    // Automotive required profiles and meta data. Profile defaults to 'not connected' and name
    // is used in debug and error messages
    private static SparseArray<ProfileInfo> sRequiredBluetoothProfiles = new SparseArray();
    static {
        sRequiredBluetoothProfiles.put(BluetoothProfile.A2DP_SINK,
                new ProfileInfo("A2DP Sink")); // 11
        sRequiredBluetoothProfiles.put(BluetoothProfile.AVRCP_CONTROLLER,
                new ProfileInfo("AVRCP Controller")); // 12
        sRequiredBluetoothProfiles.put(BluetoothProfile.HEADSET_CLIENT,
                new ProfileInfo("HSP Client")); // 16
        sRequiredBluetoothProfiles.put(BluetoothProfile.PBAP_CLIENT,
                new ProfileInfo("PBAP Client")); // 17
    }
    private static final int MAX_PROFILES_SUPPORTED = sRequiredBluetoothProfiles.size();

    // Configurable timeout for waiting for profile proxies to connect
    private static final int PROXY_CONNECTIONS_TIMEOUT_MS = 1000; // ms

    // Objects to block until all profile proxy connections have finished, or the timeout occurs
    private Condition mConditionAllProfilesConnected;
    private ReentrantLock mProfileConnectedLock;
    private int mProfilesSupported;

    // Capture profile proxy connection events
    private final class ProfileServiceListener implements BluetoothProfile.ServiceListener {
        @Override
        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            if (DBG) {
                Log.d(TAG, "Profile '" + profile + "' has connected");
            }
            mProfileConnectedLock.lock();
            try {
                sRequiredBluetoothProfiles.get(profile).mConnected = true;
                mProfilesSupported++;
                if (mProfilesSupported == MAX_PROFILES_SUPPORTED) {
                    mConditionAllProfilesConnected.signal();
                }
            } finally {
                mProfileConnectedLock.unlock();
            }
        }

        @Override
        public void onServiceDisconnected(int profile) {
            if (DBG) {
                Log.d(TAG, "Profile '" + profile + "' has disconnected");
            }
            mProfileConnectedLock.lock();
            try {
                sRequiredBluetoothProfiles.get(profile).mConnected = false;
                mProfilesSupported--;
            } finally {
                mProfileConnectedLock.unlock();
            }
        }
    }

    // Initiate connections to all profiles and wait until we connect to all, or time out
    private void waitForProfileConnections() {
        if (DBG) {
            Log.d(TAG, "Starting profile proxy connections...");
        }
        mProfileConnectedLock.lock();
        try {
            // Attempt connection to each required profile
            for (int i = 0; i < sRequiredBluetoothProfiles.size(); i++) {
                int profile = sRequiredBluetoothProfiles.keyAt(i);
                mBluetoothAdapter.getProfileProxy(mContext, new ProfileServiceListener(), profile);
            }

            // Wait for the Adapter to be disabled
            while (mProfilesSupported != MAX_PROFILES_SUPPORTED) {
                if (!mConditionAllProfilesConnected.await(
                    PROXY_CONNECTIONS_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                    Log.e(TAG, "Timeout while waiting for Profile Connections");
                    break;
                }
            }
        } catch (InterruptedException e) {
            Log.w(TAG, "waitForProfileConnections: interrupted", e);
        } finally {
            mProfileConnectedLock.unlock();
        }

        if (DBG) {
            Log.d(TAG, "Proxy connection attempts complete. Connected " + mProfilesSupported
                    + "/" + MAX_PROFILES_SUPPORTED + " profiles");
        }
    }

    // Check and make sure each profile is connected. If any are not supported then build an
    // error string to report each missing profile and assert a failure
    private void checkProfileConnections() {
        if (DBG) {
            Log.d(TAG, "Checking for all required profiles");
        }
        mProfileConnectedLock.lock();
        try {
            if (mProfilesSupported != MAX_PROFILES_SUPPORTED) {
                if (DBG) {
                    Log.d(TAG, "Some profiles failed to connect");
                }
                StringBuilder e = new StringBuilder();
                for (int i = 0; i < sRequiredBluetoothProfiles.size(); i++) {
                    int profile = sRequiredBluetoothProfiles.keyAt(i);
                    String name = sRequiredBluetoothProfiles.get(profile).mName;
                    if (!sRequiredBluetoothProfiles.get(profile).mConnected) {
                        if (e.length() == 0) {
                            e.append("Missing Profiles: ");
                        } else {
                            e.append(", ");
                        }
                        e.append(name + " (" + profile + ")");

                        if (DBG) {
                            Log.d(TAG, name + " failed to connect");
                        }
                    }
                }
                fail(e.toString());
            }
        } finally {
            mProfileConnectedLock.unlock();
        }
    }

    // Set the connection status for each profile to false
    private void clearProfileStatuses() {
        if (DBG) {
            Log.d(TAG, "Setting all profiles to 'disconnected'");
        }
        for (int i = 0; i < sRequiredBluetoothProfiles.size(); i++) {
            int profile = sRequiredBluetoothProfiles.keyAt(i);
            sRequiredBluetoothProfiles.get(profile).mConnected = false;
        }
    }

    @Before
    public void setUp() throws Exception {
        if (DBG) {
            Log.d(TAG, "Setting up Automotive Bluetooth test. Device is "
                    + (FeatureUtil.isAutomotive() ? "" : "not ") + "automotive");
        }

        // Get the context
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // Get bluetooth core objects so we can get proxies/check for profile existence
        mBluetoothManager = (BluetoothManager) mContext.getSystemService(
                Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = mBluetoothManager.getAdapter();

        // Initialize all the profile connection variables
        mProfilesSupported = 0;
        mProfileConnectedLock = new ReentrantLock();
        mConditionAllProfilesConnected = mProfileConnectedLock.newCondition();
        clearProfileStatuses();

        // Register the adapter receiver and initialize adapter state wait objects
        mDesiredState = -1; // Set and checked by waitForAdapterState()
        mBluetoothAdapterLock = new ReentrantLock();
        mConditionAdapterStateReached = mBluetoothAdapterLock.newCondition();
        mBluetoothAdapterReceiver = new BluetoothAdapterReceiver();
        IntentFilter filter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        mContext.registerReceiver(mBluetoothAdapterReceiver, filter);

        // Make sure Bluetooth is enabled before the test
        waitForAdapterOn();
        assertTrue(mBluetoothAdapter.isEnabled());
    }

    @After
    public void tearDown() {
        waitForAdapterOff();
        mContext.unregisterReceiver(mBluetoothAdapterReceiver);
    }

    // [A-0-2] : Android Automotive devices must support the following Bluetooth profiles:
    //  * Hands Free Profile (HFP) [Phone calling]
    //  * Audio Distribution Profile (A2DP) [Media playback]
    //  * Audio/Video Remote Control Profile (AVRCP) [Media playback control]
    //  * Phone Book Access Profile (PBAP) [Contact sharing/receiving]
    //
    // This test fires off connections to each required profile (which are asynchronous in nature)
    // and waits for all of them to connect (proving they are there and implemented), or for the
    // configured timeout. If all required profiles connect, the test passes.
    @Test
    @CddTest(requirement = "7.4.3/A-0-2")
    public void testRequiredBluetoothProfilesExist() throws Exception {
        if (DBG) {
            Log.d(TAG, "Begin testRequiredBluetoothProfilesExist()");
        }
        assertNotNull(mBluetoothAdapter);
        waitForProfileConnections();
        checkProfileConnections();
    }
*/
}
