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

package android.hdmicec.cts;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.RunUtil;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.util.concurrent.TimeUnit;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Pattern;

import org.junit.rules.ExternalResource;

/** Class that helps communicate with the cec-client */
public final class HdmiCecClientWrapper extends ExternalResource {

    private static final String CEC_CONSOLE_READY = "waiting for input";
    private static final int MILLISECONDS_TO_READY = 10000;
    private static final int DEFAULT_TIMEOUT = 20000;
    private static final int BUFFER_SIZE = 1024;

    private Process mCecClient;
    private BufferedWriter mOutputConsole;
    private BufferedReader mInputConsole;
    private boolean mCecClientInitialised = false;

    private LogicalAddress targetDevice;
    private String clientParams[];

    public HdmiCecClientWrapper(LogicalAddress targetDevice, String ...clientParams) {
        this.targetDevice = targetDevice;
        this.clientParams = clientParams;
    }

    @Override
    protected void before() throws Throwable {
        this.init();
    };

    @Override
    protected void after() {
        this.killCecProcess();
    };

    /** Initialise the client */
    private void init() throws Exception {
        List<String> commands = new ArrayList();

        commands.add("cec-client");
        /* "-p 2" starts the client as if it is connected to HDMI port 2, taking the physical
         * address 2.0.0.0 */
        commands.add("-p");
        commands.add("2");
        /* "-t x" starts the client as a TV device */
        commands.add("-t");
        commands.add("x");
        commands.addAll(Arrays.asList(clientParams));

        mCecClient = RunUtil.getDefault().runCmdInBackground(commands);
        mInputConsole = new BufferedReader(new InputStreamReader(mCecClient.getInputStream()));

        /* Wait for the client to become ready */
        mCecClientInitialised = true;
        if (checkConsoleOutput(CecClientMessage.CLIENT_CONSOLE_READY + "", MILLISECONDS_TO_READY)) {
            mOutputConsole = new BufferedWriter(
                                new OutputStreamWriter(mCecClient.getOutputStream()), BUFFER_SIZE);
            return;
        }

        mCecClientInitialised = false;

        throw (new Exception("Could not initialise cec-client process"));
    }

    private void checkCecClient() throws Exception {
        if (!mCecClientInitialised) {
            throw new Exception("cec-client not initialised!");
        }
        if (!mCecClient.isAlive()) {
            throw new Exception("cec-client not running!");
        }
    }

    /**
     * Sends a CEC message with source marked as broadcast to the device passed in the constructor
     * through the output console of the cec-communication channel.
     */
    public void sendCecMessage(CecOperand message) throws Exception {
        sendCecMessage(LogicalAddress.BROADCAST, targetDevice, message, "");
    }

    /**
     * Sends a CEC message from source device to the device passed in the constructor through the
     * output console of the cec-communication channel.
     */
    public void sendCecMessage(LogicalAddress source, CecOperand message) throws Exception {
        sendCecMessage(source, targetDevice, message, "");
    }

    /**
     * Sends a CEC message from source device to a destination device through the output console of
     * the cec-communication channel.
     */
    public void sendCecMessage(LogicalAddress source, LogicalAddress destination,
        CecOperand message) throws Exception {
        sendCecMessage(source, destination, message, "");
    }

    /**
     * Sends a CEC message from source device to a destination device through the output console of
     * the cec-communication channel with the appended params.
     */
    public void sendCecMessage(LogicalAddress source, LogicalAddress destination,
            CecOperand message, String params) throws Exception {
        checkCecClient();
        mOutputConsole.write("tx " + source + destination + ":" + message + params);
        mOutputConsole.newLine();
        mOutputConsole.flush();
    }

    /**
     * Sends a <USER_CONTROL_PRESSED> and <USER_CONTROL_RELEASED> from source to destination
     * through the output console of the cec-communication channel with the mentioned keycode.
     */
    public void sendUserControlPressAndRelease(LogicalAddress source, LogicalAddress destination,
            int keycode, boolean holdKey) throws Exception {
        sendUserControlPress(source, destination, keycode, holdKey);
        /* Sleep less than 200ms between press and release */
        TimeUnit.MILLISECONDS.sleep(100);
        mOutputConsole.write("tx " + source + destination + ":" +
                              CecOperand.USER_CONTROL_RELEASED);
        mOutputConsole.flush();
    }

