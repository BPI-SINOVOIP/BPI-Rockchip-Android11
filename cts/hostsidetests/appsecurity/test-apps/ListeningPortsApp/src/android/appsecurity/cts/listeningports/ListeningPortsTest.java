/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.appsecurity.cts.listeningports;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Process;
import android.os.UserHandle;
import android.test.AndroidTestCase;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import junit.framework.AssertionFailedError;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;
import java.util.regex.Pattern;

/**
 * Verifies that Android devices are not listening on accessible
 * open ports. Open ports are often targeted by attackers looking to break
 * into computer systems remotely, and minimizing the number of open ports
 * is considered a security best practice.
 */
public class ListeningPortsTest extends AndroidTestCase {
    private static final String TAG = "ListeningPortsTest";

    private static final String PROC_FILE_CONTENTS_PARAM = "procFileContents";
    private static final String IS_TCP_PARAM = "isTcp";
    private static final String LOOPBACK_PARAM = "loopback";

    private static final int CONN_TIMEOUT_IN_MS = 5000;

    /** Ports that are allowed to be listening. */
    private static final List<String> EXCEPTION_PATTERNS = new ArrayList<String>(6);

    static {
        // IPv4 exceptions
        // Patterns containing ":" are allowed address port combinations
        // Pattterns contains " " are allowed address UID combinations
        // Patterns containing both are allowed address, port, and UID combinations
        EXCEPTION_PATTERNS.add("0.0.0.0:5555");     // emulator port
        EXCEPTION_PATTERNS.add("0.0.0.0:9101");     // verified ports
        EXCEPTION_PATTERNS.add("0.0.0.0:9551");     // verified ports
        EXCEPTION_PATTERNS.add("0.0.0.0:9552");     // verified ports
        EXCEPTION_PATTERNS.add("10.0.2.15:5555");   // net forwarding for emulator
        EXCEPTION_PATTERNS.add("127.0.0.1:5037");   // adb daemon "smart sockets"
        EXCEPTION_PATTERNS.add("0.0.0.0 1020");     // used by the cast receiver
        EXCEPTION_PATTERNS.add("0.0.0.0 10000");    // used by the cast receiver
        EXCEPTION_PATTERNS.add("127.0.0.1 10000");  // used by the cast receiver
        EXCEPTION_PATTERNS.add(":: 1002");          // used by remote control
        EXCEPTION_PATTERNS.add(":: 1020");          // used by remote control
        EXCEPTION_PATTERNS.add("0.0.0.0:7275");     // used by supl
        //no current patterns involve address, port and UID combinations
        //Example for when necessary: EXCEPTION_PATTERNS.add("0.0.0.0:5555 10000")

        // IPv6 exceptions
        // TODO: this is not standard notation for IPv6. Use [$addr]:$port instead as per RFC 3986.
        EXCEPTION_PATTERNS.add(":::5555");          // emulator port for adb
        EXCEPTION_PATTERNS.add(":::7275");          // used by supl
    }

    /**
     * Remotely accessible ports (loopback==false) are often used by
     * attackers to gain unauthorized access to computers systems without
     * user knowledge or awareness.
     *
     * Locally accessible ports (loopback==true) are often targeted by
     * malicious locally installed programs to gain unauthorized access to
     * program data or cause system corruption.
     *
     * Since direct /proc/net access is no longer possible the contents of the file and the boolean
     * values are received as parameters from the host side test.
     */
    public void testNoAccessibleListeningPorts() throws Exception {
        final Bundle testArgs = InstrumentationRegistry.getArguments();
        final String procFileContents = testArgs.getString(PROC_FILE_CONTENTS_PARAM);
        final boolean isTcp = Boolean.valueOf(testArgs.getString(IS_TCP_PARAM));
        final boolean loopback = Boolean.valueOf(testArgs.getString(LOOPBACK_PARAM));

        String errors = "";
        List<ParsedProcEntry> entries = ParsedProcEntry.parse(procFileContents);
        for (ParsedProcEntry entry : entries) {
            String addrPort = entry.localAddress.getHostAddress() + ':' + entry.port;
            String addrUid = entry.localAddress.getHostAddress() + ' ' + entry.uid;
            String addrPortUid = addrPort + ' ' + entry.uid;

            if (isPortListening(entry.state, isTcp)
                    && !(isException(addrPort) || isException(addrUid) || isException(addrPortUid))
                    && (!entry.localAddress.isLoopbackAddress() ^ loopback)) {
                if (isTcp && !isTcpConnectable(entry.localAddress, entry.port)) {
                    continue;
                }
                // allow non-system processes to listen
                int appId = UserHandle.getAppId(entry.uid);
                if (appId >= Process.FIRST_APPLICATION_UID
                        && appId <= Process.LAST_APPLICATION_UID) {
                    continue;
                }
                errors += "\nFound port listening on addr="
                        + entry.localAddress.getHostAddress() + ", port="
                        + entry.port + ", UID=" + entry.uid
                        + " " + uidToPackage(entry.uid);
            }
        }
        if (!errors.equals("")) {
            throw new ListeningPortsAssertionError(errors);
        }
    }

