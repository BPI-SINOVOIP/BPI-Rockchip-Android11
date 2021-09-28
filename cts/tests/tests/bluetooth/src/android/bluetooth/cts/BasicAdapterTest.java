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

package android.bluetooth.cts;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.test.AndroidTestCase;
import android.util.Log;

import java.io.IOException;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Very basic test, just of the static methods of {@link
 * BluetoothAdapter}.
 */
public class BasicAdapterTest extends AndroidTestCase {
    private static final String TAG = "BasicAdapterTest";
    private static final int SET_NAME_TIMEOUT = 5000; // ms timeout for setting adapter name

    private boolean mHasBluetooth;
    private ReentrantLock mAdapterNameChangedlock;
    private Condition mConditionAdapterNameChanged;
    private boolean mIsAdapterNameChanged;

    public void setUp() throws Exception {
        super.setUp();

        mHasBluetooth = getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_BLUETOOTH);
        mAdapterNameChangedlock = new ReentrantLock();
        mConditionAdapterNameChanged = mAdapterNameChangedlock.newCondition();
        mIsAdapterNameChanged = false;
    }

    public void test_getDefaultAdapter() {
        /*
         * Note: If the target doesn't support Bluetooth at all, then
         * this method should return null.
         */
        if (mHasBluetooth) {
            assertNotNull(BluetoothAdapter.getDefaultAdapter());
        } else {
            assertNull(BluetoothAdapter.getDefaultAdapter());
        }
    }

    public void test_checkBluetoothAddress() {
        // Can't be null.
        assertFalse(BluetoothAdapter.checkBluetoothAddress(null));

        // Must be 17 characters long.
        assertFalse(BluetoothAdapter.checkBluetoothAddress(""));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("0"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:0"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:0"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:0"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:00:"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:00:0"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress("00:00:00:00:00:"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:00:00:00:0"));

        // Must have colons between octets.
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00x00:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00.00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:00-00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:00:00900:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:00:00:00?00"));

        // Hex letters must be uppercase.
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "a0:00:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "0b:00:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:c0:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:0d:00:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:e0:00:00:00"));
        assertFalse(BluetoothAdapter.checkBluetoothAddress(
            "00:00:0f:00:00:00"));

        assertTrue(BluetoothAdapter.checkBluetoothAddress(
            "00:00:00:00:00:00"));
        assertTrue(BluetoothAdapter.checkBluetoothAddress(
            "12:34:56:78:9A:BC"));
        assertTrue(BluetoothAdapter.checkBluetoothAddress(
            "DE:F0:FE:DC:B8:76"));
    }

    /** Checks enable(), disable(), getState(), isEnabled() */
    public void test_enableDisable() {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();

        for (int i=0; i<5; i++) {
            assertTrue(BTAdapterUtils.disableAdapter(adapter, mContext));
            assertTrue(BTAdapterUtils.enableAdapter(adapter, mContext));
        }
    }

    public void test_getAddress() {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        assertTrue(BTAdapterUtils.enableAdapter(adapter, mContext));

        assertTrue(BluetoothAdapter.checkBluetoothAddress(adapter.getAddress()));
    }

    public void test_setName_getName() {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        assertTrue(BTAdapterUtils.enableAdapter(adapter, mContext));

        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothAdapter.ACTION_LOCAL_NAME_CHANGED);
        filter.setPriority(IntentFilter.SYSTEM_HIGH_PRIORITY);
        mContext.registerReceiver(mAdapterNameChangeReceiver, filter);

        String name = adapter.getName();
        assertNotNull(name);

        // Check renaming the adapter
        String genericName = "Generic Device 1";
        mIsAdapterNameChanged = false;
        assertTrue(adapter.setName(genericName));
        assertTrue(waitForAdapterNameChange());
        mIsAdapterNameChanged = false;
        assertEquals(genericName, adapter.getName());

        // Check setting adapter back to original name
        assertTrue(adapter.setName(name));
        assertTrue(waitForAdapterNameChange());
        mIsAdapterNameChanged = false;
        assertEquals(name, adapter.getName());
    }

    public void test_getBondedDevices() {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        assertTrue(BTAdapterUtils.enableAdapter(adapter, mContext));

        Set<BluetoothDevice> devices = adapter.getBondedDevices();
        assertNotNull(devices);
        for (BluetoothDevice device : devices) {
            assertTrue(BluetoothAdapter.checkBluetoothAddress(device.getAddress()));
        }
    }

    public void test_getRemoteDevice() {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        // getRemoteDevice() should work even with Bluetooth disabled
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        assertTrue(BTAdapterUtils.disableAdapter(adapter, mContext));

        // test bad addresses
        try {
            adapter.getRemoteDevice((String)null);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {}
        try {
            adapter.getRemoteDevice("00:00:00:00:00:00:00:00");
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {}
        try {
            adapter.getRemoteDevice((byte[])null);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {}
        try {
            adapter.getRemoteDevice(new byte[] {0x00, 0x00, 0x00, 0x00, 0x00});
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {}

        // test success
        BluetoothDevice device = adapter.getRemoteDevice("00:11:22:AA:BB:CC");
        assertNotNull(device);
        assertEquals("00:11:22:AA:BB:CC", device.getAddress());
        device = adapter.getRemoteDevice(
                new byte[] {0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
        assertNotNull(device);
        assertEquals("01:02:03:04:05:06", device.getAddress());
    }

    public void test_listenUsingRfcommWithServiceRecord() throws IOException {
        if (!mHasBluetooth) {
            // Skip the test if bluetooth is not present.
            return;
        }
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        assertTrue(BTAdapterUtils.enableAdapter(adapter, mContext));

        BluetoothServerSocket socket = adapter.listenUsingRfcommWithServiceRecord(
                "test", UUID.randomUUID());
        assertNotNull(socket);
        socket.close();
    }

    private static void sleep(long t) {
        try {
            Thread.sleep(t);
        } catch (InterruptedException e) {}
    }

    private boolean waitForAdapterNameChange() {
        mAdapterNameChangedlock.lock();
        try {
            // Wait for the Adapter name to be changed
            while (!mIsAdapterNameChanged) {
                if (!mConditionAdapterNameChanged.await(
                        SET_NAME_TIMEOUT, TimeUnit.MILLISECONDS)) {
                    Log.e(TAG, "Timeout while waiting for adapter name change");
                    break;
                }
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "waitForAdapterNameChange: interrrupted");
        } finally {
            mAdapterNameChangedlock.unlock();
        }
        return mIsAdapterNameChanged;
    }

    private final BroadcastReceiver mAdapterNameChangeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(BluetoothAdapter.ACTION_LOCAL_NAME_CHANGED)) {
                mAdapterNameChangedlock.lock();
                mIsAdapterNameChanged = true;
                try {
                    mConditionAdapterNameChanged.signal();
                } catch (IllegalMonitorStateException ex) {
                } finally {
                    mAdapterNameChangedlock.unlock();
                }
            }
        }
    };
}
