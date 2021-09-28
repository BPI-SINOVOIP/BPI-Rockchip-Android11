/*
 * Copyright (C) 2011 The Android Open Source Project
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

package android.telephony.cts;

import android.app.Instrumentation;
import android.os.ParcelFileDescriptor;
import android.telephony.TelephonyManager;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

public class TelephonyUtils {
    private static final String COMMAND_ADD_TEST_EMERGENCY_NUMBER =
            "cmd phone emergency-number-test-mode -a ";

    private static final String COMMAND_REMOVE_TEST_EMERGENCY_NUMBER =
            "cmd phone emergency-number-test-mode -r ";

    private static final String COMMAND_END_BLOCK_SUPPRESSION = "cmd phone end-block-suppression";

    private static final String COMMAND_FLUSH_TELEPHONY_METRICS =
            "/system/bin/dumpsys activity service TelephonyDebugService --metricsproto";

    public static void addTestEmergencyNumber(Instrumentation instr, String testNumber)
            throws Exception {
        executeShellCommand(instr, COMMAND_ADD_TEST_EMERGENCY_NUMBER + testNumber);
    }

    public static void removeTestEmergencyNumber(Instrumentation instr, String testNumber)
            throws Exception {
        executeShellCommand(instr, COMMAND_REMOVE_TEST_EMERGENCY_NUMBER + testNumber);
    }

    public static void endBlockSuppression(Instrumentation instr) throws Exception {
        executeShellCommand(instr, COMMAND_END_BLOCK_SUPPRESSION);
    }

    public static void flushTelephonyMetrics(Instrumentation instr) throws Exception {
        executeShellCommand(instr, COMMAND_FLUSH_TELEPHONY_METRICS);
    }

    public static boolean isSkt(TelephonyManager telephonyManager) {
        return isOperator(telephonyManager, "45005");
    }

    public static boolean isKt(TelephonyManager telephonyManager) {
        return isOperator(telephonyManager, "45002")
                || isOperator(telephonyManager, "45004")
                || isOperator(telephonyManager, "45008");
    }

    private static boolean isOperator(TelephonyManager telephonyManager, String operator) {
        String simOperator = telephonyManager.getSimOperator();
        return simOperator != null && simOperator.equals(operator);
    }

    /**
     * Executes the given shell command and returns the output in a string. Note that even
     * if we don't care about the output, we have to read the stream completely to make the
     * command execute.
     */
    public static String executeShellCommand(Instrumentation instrumentation,
            String command) throws Exception {
        final ParcelFileDescriptor pfd =
                instrumentation.getUiAutomation().executeShellCommand(command);
        BufferedReader br = null;
        try (InputStream in = new FileInputStream(pfd.getFileDescriptor())) {
            br = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8));
            String str = null;
            StringBuilder out = new StringBuilder();
            while ((str = br.readLine()) != null) {
                out.append(str);
            }
            return out.toString();
        } finally {
            if (br != null) {
                closeQuietly(br);
            }
            closeQuietly(pfd);
        }
    }

    private static void closeQuietly(AutoCloseable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (RuntimeException rethrown) {
                throw rethrown;
            } catch (Exception ignored) {
            }
        }
    }
}
