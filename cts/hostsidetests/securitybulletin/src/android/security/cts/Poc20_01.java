package android.security.cts;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class Poc20_01 extends SecurityTestCase {
    /**
     * CVE-2019-14002
     */
    @Test
    @SecurityTest(minPatchLevel = "2020-01")
    public void testPocCVE_2019_14002() throws Exception {
        String result =
                AdbUtils.runCommandLine(
                        "dumpsys package com.qualcomm.qti.callenhancement", getDevice());
        assertNotMatchesMultiLine("READ_EXTERNAL_STORAGE.*?WRITE_EXTERNAL_STORAGE", result);
    }
}