    /**
     * Sends a <UCP> message from source to destination through the output console of the
     * cec-communication channel with the mentioned keycode. If holdKey is true, the method will
     * send multiple <UCP> messages to simulate a long press. No <UCR> will be sent.
     */
    public void sendUserControlPress(LogicalAddress source, LogicalAddress destination,
            int keycode, boolean holdKey) throws Exception {
        String key = String.format("%02x", keycode);
        String command = "tx " + source + destination + ":" +
                CecOperand.USER_CONTROL_PRESSED + ":" + key;

        if (holdKey) {
            /* Repeat once between 200ms and 450ms for at least 5 seconds. Since message will be
             * sent once later, send 16 times in loop every 300ms. */
            int repeat = 16;
            for (int i = 0; i < repeat; i++) {
                mOutputConsole.write(command);
                mOutputConsole.newLine();
                mOutputConsole.flush();
                TimeUnit.MILLISECONDS.sleep(300);
            }
        }

        mOutputConsole.write(command);
        mOutputConsole.newLine();
        mOutputConsole.flush();
    }

    /**
     * Sends a series of <UCP> [firstKeycode] from source to destination through the output console
     * of the cec-communication channel immediately followed by <UCP> [secondKeycode]. No <UCR>
     *  message is sent.
     */
    public void sendUserControlInterruptedPressAndHold(
        LogicalAddress source, LogicalAddress destination,
            int firstKeycode, int secondKeycode, boolean holdKey) throws Exception {
        sendUserControlPress(source, destination, firstKeycode, holdKey);
        /* Sleep less than 200ms between press and release */
        TimeUnit.MILLISECONDS.sleep(100);
        sendUserControlPress(source, destination, secondKeycode, false);
    }

    /** Sends a message to the output console of the cec-client */
    public void sendConsoleMessage(String message) throws Exception {
        checkCecClient();
        CLog.v("Sending message:: " + message);
        mOutputConsole.write(message);
        mOutputConsole.flush();
    }

    /** Check for any string on the input console of the cec-client, uses default timeout */
    public boolean checkConsoleOutput(String expectedMessage) throws Exception {
        return checkConsoleOutput(expectedMessage, DEFAULT_TIMEOUT);
    }

    /** Check for any string on the input console of the cec-client */
    public boolean checkConsoleOutput(String expectedMessage,
                                       long timeoutMillis) throws Exception {
        checkCecClient();
        long startTime = System.currentTimeMillis();
        long endTime = startTime;

        while ((endTime - startTime <= timeoutMillis)) {
            if (mInputConsole.ready()) {
                String line = mInputConsole.readLine();
                if (line.contains(expectedMessage)) {
                    CLog.v("Found " + expectedMessage + " in " + line);
                    return true;
                }
            }
            endTime = System.currentTimeMillis();
        }
        return false;
    }

    /** Gets all the messages received from the given source device during a period of duration
     * seconds.
     */
    public List<CecOperand> getAllMessages(LogicalAddress source, int duration) throws Exception {
        List<CecOperand> receivedOperands = new ArrayList<>();
        long startTime = System.currentTimeMillis();
        long endTime = startTime;
        Pattern pattern = Pattern.compile("(.*>>)(.*?)" +
                "(" + source + "\\p{XDigit}):(.*)",
            Pattern.CASE_INSENSITIVE);

        while ((endTime - startTime <= duration)) {
            if (mInputConsole.ready()) {
                String line = mInputConsole.readLine();
                if (pattern.matcher(line).matches()) {
                    CecOperand operand = CecMessage.getOperand(line);
                    if (!receivedOperands.contains(operand)) {
                        receivedOperands.add(operand);
                    }
                }
            }
            endTime = System.currentTimeMillis();
        }
        return receivedOperands;
    }


    /**
     * Looks for the CEC expectedMessage broadcast on the cec-client communication channel and
     * returns the first line that contains that message within default timeout. If the CEC message
     * is not found within the timeout, an exception is thrown.
     */
    public String checkExpectedOutput(CecOperand expectedMessage) throws Exception {
        return checkExpectedOutput(LogicalAddress.BROADCAST, expectedMessage, DEFAULT_TIMEOUT);
    }

    /**
     * Looks for the CEC expectedMessage sent to CEC device toDevice on the cec-client
     * communication channel and returns the first line that contains that message within
     * default timeout. If the CEC message is not found within the timeout, an exception is thrown.
     */
    public String checkExpectedOutput(LogicalAddress toDevice,
                                      CecOperand expectedMessage) throws Exception {
        return checkExpectedOutput(toDevice, expectedMessage, DEFAULT_TIMEOUT);
    }

