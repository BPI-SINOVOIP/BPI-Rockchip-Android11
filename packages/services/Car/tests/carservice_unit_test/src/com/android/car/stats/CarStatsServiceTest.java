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

package com.android.car.stats;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.car.vms.VmsLayer;
import android.content.Context;
import android.content.pm.PackageManager;

import androidx.test.filters.SmallTest;

import com.android.car.stats.VmsClientLogger.ConnectionState;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.io.PrintWriter;
import java.io.StringWriter;

@SmallTest
@RunWith(JUnit4.class)
public class CarStatsServiceTest {
    private static final int CLIENT_UID = 10101;
    private static final int CLIENT_UID2 = 10102;
    private static final String CLIENT_PACKAGE = "test.package";
    private static final String CLIENT_PACKAGE2 = "test.package2";
    private static final VmsLayer LAYER = new VmsLayer(1, 2, 3);
    private static final VmsLayer LAYER2 = new VmsLayer(2, 3, 4);
    private static final VmsLayer LAYER3 = new VmsLayer(3, 4, 5);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private Context mContext;
    @Mock
    private PackageManager mPackageManager;

    private CarStatsService mCarStatsService;
    private StringWriter mDumpsysOutput;
    private PrintWriter mDumpsysWriter;

    @Before
    public void setUp() {
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mPackageManager.getNameForUid(CLIENT_UID)).thenReturn(CLIENT_PACKAGE);
        when(mPackageManager.getNameForUid(CLIENT_UID2)).thenReturn(CLIENT_PACKAGE2);

