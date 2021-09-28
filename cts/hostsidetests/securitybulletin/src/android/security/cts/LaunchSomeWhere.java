/**
 * Copyright (C) 2018 The Android Open Source Project
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

import com.android.tradefed.device.ITestDevice;

/*
 * Adding Tests:
 * We are testing a series of exploits that all take advantage of binder in the
 * same way, using a malformed parcel to get system permission, with the only
 * difference being the details of how we create the malformed parcel. In order
 * to take advantage of these similarities (among other reasons) we share code
 * between these exploits with an app that only requires two things to run a new
 * version of this exploit: a class implementing IGenerateMalformedParcel and an
 * intent telling the app which version of the exploit to run.
 *
 * When you recieve a new LaunchAnyWhere exploit it will likely be in the form
 * of an app that can perform a number of actions such as creating a new pin
 * or installing an app without recieving the appropriate permissions. However,
 * the only file we care about form the app will be GenMalformedParcel.java.
 * Find that file and follow these steps to add a new LaunchAnyWhere test:
 *
 * 1. Copy GenMalformedParcel.java into the LaunchAnyWhere app at
 *    cts/hostsidetests/security/test-apps/launchanywhere/src... Rename the file
 *    and class after the CVE that you are addressing. Modify the class
 *    signature and method signature so that it implements
 *    IGenerateMalformedParcel (namely, add the `implements` clause and change
 *    the function to public Parcel generate(Intent intent)).
 *
 * 2. Next, add a hostside test to the appropriate file in this directory.
 *    In the test all you have to do is call
 *    LaunchSomeWhere.launchSomeWhere("CVE_20XX_XXXXX", getDevice());
 *
 * 3. Verify your test and submit, assuming all went well. If not then check
 *    for differences between the files in the submitted apk and the code in
 *    tests/tests/security/src/android/security/cts/launchanywhere.
 *
 * Exploit Overview:
 * All LaunchAnyWhere exploits take advantage of classes that write more data
 * than they read. They follow the same process to send an intent with system
 * permissions. The process is described below (you do not need to understand
 * this in order to create tests, but we learned this while debugging some
 * things and don't want the information to be lost):
 *
 * 1. Add an account with the account type 'com.launchanywhere' When an account
 *    is added the AccountManager delegates the task of authenticating the
 *    account to an instance of AbstractAccountAuthenticator. Our malicious
 *    authenticator finds
 *    android.accounts.IAccountAuthenticatorResponse.Stub.Proxy and replaces
 *    it's mRemote field with our anonymous IBinder before returning a
 *    default-constructed bundle. We save the old value and delegate to it
 *    after altering the arguments when appropriate (MitM).
 *
 * 2. When we finish, our IBinder's transact is called. At this point we create
 *    a reboot intent and send it to the appropriate class to generate the
 *    malformed parcel. This grants the intent system permissions.
 *
 * 3. The phone reboots, proving a successful exploit.
 */
class LaunchSomeWhere {
    public static void launchSomeWhere(String cve, ITestDevice device)
        throws Exception {

        String command = "am start";

        String[] args = {
            "--es", "cve", cve,
            "-n", "com.android.security.cts.launchanywhere/.StartExploit"
        };

        for (String s : args) {
          command += " " + s;
        }

        AdbUtils.runCommandLine(command, device);
        if (device.waitForDeviceNotAvailable(9_000))
            device.waitForDeviceAvailable();
    }
}