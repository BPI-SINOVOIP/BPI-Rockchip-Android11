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
package android.cts.statsd.subscriber;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import com.android.compatibility.common.util.CpuFeatures;
import com.android.internal.os.StatsdConfigProto;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.SystemUptime;
import com.android.os.ShellConfig;
import com.android.os.statsd.ShellDataProto;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.testtype.DeviceTestCase;
import com.google.common.io.Files;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/**
 * Statsd shell data subscription test.
 */
public class ShellSubscriberTest extends DeviceTestCase {
    private int sizetBytes;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        sizetBytes = getSizetBytes();
    }

    public void testShellSubscription() {
        if (sizetBytes < 0) {
            return;
        }

        ShellConfig.ShellSubscription config = createConfig();
        CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        startSubscription(config, receiver, /*maxTimeoutForCommandSec=*/5,
                /*subscriptionTimeSec=*/5);
        checkOutput(receiver);
    }

    public void testShellSubscriptionReconnect() {
        if (sizetBytes < 0) {
            return;
        }

        ShellConfig.ShellSubscription config = createConfig();
        for (int i = 0; i < 5; i++) {
            CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
            // A subscription time of -1 means that statsd will not impose a timeout on the
            // subscription. Thus, the client will exit before statsd ends the subscription.
            startSubscription(config, receiver, /*maxTimeoutForCommandSec=*/5,
                    /*subscriptionTimeSec=*/-1);
            checkOutput(receiver);
        }
    }

    private int getSizetBytes() {
        try {
            ITestDevice device = getDevice();
            if (CpuFeatures.isArm64(device)) {
                return 8;
            }
            if (CpuFeatures.isArm32(device)) {
                return 4;
            }
            return -1;
        } catch (DeviceNotAvailableException e) {
            return -1;
        }
    }

    // Choose a pulled atom that is likely to be supported on all devices (SYSTEM_UPTIME). Testing
    // pushed atoms is trickier because executeShellCommand() is blocking, so we cannot push a
    // breadcrumb event while the shell subscription is running.
    private ShellConfig.ShellSubscription createConfig() {
        return ShellConfig.ShellSubscription.newBuilder()
                .addPulled(ShellConfig.PulledAtomSubscription.newBuilder()
                        .setMatcher(StatsdConfigProto.SimpleAtomMatcher.newBuilder()
                                .setAtomId(Atom.SYSTEM_UPTIME_FIELD_NUMBER))
                        .setFreqMillis(2000))
                .build();
    }

    /**
     * @param maxTimeoutForCommandSec maximum time imposed by adb that the command will run
     * @param subscriptionTimeSec maximum time imposed by statsd that the subscription will last
     */
    private void startSubscription(
            ShellConfig.ShellSubscription config,
            CollectingByteOutputReceiver receiver,
            int maxTimeoutForCommandSec,
            int subscriptionTimeSec) {
        LogUtil.CLog.d("Uploading the following config:\n" + config.toString());
        try {
            File configFile = File.createTempFile("shellconfig", ".config");
            configFile.deleteOnExit();
            int length = config.toByteArray().length;
            byte[] combined = new byte[sizetBytes + config.toByteArray().length];

            System.arraycopy(IntToByteArrayLittleEndian(length), 0, combined, 0, sizetBytes);
            System.arraycopy(config.toByteArray(), 0, combined, sizetBytes, length);

            Files.write(combined, configFile);
            String remotePath = "/data/local/tmp/" + configFile.getName();
            getDevice().pushFile(configFile, remotePath);
            LogUtil.CLog.d("waiting....................");

            String cmd = String.join(" ", "cat", remotePath, "|", "cmd stats data-subscribe",
                  String.valueOf(subscriptionTimeSec));


            getDevice().executeShellCommand(cmd, receiver, maxTimeoutForCommandSec,
                    /*maxTimeToOutputShellResponse=*/maxTimeoutForCommandSec, TimeUnit.SECONDS,
                    /*retryAttempts=*/0);
            getDevice().executeShellCommand("rm " + remotePath);
        } catch (Exception e) {
            fail(e.getMessage());
        }
    }

    private byte[] IntToByteArrayLittleEndian(int length) {
        ByteBuffer b = ByteBuffer.allocate(sizetBytes);
        b.order(ByteOrder.LITTLE_ENDIAN);
        b.putInt(length);
        return b.array();
    }

    // We do not know how much data will be returned, but we can check the data format.
    private void checkOutput(CollectingByteOutputReceiver receiver) {
        int atomCount = 0;
        int startIndex = 0;

        byte[] output = receiver.getOutput();
        assertThat(output.length).isGreaterThan(0);
        while (output.length > startIndex) {
            assertThat(output.length).isAtLeast(startIndex + sizetBytes);
            int dataLength = readSizetFromByteArray(output, startIndex);
            if (dataLength == 0) {
                // We have received a heartbeat from statsd. This heartbeat isn't accompanied by any
                // atoms so return to top of while loop.
                startIndex += sizetBytes;
                continue;
            }
            assertThat(output.length).isAtLeast(startIndex + sizetBytes + dataLength);

            ShellDataProto.ShellData data = null;
            try {
                int dataStart = startIndex + sizetBytes;
                int dataEnd = dataStart + dataLength;
                data = ShellDataProto.ShellData.parseFrom(
                        Arrays.copyOfRange(output, dataStart, dataEnd));
            } catch (InvalidProtocolBufferException e) {
                fail("Failed to parse proto");
            }

            assertThat(data.getAtomCount()).isEqualTo(1);
            assertThat(data.getAtom(0).hasSystemUptime()).isTrue();
            assertThat(data.getAtom(0).getSystemUptime().getUptimeMillis()).isGreaterThan(0L);
            atomCount++;
            startIndex += sizetBytes + dataLength;
        }
        assertThat(atomCount).isGreaterThan(0);
    }

    // Converts the bytes in range [startIndex, startIndex + sizetBytes) from a little-endian array
    // into an integer. Even though sizetBytes could be greater than 4, we assume that the result
    // will fit within an int.
    private int readSizetFromByteArray(byte[] arr, int startIndex) {
        int value = 0;
        for (int j = 0; j < sizetBytes; j++) {
            value += ((int) arr[j + startIndex] & 0xffL) << (8 * j);
        }
        return value;
    }
}
