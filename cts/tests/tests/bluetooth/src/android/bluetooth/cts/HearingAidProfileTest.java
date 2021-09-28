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

package android.bluetooth.cts;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHearingAid;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.Log;

import java.io.IOException;
import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;
import java.util.concurrent.TimeUnit;

/**
 * Unit test cases for {@link BluetoothHearingAid}.
 * <p>
 * To run the test, use adb shell am instrument -e class 'android.bluetooth.HearingAidProfileTest'
 * -w 'com.android.bluetooth.tests/android.bluetooth.BluetoothTestRunner'
 */
public class HearingAidProfileTest extends AndroidTestCase {
    private static final String TAG = "HearingAidProfileTest";

    private static final int WAIT_FOR_INTENT_TIMEOUT_MS = 10000; // ms to wait for intent callback
    private static final int PROXY_CONNECTION_TIMEOUT_MS = 500;  // ms timeout for Proxy Connect
    // ADAPTER_ENABLE_TIMEOUT_MS = AdapterState.BLE_START_TIMEOUT_DELAY +
    //                              AdapterState.BREDR_START_TIMEOUT_DELAY
    private static final int ADAPTER_ENABLE_TIMEOUT_MS = 8000;
    // ADAPTER_DISABLE_TIMEOUT_MS = AdapterState.BLE_STOP_TIMEOUT_DELAY +
    //                                  AdapterState.BREDR_STOP_TIMEOUT_DELAY
    private static final int ADAPTER_DISABLE_TIMEOUT_MS = 5000;

    private boolean mIsHearingAidSupported;
    private boolean mIsBleSupported;
    private BluetoothHearingAid mService;
    private BluetoothAdapter mBluetoothAdapter;
    private BroadcastReceiver mIntentReceiver;

    private Condition mConditionProfileIsConnected;
    private ReentrantLock mProfileConnectedlock;
    private boolean mIsProfileReady;

    private static List<Integer> mValidConnectionStates = new ArrayList<Integer>(
        Arrays.asList(BluetoothProfile.STATE_CONNECTING, BluetoothProfile.STATE_CONNECTED,
                      BluetoothProfile.STATE_DISCONNECTED, BluetoothProfile.STATE_DISCONNECTING));

    private List<BluetoothDevice> mIntentCallbackDeviceList;

    public void setUp() throws Exception {
        if (!isBleSupported()) return;
        mIsBleSupported = true;

        BluetoothManager manager = (BluetoothManager) mContext.getSystemService(
                Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = manager.getAdapter();

        if (!BTAdapterUtils.enableAdapter(mBluetoothAdapter, mContext)) {
            Log.e(TAG, "Unable to enable Bluetooth Adapter!");
            assertTrue(mBluetoothAdapter.isEnabled());
        }

        mProfileConnectedlock = new ReentrantLock();
        mConditionProfileIsConnected  = mProfileConnectedlock.newCondition();
        mIsProfileReady = false;
        mService = null;
        mIsHearingAidSupported = mBluetoothAdapter.getProfileProxy(getContext(),
                                                  new HearingAidsServiceListener(),
                                                  BluetoothProfile.HEARING_AID);
        if (!mIsHearingAidSupported) return;
    }

    @Override
    public void tearDown() {
        if (!mIsBleSupported) return;

        if (!BTAdapterUtils.disableAdapter(mBluetoothAdapter, mContext)) {
            Log.e(TAG, "Unable to disable Bluetooth Adapter!");
            assertTrue(mBluetoothAdapter.isEnabled());
        }
    }

    /**
     * Basic test case to make sure that Hearing Aid Profile Proxy can connect.
     */
    @MediumTest
    public void test_getProxyServiceConnect() {
        if (!(mIsBleSupported && mIsHearingAidSupported)) return;

        waitForProfileConnect();
        assertTrue(mIsProfileReady);
        assertNotNull(mService);
    }

    /**
     * Basic test case to make sure that a fictional device is disconnected.
     */
    @MediumTest
    public void test_getConnectionState() {
        if (!(mIsBleSupported && mIsHearingAidSupported)) {
            return;
        }

        waitForProfileConnect();
        assertTrue(mIsProfileReady);
        assertNotNull(mService);

        // Create a dummy device
        BluetoothDevice device = mBluetoothAdapter.getRemoteDevice("00:11:22:AA:BB:CC");
        assertNotNull(device);

        int connectionState = mService.getConnectionState(device);
        // Dummy device should be disconnected
        assertEquals(connectionState, BluetoothProfile.STATE_DISCONNECTED);
    }

    /**
     * Basic test case to get the list of connected Hearing Aid devices.
     */
    @MediumTest
    public void test_getConnectedDevices() {
        if (!(mIsBleSupported && mIsHearingAidSupported)) {
            return;
        }

        waitForProfileConnect();
        assertTrue(mIsProfileReady);
        assertNotNull(mService);

        List<BluetoothDevice> deviceList;

        deviceList = mService.getConnectedDevices();
        Log.d(TAG, "getConnectedDevices(): size=" + deviceList.size());
        for (BluetoothDevice device : deviceList) {
            int connectionState = mService.getConnectionState(device);
            checkValidConnectionState(connectionState);
        }
    }

    /**
     * Basic test case to get the list of matching Hearing Aid devices for each of the 4 connection
     * states.
     */
    @MediumTest
    public void test_getDevicesMatchingConnectionStates() {
        if (!(mIsBleSupported && mIsHearingAidSupported)) {
            return;
        }

        waitForProfileConnect();
        assertTrue(mIsProfileReady);
        assertNotNull(mService);

        for (int connectionState : mValidConnectionStates) {
            List<BluetoothDevice> deviceList;

            deviceList = mService.getDevicesMatchingConnectionStates(new int[]{connectionState});
            assertNotNull(deviceList);
            Log.d(TAG, "getDevicesMatchingConnectionStates(" + connectionState + "): size="
                  + deviceList.size());
            checkDeviceListAndStates(deviceList, connectionState);
        }
    }

    /**
     * Test case to make sure that if the connection changed intent is called, the parameters and
     * device are correct.
     */
    @MediumTest
    public void test_getConnectionStateChangedIntent() {
        if (!(mIsBleSupported && mIsHearingAidSupported)) {
            return;
        }

        waitForProfileConnect();
        assertTrue(mIsProfileReady);
        assertNotNull(mService);

        // Find out how many Hearing Aid bonded devices
        List<BluetoothDevice> bondedDeviceList = new ArrayList();
        int numDevices = 0;
        for (int connectionState : mValidConnectionStates) {
            List<BluetoothDevice> deviceList;

            deviceList = mService.getDevicesMatchingConnectionStates(new int[]{connectionState});
            bondedDeviceList.addAll(deviceList);
            numDevices += deviceList.size();
        }

        if (numDevices <= 0) return;
        Log.d(TAG, "Number Hearing Aids devices bonded=" + numDevices);

        mIntentCallbackDeviceList = new ArrayList();

        // Set up the Connection State Changed receiver
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothHearingAid.ACTION_CONNECTION_STATE_CHANGED);
        mIntentReceiver = new HearingAidIntentReceiver();
        mContext.registerReceiver(mIntentReceiver, filter);

        Log.d(TAG, "test_getConnectionStateChangedIntent: disable adapter and wait");
        assertTrue(BTAdapterUtils.disableAdapter(mBluetoothAdapter, mContext));

        Log.d(TAG, "test_getConnectionStateChangedIntent: enable adapter and wait");
        assertTrue(BTAdapterUtils.enableAdapter(mBluetoothAdapter, mContext));

        int sanityCount = WAIT_FOR_INTENT_TIMEOUT_MS;
        while ((numDevices != mIntentCallbackDeviceList.size()) && (sanityCount > 0)) {
            final int SLEEP_QUANTUM_MS = 100;
            sleep(SLEEP_QUANTUM_MS);
            sanityCount -= SLEEP_QUANTUM_MS;
        }

        // Tear down
        mContext.unregisterReceiver(mIntentReceiver);

        Log.d(TAG, "test_getConnectionStateChangedIntent: number of bonded device="
              + numDevices + ", mIntentCallbackDeviceList.size()="
              + mIntentCallbackDeviceList.size());
        for (BluetoothDevice device : mIntentCallbackDeviceList) {
            assertTrue(bondedDeviceList.contains(device));
        }
    }

