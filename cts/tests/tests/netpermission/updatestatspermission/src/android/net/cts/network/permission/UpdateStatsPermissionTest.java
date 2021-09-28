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

package android.net.cts.networkpermission.updatestatspermission;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.TrafficStats;
import android.os.Process;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.OutputStream;
import java.net.Socket;

/**
* Test that protected android.net.ConnectivityManager methods cannot be called without
* permissions
*/
@RunWith(AndroidJUnit4.class)
public class UpdateStatsPermissionTest {

    /**
     * Verify that setCounterSet for a different uid failed because of the permission cannot be
     * granted to a third-party app.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#UPDATE_DEVICE_STATS}.
     */
    @SmallTest
    @Test
    public void testUpdateDeviceStatsPermission() throws Exception {

        // Set the current thread uid to a another uid. It should silently fail when tagging the
        // socket since the current process doesn't have UPDATE_DEVICE_STATS permission.
        TrafficStats.setThreadStatsTag(0);
        TrafficStats.setThreadStatsUid(/*root uid*/ 0);
        Socket socket = new Socket("example.com", 80);
        TrafficStats.tagSocket(socket);

        // Transfer 1K of data to a remote host and verify the stats is still billed to the current
        // uid.
        final int byteCount = 1024;

        socket.setTcpNoDelay(true);
        socket.setSoLinger(true, 0);
        OutputStream out = socket.getOutputStream();
        byte[] buf = new byte[byteCount];
        final long uidTxBytesBefore = TrafficStats.getUidTxBytes(Process.myUid());
        out.write(buf);
        out.close();
        socket.close();
        long uidTxBytesAfter = TrafficStats.getUidTxBytes(Process.myUid());
        long uidTxDeltaBytes = uidTxBytesAfter - uidTxBytesBefore;
        assertTrue("uidtxb: " + uidTxBytesBefore + " -> " + uidTxBytesAfter + " delta="
                + uidTxDeltaBytes + " >= " + byteCount, uidTxDeltaBytes >= byteCount);
    }

    static final int UNSUPPORTED = -1;

    /**
     * Verify that get TrafficStats of a different uid failed because of the permission is not
     * granted to a third-party app.
     * <p>Tests Permission:
     *   {@link android.Manifest.permission#UPDATE_DEVICE_STATS}.
     */
    @SmallTest
    @Test
    public void testGetStatsOfOtherUid() throws Exception {
        // Test get stats of another uid failed since the current process does not have permission
        assertEquals(UNSUPPORTED, TrafficStats.getUidRxBytes(/*root uid*/ 0));
    }
}
