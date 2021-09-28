/*
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

package android.appsecurity.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Scanner;

/**
 * Runs the n portion of the listening ports test. With /proc/net access removed for third
 * party apps the device side test could no longer directly parse the file for listening ports. This
 * class pulls the /proc/net files from the device and passes their contents as a parameter to the
 * device side test that will then check for any listening ports and perform any necessary
 * localhost connection tests.
 */
public class ListeningPortsTest extends DeviceTestCase implements IBuildReceiver {

    private static final String DEVICE_TEST_APK = "CtsListeningPortsTest.apk";
    private static final String DEVICE_TEST_PKG = "android.appsecurity.cts.listeningports";
    private static final String DEVICE_TEST_CLASS = DEVICE_TEST_PKG + ".ListeningPortsTest";
    private static final String DEVICE_TEST_METHOD = "testNoAccessibleListeningPorts";

    private static final String PROC_FILE_CONTENTS_PARAM = "procFileContents";
    private static final String IS_TCP_PARAM = "isTcp";
    private static final String LOOPBACK_PARAM = "loopback";

    private IBuildInfo mCtsBuild;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Utils.prepareSingleUser(getDevice());
        assertNotNull(mCtsBuild);
        installDeviceTestPkg();
    }

    @Override
    protected void tearDown() throws Exception {
        try {
            uninstallDeviceTestPackage();
        } catch (DeviceNotAvailableException ignored) {
        } finally {
            super.tearDown();
        }
    }

    private void installDeviceTestPkg() throws Exception {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File apk = buildHelper.getTestFile(DEVICE_TEST_APK);
        String result = getDevice().installPackage(apk, true);
        assertNull("failed to install " + DEVICE_TEST_APK + ", Reason: " + result, result);
    }

    private String uninstallDeviceTestPackage() throws DeviceNotAvailableException {
        return getDevice().uninstallPackage(DEVICE_TEST_PKG);
    }

    /**
     * Remotely accessible ports are often used by attackers to gain
     * unauthorized access to computers systems without user knowledge or
     * awareness.
     */
    public void testNoRemotelyAccessibleListeningTcpPorts() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/tcp", true, false);
    }

    /**
     * Remotely accessible ports are often used by attackers to gain
     * unauthorized access to computers systems without user knowledge or
     * awareness.
     */
    public void testNoRemotelyAccessibleListeningTcp6Ports() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/tcp6", true, false);
    }

    /**
     * Remotely accessible ports are often used by attackers to gain
     * unauthorized access to computers systems without user knowledge or
     * awareness.
     */
    public void testNoRemotelyAccessibleListeningUdpPorts() throws Exception {
        assertNoRemotelyAccessibleListeningUdpPorts("/proc/net/udp", false);
    }

    /**
     * Remotely accessible ports are often used by attackers to gain
     * unauthorized access to computers systems without user knowledge or
     * awareness.
     */
    /*
    public void testNoRemotelyAccessibleListeningUdp6Ports() throws Exception {
        assertNoRemotelyAccessibleListeningUdpPorts("/proc/net/udp6", false);
    }
    */

    /**
     * Locally accessible ports are often targeted by malicious locally
     * installed programs to gain unauthorized access to program data or
     * cause system corruption.
     *
     * In all cases, a local listening IP port can be replaced by a UNIX domain
     * socket. Unix domain sockets can be protected with unix filesystem
     * permission. Alternatively, you can use getsockopt(SO_PEERCRED) to
     * determine if a program is authorized to connect to your socket.
     *
     * Please convert loopback IP connections to unix domain sockets.
     */
    public void testNoListeningLoopbackTcpPorts() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/tcp", true, true);
    }

    /**
     * Locally accessible ports are often targeted by malicious locally
     * installed programs to gain unauthorized access to program data or
     * cause system corruption.
     *
     * In all cases, a local listening IP port can be replaced by a UNIX domain
     * socket. Unix domain sockets can be protected with unix filesystem
     * permission. Alternatively, you can use getsockopt(SO_PEERCRED) to
     * determine if a program is authorized to connect to your socket.
     *
     * Please convert loopback IP connections to unix domain sockets.
     */
    public void testNoListeningLoopbackTcp6Ports() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/tcp6", true, true);
    }

    /**
     * Locally accessible ports are often targeted by malicious locally
     * installed programs to gain unauthorized access to program data or
     * cause system corruption.
     *
     * In all cases, a local listening IP port can be replaced by a UNIX domain
     * socket. Unix domain sockets can be protected with unix filesystem
     * permission.  Alternately, or you can use setsockopt(SO_PASSCRED) to
     * send credentials, and recvmsg to retrieve the passed credentials.
     *
     * Please convert loopback IP connections to unix domain sockets.
     */
    public void testNoListeningLoopbackUdpPorts() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/udp", false, true);
    }

    /**
     * Locally accessible ports are often targeted by malicious locally
     * installed programs to gain unauthorized access to program data or
     * cause system corruption.
     *
     * In all cases, a local listening IP port can be replaced by a UNIX domain
     * socket. Unix domain sockets can be protected with unix filesystem
     * permission.  Alternately, or you can use setsockopt(SO_PASSCRED) to
     * send credentials, and recvmsg to retrieve the passed credentials.
     *
     * Please convert loopback IP connections to unix domain sockets.
     */
    public void testNoListeningLoopbackUdp6Ports() throws Exception {
        assertNoAccessibleListeningPorts("/proc/net/udp6", false, true);
    }

    private static final int RETRIES_MAX = 6;

    /**
     * UDP tests can be flaky due to DNS lookups.  Compensate.
     */
    private void assertNoRemotelyAccessibleListeningUdpPorts(String procFilePath, boolean loopback)
            throws Exception {
        for (int i = 0; i < RETRIES_MAX; i++) {
            try {
                assertNoAccessibleListeningPorts(procFilePath, false, loopback);
                return;
            } catch (AssertionError e) {
                // If an AssertionError is caught then a listening UDP port was detected on the
                // device; since this could be due to a DNS lookup sleep and retry the test. If
                // the test continues to fail then report the error.
                if (i == RETRIES_MAX - 1) {
                    throw e;
                }
                Thread.sleep(2 * 1000 * i);
            }
        }
        throw new IllegalStateException("unreachable");
    }


    /**
     * Passes the contents of the /proc/net file and the boolean arguments to the device side test
     * which performs the necessary checks to determine if the port is listening and if it is
     * accessible from the localhost.
     */
    private void assertNoAccessibleListeningPorts(String procFilePath, boolean isTcp,
            boolean loopback) throws Exception {
        Map<String, String> args = new HashMap<>();
        String procFileContents = parse(procFilePath);
        args.put(PROC_FILE_CONTENTS_PARAM, procFileContents);
        args.put(IS_TCP_PARAM, String.valueOf(isTcp));
        args.put(LOOPBACK_PARAM, String.valueOf(loopback));
        Utils.runDeviceTests(getDevice(), DEVICE_TEST_PKG, DEVICE_TEST_CLASS, DEVICE_TEST_METHOD,
                args);
    }

    /**
     * Since the device side test cannot directly access the /proc/net files this method pulls the
     * file from the device and returns a String with its contents which can then be passed as an
     * argument to the device side test.
     */
    private String parse(String procFilePath) throws IOException, DeviceNotAvailableException {
        File procFile = File.createTempFile("ListeningPortsTest", "tmp");
        getDevice().pullFile(procFilePath, procFile);
        procFile.deleteOnExit();

        Scanner scanner = null;
        StringBuilder contents = new StringBuilder();
        // When passing the file contents as an argument to the device side method a 'No
        // instrumentation found' error was reported due to spaces in the String. To prevent this
        // the entire contents of the file need to be quoted before passing it as an argument.
        contents.append("'");
        try {
            scanner = new Scanner(procFile);
            while (scanner.hasNextLine()) {
                contents.append(scanner.nextLine() + "\n");
            }
        } finally {
            if (scanner != null) {
                scanner.close();
            }
        }
        contents.append("'");
        return contents.toString();
    }
}
