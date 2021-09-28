/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.bootstats.cts;

import static com.google.common.truth.Truth.assertThat;

import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.os.AtomsProto.Atom;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IDeviceTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.LinkedList;


/**
 * Set of tests that verify statistics collection during boot.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class BootStatsHostTest implements IDeviceTest {

    private static final long MAX_WAIT_TIME_MS = 100000;
    private static final long WAIT_SLEEP_MS = 100;

    private static int[] ATOMS_EXPECTED = {
            Atom.BOOT_TIME_EVENT_DURATION_REPORTED_FIELD_NUMBER,
            Atom.BOOT_TIME_EVENT_ELAPSED_TIME_REPORTED_FIELD_NUMBER
    };

    private ITestDevice mDevice;

    @Test
    public void testBootStats() throws Exception {
        final int apiLevel = getDevice().getApiLevel();
        Assume.assumeFalse("Skipping test because boot time metrics were introduced"
                + " in Android 8.0. Current API Level " + apiLevel,
                apiLevel < 26 /* Build.VERSION_CODES.O */);

        if (apiLevel <= 29 /* Build.VERSION_CODES.Q */) {
            testBootStatsForApiLevel29AndBelow();
            return;
        }

        // Clear buffer to make it easier to find new logs
        getDevice().executeShellCommand("logcat --buffer=events --clear");

        // reboot device
        getDevice().rebootUntilOnline();

        LinkedList<String> expectedAtomHeaders = new LinkedList<>();
        // example format: Atom 239->(total count)5, (error count)0
        for (int atom : ATOMS_EXPECTED) {
            expectedAtomHeaders.add("Atom " + atom + "->(total count)");
        }
        long timeoutMs = System.currentTimeMillis() + MAX_WAIT_TIME_MS;
        while (System.currentTimeMillis() < timeoutMs) {
            LinkedList<String> notExistingAtoms = checkAllExpectedAtoms(expectedAtomHeaders);
            if (notExistingAtoms.isEmpty()) {
                return;
            }
            Thread.sleep(WAIT_SLEEP_MS);
        }
        assertThat(checkAllExpectedAtoms(expectedAtomHeaders)).isEmpty();
    }

    /** Check all atoms are available and return atom headers not available */
    private LinkedList<String> checkAllExpectedAtoms(LinkedList<String> expectedAtomHeaders)
            throws Exception {
        LinkedList<String> notExistingAtoms = new LinkedList<>(expectedAtomHeaders);
        String log = getDevice().executeShellCommand("cmd stats print-stats");
        for (String atom : expectedAtomHeaders) {
            int atomIndex = log.indexOf(atom);
            if (atomIndex < 0) {
                continue;
            }
            int numberOfEvents = getIntValue(log, atomIndex + atom.length());
            if (numberOfEvents <= 0) {
                continue;
            }
            // valid event happened.
            notExistingAtoms.remove(atom);
        }
        return notExistingAtoms;
    }

    // extract the value from the string starting from index till ',''
    private int getIntValue(String str, int index) throws Exception {
        int lastIndex = index;
        for (int i = index; i < str.length(); i++) {
            if (str.charAt(i) == ',') {
                lastIndex = i;
                break;
            }
        }
        String valueStr = str.substring(index, lastIndex);
        int value = Integer.valueOf(valueStr);
        return value;
    }

    /** Need to keep the old version of test for api 27, 28, 29 as new version 
        of tests can be used on devices with old Android versions */
    private void testBootStatsForApiLevel29AndBelow() throws Exception {
        long startTime = System.currentTimeMillis();
        // Clear buffer to make it easier to find new logs
        getDevice().executeShellCommand("logcat --buffer=events --clear");

        // reboot device
        getDevice().rebootUntilOnline();
        waitForBootCompleted();
        int upperBoundSeconds = (int) ((System.currentTimeMillis() - startTime) / 1000);

        // wait for logs to post
        Thread.sleep(10000);

        // find logs and parse them
        // ex: sysui_multi_action: [757,804,799,ota_boot_complete,801,85,802,1]
        // ex: 757,804,799,counter_name,801,bucket_value,802,increment_value
        final String bucketTag = Integer.toString(MetricsEvent.RESERVED_FOR_LOGBUILDER_BUCKET);
        final String counterNameTag = Integer.toString(MetricsEvent.RESERVED_FOR_LOGBUILDER_NAME);
        final String counterNamePattern = counterNameTag + ",boot_complete,";
        final String multiActionPattern = "sysui_multi_action: [";

        final String log = getDevice().executeShellCommand("logcat --buffer=events -d");

        int counterNameIndex = log.indexOf(counterNamePattern);
        Assert.assertTrue("did not find boot logs", counterNameIndex != -1);

        int multiLogStart = log.lastIndexOf(multiActionPattern, counterNameIndex);
        multiLogStart += multiActionPattern.length();
        int multiLogEnd = log.indexOf("]", multiLogStart);
        String[] multiLogDataStrings = log.substring(multiLogStart, multiLogEnd).split(",");

        boolean foundBucket = false;
        int bootTime = 0;
        for (int i = 0; i < multiLogDataStrings.length; i += 2) {
            if (bucketTag.equals(multiLogDataStrings[i])) {
                foundBucket = true;
                Assert.assertTrue("histogram data was truncated",
                        (i + 1) < multiLogDataStrings.length);
                bootTime = Integer.valueOf(multiLogDataStrings[i + 1]);
            }
        }
        Assert.assertTrue("log line did not contain a tag " + bucketTag, foundBucket);
        Assert.assertTrue("reported boot time must be less than observed boot time",
                bootTime < upperBoundSeconds);
        Assert.assertTrue("reported boot time must be non-zero", bootTime > 0);
    }

    private boolean isBootCompleted() throws Exception {
        return "1".equals(getDevice().executeShellCommand("getprop sys.boot_completed").trim());
    }

    private void waitForBootCompleted() throws Exception {
        for (int i = 0; i < 45; i++) {
            if (isBootCompleted()) {
                return;
            }
            Thread.sleep(1000);
        }
        throw new AssertionError("System failed to become ready!");
    }

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }
}
