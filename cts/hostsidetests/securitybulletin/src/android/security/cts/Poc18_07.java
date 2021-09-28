/**
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

package android.security.cts;

import android.platform.test.annotations.SecurityTest;
import org.junit.Test;
import org.junit.runner.RunWith;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import static org.junit.Assert.*;

@RunWith(DeviceJUnit4ClassRunner.class)
public class Poc18_07 extends SecurityTestCase {

   /**
    * b/76221123
    */
    @Test
    @SecurityTest(minPatchLevel = "2018-07")
    public void testPocCVE_2018_9424() throws Exception {
        AdbUtils.runPocAssertNoCrashes(
            "CVE-2018-9424", getDevice(), "android\\.hardware\\.drm@\\d\\.\\d-service");
    }

    /*
     * CVE-2017-18275
     */
    @Test
    @SecurityTest(minPatchLevel = "2018-07")
    public void testPocCVE_2017_18275() throws Exception {
      String command =
          "am startservice "
          + "-n com.qualcomm.simcontacts/com.qualcomm.simcontacts.SimAuthenticateService "
          + "-a android.accounts.AccountAuthenticator -e account_name cve_2017_18275";
      String result = AdbUtils.runCommandLine(
          "pm list packages | grep com.qualcomm.simcontacts", getDevice());
      if (result.contains("com.qualcomm.simcontacts")) {
          AdbUtils.runCommandLine("logcat -c", getDevice());
          AdbUtils.runCommandLine(command, getDevice());
          String logcat = AdbUtils.runCommandLine("logcat -d", getDevice());
          assertNotMatchesMultiLine(
                "Authenticator: Add SIM account.*ContactsProvider: Accounts changed",
                logcat);
      }
    }
}
