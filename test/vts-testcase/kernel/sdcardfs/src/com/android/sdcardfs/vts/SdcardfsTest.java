/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import java.util.Scanner;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public final class SdcardfsTest extends BaseHostJUnit4Test {
    public static final String TAG = SdcardfsTest.class.getSimpleName();

    private static final int MIN_KERNEL_MAJOR = 5;
    private static final int MIN_KERNEL_MINOR = 4;

    private boolean should_run(String str) {
        Scanner versionScanner = new Scanner(str).useDelimiter("\\.");
        int major = versionScanner.nextInt();
        int minor = versionScanner.nextInt();
        if (major > MIN_KERNEL_MAJOR)
            return true;
        if (major < MIN_KERNEL_MAJOR)
            return false;
        return minor >= MIN_KERNEL_MINOR;
    }

    @Test
    public void testSdcardfsNotPresent() throws Exception {
        CommandResult result = getDevice().executeShellV2Command("uname -r");
        assertEquals(result.getStatus(), CommandStatus.SUCCESS);
        if (!should_run(result.getStdout()))
            return;
        String cmd = "mount | grep \"type sdcardfs\"";
        CLog.i("Invoke shell command [" + cmd + "]");
        try {
            String output = getDevice().executeShellCommand(cmd);
            assertEquals("Found sdcardfs entries:" + output, output, "");
        } catch (Exception e) {
            fail("Could not run command [" + cmd + "] (" + e.getMessage() + ")");
        }
    }
}