    /**
     * Looks for the CEC expectedMessage broadcast on the cec-client communication channel and
     * returns the first line that contains that message within timeoutMillis. If the CEC message
     * is not found within the timeout, an exception is thrown.
     */
    public String checkExpectedOutput(CecOperand expectedMessage,
                                      long timeoutMillis) throws Exception {
        return checkExpectedOutput(LogicalAddress.BROADCAST, expectedMessage, timeoutMillis);
    }

    /**
     * Looks for the CEC expectedMessage sent to CEC device toDevice on the cec-client
     * communication channel and returns the first line that contains that message within
     * timeoutMillis. If the CEC message is not found within the timeout, an exception is thrown.
     */
    public String checkExpectedOutput(LogicalAddress toDevice, CecOperand expectedMessage,
                                       long timeoutMillis) throws Exception {
        checkCecClient();
        long startTime = System.currentTimeMillis();
        long endTime = startTime;
        Pattern pattern = Pattern.compile("(.*>>)(.*?)" +
                                          "(" + targetDevice + toDevice + "):" +
                                          "(" + expectedMessage + ")(.*)",
                                          Pattern.CASE_INSENSITIVE);

        while ((endTime - startTime <= timeoutMillis)) {
            if (mInputConsole.ready()) {
                String line = mInputConsole.readLine();
                if (pattern.matcher(line).matches()) {
                    CLog.v("Found " + expectedMessage.name() + " in " + line);
                    return line;
                }
            }
            endTime = System.currentTimeMillis();
        }
        throw new Exception("Could not find message " + expectedMessage.name());
    }

    /**
     * Looks for the CEC message incorrectMessage sent to CEC device toDevice on the cec-client
     * communication channel and throws an exception if it finds the line that contains the message
     * within the default timeout. If the CEC message is not found within the timeout, function
     * returns without error.
     */
    public void checkOutputDoesNotContainMessage(LogicalAddress toDevice,
            CecOperand incorrectMessage) throws Exception {
        checkOutputDoesNotContainMessage(toDevice, incorrectMessage, DEFAULT_TIMEOUT);
     }

    /**
     * Looks for the CEC message incorrectMessage sent to CEC device toDevice on the cec-client
     * communication channel and throws an exception if it finds the line that contains the message
     * within timeoutMillis. If the CEC message is not found within the timeout, function returns
     * without error.
     */
    public void checkOutputDoesNotContainMessage(LogicalAddress toDevice, CecOperand incorrectMessage,
            long timeoutMillis) throws Exception {

        checkCecClient();
        long startTime = System.currentTimeMillis();
        long endTime = startTime;
        Pattern pattern = Pattern.compile("(.*>>)(.*?)" +
                                          "(" + targetDevice + toDevice + "):" +
                                          "(" + incorrectMessage + ")(.*)",
                                          Pattern.CASE_INSENSITIVE);

        while ((endTime - startTime <= timeoutMillis)) {
            if (mInputConsole.ready()) {
                String line = mInputConsole.readLine();
                if (pattern.matcher(line).matches()) {
                    CLog.v("Found " + incorrectMessage.name() + " in " + line);
                    throw new Exception("Found " + incorrectMessage.name() + " to " + toDevice +
                            " with params " + CecMessage.getParamsAsString(line));
                }
            }
            endTime = System.currentTimeMillis();
        }
     }

    /**
     * Kills the cec-client process that was created in init().
     */
    private void killCecProcess() {
        try {
            checkCecClient();
            sendConsoleMessage(CecClientMessage.QUIT_CLIENT.toString());
            mOutputConsole.close();
            mInputConsole.close();
            mCecClientInitialised = false;
            if (!mCecClient.waitFor(MILLISECONDS_TO_READY, TimeUnit.MILLISECONDS)) {
                /* Use a pkill cec-client if the cec-client process is not dead in spite of the
                 * quit above.
                 */
                List<String> commands = new ArrayList<>();
                Process killProcess;
                commands.add("pkill");
                commands.add("cec-client");
                killProcess = RunUtil.getDefault().runCmdInBackground(commands);
                killProcess.waitFor();
            }
        } catch (Exception e) {
            /* If cec-client is not running, do not throw an exception, just return. */
            CLog.w("Unable to close cec-client", e);
        }
    }
}