        mCarStatsService = new CarStatsService(mContext);
        mDumpsysOutput = new StringWriter();
        mDumpsysWriter = new PrintWriter(mDumpsysOutput);
    }

    @Test
    public void testEmptyStats() {
        mCarStatsService.dump(null, mDumpsysWriter, new String[0]);
        assertEquals(
                "uid,packageName,attempts,connected,disconnected,terminated,errors\n"
                        + "\nuid,layerType,layerChannel,layerVersion,"
                        + "txBytes,txPackets,rxBytes,rxPackets,droppedBytes,droppedPackets\n",
                mDumpsysOutput.toString());
    }

    @Test
    public void testLogConnectionState_Connecting() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTING);
        validateConnectionStats("10101,test.package,1,0,0,0,0");
    }

    @Test
    public void testLogConnectionState_Connected() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTED);
        validateConnectionStats("10101,test.package,0,1,0,0,0");
    }

    @Test
    public void testLogConnectionState_Disconnected() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.DISCONNECTED);
        validateConnectionStats("10101,test.package,0,0,1,0,0");
    }

    @Test
    public void testLogConnectionState_Terminated() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.TERMINATED);
        validateConnectionStats("10101,test.package,0,0,0,1,0");
    }

    @Test
    public void testLogConnectionState_ConnectionError() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTION_ERROR);
        validateConnectionStats("10101,test.package,0,0,0,0,1");
    }

    @Test
    public void testLogConnectionState_UnknownUID() {
        mCarStatsService.getVmsClientLogger(-1)
                .logConnectionState(ConnectionState.CONNECTING);
        testEmptyStats();
    }

    @Test
    public void testLogConnectionState_MultipleClients_MultipleStates() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTING);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTED);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.DISCONNECTED);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logConnectionState(ConnectionState.CONNECTED);

        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logConnectionState(ConnectionState.CONNECTING);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logConnectionState(ConnectionState.CONNECTED);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logConnectionState(ConnectionState.TERMINATED);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logConnectionState(ConnectionState.CONNECTING);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logConnectionState(ConnectionState.CONNECTION_ERROR);
        validateConnectionStats(
                "10101,test.package,1,2,1,0,0\n"
                + "10102,test.package2,2,1,0,1,1");
    }

    @Test
    public void testLogPacketSent() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 5);
        validateClientStats("10101,1,2,3,5,1,0,0,0,0");
    }

    @Test
    public void testLogPacketSent_MultiplePackets() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 1);

        validateClientStats("10101,1,2,3,6,3,0,0,0,0");
    }

    @Test
    public void testLogPacketSent_MultipleLayers() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER2, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER3, 1);

        validateClientStats(
                "10101,1,2,3,3,1,0,0,0,0\n"
                        + "10101,2,3,4,2,1,0,0,0,0\n"
                        + "10101,3,4,5,1,1,0,0,0,0");
    }

    @Test
    public void testLogPacketSent_MultipleClients() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketSent(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketSent(LAYER2, 1);

        validateDumpsys(
                "10101,test.package,0,0,0,0,0\n"
                        + "10102,test.package2,0,0,0,0,0\n",
                "10101,1,2,3,3,1,0,0,0,0\n"
                        + "10102,1,2,3,2,1,0,0,0,0\n"
                        + "10102,2,3,4,1,1,0,0,0,0\n");
    }

    @Test
    public void testLogPacketReceived() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 5);
        validateClientStats("10101,1,2,3,0,0,5,1,0,0");
    }

    @Test
    public void testLogPacketReceived_MultiplePackets() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 1);

        validateClientStats("10101,1,2,3,0,0,6,3,0,0");
    }

    @Test
    public void testLogPacketReceived_MultipleLayers() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER2, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER3, 1);

        validateClientStats(
                "10101,1,2,3,0,0,3,1,0,0\n"
                        + "10101,2,3,4,0,0,2,1,0,0\n"
                        + "10101,3,4,5,0,0,1,1,0,0");
    }

    @Test
    public void testLogPacketReceived_MultipleClients() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketReceived(LAYER2, 1);

        validateDumpsys(
                "10101,test.package,0,0,0,0,0\n"
                        + "10102,test.package2,0,0,0,0,0\n",
                "10101,1,2,3,0,0,3,1,0,0\n"
                        + "10102,1,2,3,0,0,2,1,0,0\n"
                        + "10102,2,3,4,0,0,1,1,0,0\n");
    }

    @Test
    public void testLogPacketDropped() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 5);
        validateClientStats("10101,1,2,3,0,0,0,0,5,1");
    }

    @Test
    public void testLogPacketDropped_MultiplePackets() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 1);

        validateClientStats("10101,1,2,3,0,0,0,0,6,3");
    }

    @Test
    public void testLogPacketDropped_MultipleLayers() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER2, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER3, 1);

        validateClientStats(
                "10101,1,2,3,0,0,0,0,3,1\n"
                        + "10101,2,3,4,0,0,0,0,2,1\n"
                        + "10101,3,4,5,0,0,0,0,1,1");
    }

    @Test
    public void testLogPacketDropped_MultipleClients() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketDropped(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketDropped(LAYER2, 1);

        validateDumpsys(
                "10101,test.package,0,0,0,0,0\n"
                        + "10102,test.package2,0,0,0,0,0\n",
                "10101,1,2,3,0,0,0,0,3,1\n"
                        + "10102,1,2,3,0,0,0,0,2,1\n"
                        + "10102,2,3,4,0,0,0,0,1,1\n");
    }

    @Test
    public void testLogPackets_MultipleClients_MultipleLayers_MultipleOperations() {
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketSent(LAYER, 3);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID)
                .logPacketDropped(LAYER, 1);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketReceived(LAYER, 2);
        mCarStatsService.getVmsClientLogger(CLIENT_UID2)
                .logPacketSent(LAYER2, 2);
        mCarStatsService.getVmsClientLogger(-1)
                .logPacketDropped(LAYER2, 12);


        validateDumpsys(
                "10101,test.package,0,0,0,0,0\n"
                        + "10102,test.package2,0,0,0,0,0\n",
                "-1,2,3,4,0,0,0,0,12,1\n"
                        + "10101,1,2,3,3,1,2,1,1,1\n"
                        + "10102,1,2,3,0,0,6,3,0,0\n"
                        + "10102,2,3,4,2,1,0,0,0,0\n");
    }


    private void validateConnectionStats(String vmsConnectionStats) {
        validateDumpsys(vmsConnectionStats + "\n", "");
    }

    private void validateClientStats(String vmsClientStats) {
        validateDumpsys(
                "10101,test.package,0,0,0,0,0\n",
                vmsClientStats + "\n");
    }

    private void validateDumpsys(String vmsConnectionStats, String vmsClientStats) {
        mCarStatsService.dump(null, mDumpsysWriter, new String[0]);
        assertEquals(
                "uid,packageName,attempts,connected,disconnected,terminated,errors\n"
                        + vmsConnectionStats
                        + "\n"
                        + "uid,layerType,layerChannel,layerVersion,"
                        + "txBytes,txPackets,rxBytes,rxPackets,droppedBytes,droppedPackets\n"
                        + vmsClientStats,
                mDumpsysOutput.toString());
    }
}
