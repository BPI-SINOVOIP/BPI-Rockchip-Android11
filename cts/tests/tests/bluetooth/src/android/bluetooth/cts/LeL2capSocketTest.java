/*
 * Copyright 2020 The Android Open Source Project
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
import android.bluetooth.BluetoothServerSocket;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

import java.io.IOException;

public class LeL2capSocketTest extends AndroidTestCase {

    private static final int NUM_ITERATIONS_FOR_REPEATED_TEST = 100;

    private BluetoothAdapter mAdapter = null;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        mAdapter = BluetoothAdapter.getDefaultAdapter();
        assertNotNull("BluetoothAdapter.getDefaultAdapter() returned null. "
                + "Does this device have a Bluetooth adapter?", mAdapter);
        if (!mAdapter.isEnabled()) {
            assertTrue(BTAdapterUtils.enableAdapter(mAdapter, mContext));
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        assertTrue(BTAdapterUtils.disableAdapter(mAdapter, mContext));
        mAdapter = null;
        super.tearDown();
    }


    @SmallTest
    public void testOpenInsecureLeL2capServerSocketOnce() {
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        assertTrue("Bluetooth is not enabled", mAdapter.isEnabled());
        try {
            final BluetoothServerSocket serverSocket = mAdapter.listenUsingInsecureL2capChannel();
            assertNotNull("Failed to get server socket", serverSocket);
            serverSocket.close();
        } catch (IOException exp) {
            fail("IOException while opening and closing server socket: " + exp);
        }
    }

    @SmallTest
    public void testOpenInsecureLeL2capServerSocketRepeatedly() {
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        assertTrue("Bluetooth is not enabled", mAdapter.isEnabled());
        try {
            for (int i = 0; i < NUM_ITERATIONS_FOR_REPEATED_TEST; i++) {
                final BluetoothServerSocket serverSocket =
                        mAdapter.listenUsingInsecureL2capChannel();
                assertNotNull("Failed to get server socket", serverSocket);
                serverSocket.close();
            }
        } catch (IOException exp) {
            fail("IOException while opening and closing server socket: " + exp);
        }
    }

    @SmallTest
    public void testOpenSecureLeL2capServerSocketOnce() {
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        assertTrue("Bluetooth is not enabled", mAdapter.isEnabled());
        try {
            final BluetoothServerSocket serverSocket = mAdapter.listenUsingL2capChannel();
            assertNotNull("Failed to get server socket", serverSocket);
            serverSocket.close();
        } catch (IOException exp) {
            fail("IOException while opening and closing server socket: " + exp);
        }
    }

    @SmallTest
    public void testOpenSecureLeL2capServerSocketRepeatedly() {
        if (!TestUtils.isBleSupported(getContext())) {
            return;
        }
        assertTrue("Bluetooth is not enabled", mAdapter.isEnabled());
        try {
            for (int i = 0; i < NUM_ITERATIONS_FOR_REPEATED_TEST; i++) {
                final BluetoothServerSocket serverSocket = mAdapter.listenUsingL2capChannel();
                assertNotNull("Failed to get server socket", serverSocket);
                serverSocket.close();
            }
        } catch (IOException exp) {
            fail("IOException while opening and closing server socket: " + exp);
        }
    }
}