    private String uidToPackage(int uid) {
        PackageManager pm = this.getContext().getPackageManager();
        String[] packages = pm.getPackagesForUid(uid);
        if (packages == null) {
            return "[unknown]";
        }
        return Arrays.asList(packages).toString();
    }

    private boolean isTcpConnectable(InetAddress address, int port) {
        Socket socket = new Socket();

        try {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Trying to connect " + address + ":" + port);
            }
            socket.connect(new InetSocketAddress(address, port), CONN_TIMEOUT_IN_MS);
        } catch (IOException ioe) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Unable to connect:" + ioe);
            }
            return false;
        } finally {
            try {
                socket.close();
            } catch (IOException closeError) {
                Log.e(TAG, "Unable to close socket: " + closeError);
            }
        }

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, address + ":" + port + " is connectable.");
        }
        return true;
    }

    private static boolean isException(String localAddress) {
        return isPatternMatch(EXCEPTION_PATTERNS, localAddress);
    }

    private static boolean isPatternMatch(List<String> patterns, String input) {
        for (String pattern : patterns) {
            pattern = Pattern.quote(pattern);
            if (Pattern.matches(pattern, input)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isPortListening(String state, boolean isTcp) {
        // 0A = TCP_LISTEN from include/net/tcp_states.h
        String listeningState = isTcp ? "0A" : "07";
        return listeningState.equals(state);
    }

    private static class ListeningPortsAssertionError extends AssertionFailedError {
        private ListeningPortsAssertionError(String msg) {
            super(msg);
        }
    }

    private static class ParsedProcEntry {
        private final InetAddress localAddress;
        private final int port;
        private final String state;
        private final int uid;

        private ParsedProcEntry(InetAddress addr, int port, String state, int uid) {
            this.localAddress = addr;
            this.port = port;
            this.state = state;
            this.uid = uid;
        }


        private static List<ParsedProcEntry> parse(String procFileContents) throws IOException {

            List<ParsedProcEntry> retval = new ArrayList<ParsedProcEntry>();
            /*
            * Sample output of "cat /proc/net/tcp" on emulator:
            *
            * sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  ...
            * 0: 0100007F:13AD 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0   ...
            * 1: 00000000:15B3 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0   ...
            * 2: 0F02000A:15B3 0202000A:CE8A 01 00000000:00000000 00:00000000 00000000     0   ...
            *
            */

            Scanner scanner = null;
            try {
                scanner = new Scanner(procFileContents);
                while (scanner.hasNextLine()) {
                    String line = scanner.nextLine().trim();

                    // Skip column headers
                    if (line.startsWith("sl")) {
                        continue;
                    }

                    String[] fields = line.split("\\s+");
                    final int expectedNumColumns = 12;
                    assertTrue(line + " should have at least " + expectedNumColumns
                            + " columns of output " + Arrays.toString(fields),
                            fields.length >= expectedNumColumns);

                    String state = fields[3];
                    int uid = Integer.parseInt(fields[7]);
                    InetAddress localIp = addrToInet(fields[1].split(":")[0]);
                    int localPort = Integer.parseInt(fields[1].split(":")[1], 16);

                    retval.add(new ParsedProcEntry(localIp, localPort, state, uid));
                }
            } finally {
                if (scanner != null) {
                    scanner.close();
                }
            }
            return retval;
        }

        /**
         * Convert a string stored in little endian format to an IP address.
         */
        private static InetAddress addrToInet(String s) throws UnknownHostException {
            int len = s.length();
            if (len != 8 && len != 32) {
                throw new IllegalArgumentException(len + "");
            }
            byte[] retval = new byte[len / 2];

            for (int i = 0; i < len / 2; i += 4) {
                retval[i] = (byte) ((Character.digit(s.charAt(2*i + 6), 16) << 4)
                        + Character.digit(s.charAt(2*i + 7), 16));
                retval[i + 1] = (byte) ((Character.digit(s.charAt(2*i + 4), 16) << 4)
                        + Character.digit(s.charAt(2*i + 5), 16));
                retval[i + 2] = (byte) ((Character.digit(s.charAt(2*i + 2), 16) << 4)
                        + Character.digit(s.charAt(2*i + 3), 16));
                retval[i + 3] = (byte) ((Character.digit(s.charAt(2*i), 16) << 4)
                        + Character.digit(s.charAt(2*i + 1), 16));
            }
            return InetAddress.getByAddress(retval);
        }
    }
}