    private boolean waitForProfileConnect() {
        mProfileConnectedlock.lock();
        try {
            // Wait for the Adapter to be disabled
            while (!mIsProfileReady) {
                if (!mConditionProfileIsConnected.await(
                    PROXY_CONNECTION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                    // Timeout
                    Log.e(TAG, "Timeout while waiting for Profile Connect");
                    break;
                } // else spurious wakeups
            }
        } catch(InterruptedException e) {
            Log.e(TAG, "waitForProfileConnect: interrrupted");
        } finally {
            mProfileConnectedlock.unlock();
        }
        return mIsProfileReady;
    }

    private final class HearingAidsServiceListener
            implements BluetoothProfile.ServiceListener {

        public void onServiceConnected(int profile, BluetoothProfile proxy) {
            mProfileConnectedlock.lock();
            mService = (BluetoothHearingAid) proxy;
            mIsProfileReady = true;
            try {
                mConditionProfileIsConnected.signal();
            } finally {
                mProfileConnectedlock.unlock();
            }
        }

        public void onServiceDisconnected(int profile) {
            mProfileConnectedlock.lock();
            mIsProfileReady = false;
            mService = null;
            mProfileConnectedlock.unlock();
        }
    }

    private class HearingAidIntentReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (BluetoothHearingAid.ACTION_CONNECTION_STATE_CHANGED.equals(intent.getAction())) {
                int state = intent.getIntExtra(BluetoothProfile.EXTRA_STATE, -1);
                int previousState = intent.getIntExtra(BluetoothProfile.EXTRA_PREVIOUS_STATE, -1);
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);

                Log.d(TAG,"HearingAidIntentReceiver.onReceive: device=" + device
                      + ", state=" + state + ", previousState=" + previousState);

                checkValidConnectionState(state);
                checkValidConnectionState(previousState);

                mIntentCallbackDeviceList.add(device);
            }
        }
    }

    private void checkDeviceListAndStates(List<BluetoothDevice> deviceList, int connectionState) {
        Log.d(TAG, "checkDeviceListAndStates(): size=" + deviceList.size()
              + ", connectionState=" + connectionState);
        for (BluetoothDevice device : deviceList) {
            int deviceConnectionState = mService.getConnectionState(device);
            assertEquals("Mismatched connection state for " + device,
                         connectionState, deviceConnectionState);
        }
    }

    private void checkValidConnectionState(int connectionState) {
        assertTrue(mValidConnectionStates.contains(connectionState));
    }

    // Returns whether offloaded scan batching is supported.
    private boolean isBleBatchScanSupported() {
        return mBluetoothAdapter.isOffloadedScanBatchingSupported();
    }

    // Check if Bluetooth LE feature is supported on DUT.
    private boolean isBleSupported() {
        return getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE);
    }

    private static void sleep(long t) {
        try {
            Thread.sleep(t);
        } catch (InterruptedException e) {}
    }
}
