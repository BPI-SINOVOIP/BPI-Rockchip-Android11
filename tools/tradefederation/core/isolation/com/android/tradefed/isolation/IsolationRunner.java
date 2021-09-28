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
package com.android.tradefed.isolation;

import com.android.tradefed.lite.DryRunner;
import com.android.tradefed.lite.HostUtils;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.junit.internal.runners.ErrorReportingRunner;
import org.junit.runner.JUnitCore;
import org.junit.runner.Request;
import org.junit.runner.Runner;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashSet;
import java.util.List;

/**
 * A process to remotely execute locally stored JUnit tests.
 *
 * <p>This class implements a process that attempts to: 1) Open a socket connection to a given
 * address and port. 2) Receive instructions on which tests to run from that socket connection. 3)
 * Setup result forwarding over the socket connection. 4) Execute the tests specified in #2 with the
 * result forwarders from #3.
 */
public final class IsolationRunner {
    private Socket mSocket = null;
    private ServerSocket mServer = null;

    public static void main(String[] args)
            throws ParseException, NumberFormatException, IOException {
        // Delegate to a utility function to process the command line arguments
        RunnerConfig flags = IsolationRunner.parseFlags(args);

        new IsolationRunner().startServer(flags);
    }

    private void startServer(RunnerConfig config) throws IOException {
        // connect to the host that started this process
        mSocket = new Socket(config.getAddress(), config.getPort());

        // Set a timeout for hearing something from the host when we start a read.
        mSocket.setSoTimeout(config.getTimeout());

        InputStream input = mSocket.getInputStream();
        OutputStream output = mSocket.getOutputStream();

        // Process messages by receiving and looping
        mainLoop:
        while (true) {
            RunnerMessage message = RunnerMessage.parseDelimitedFrom(mSocket.getInputStream());
            RunnerReply reply;
            switch (message.getCommand()) {
                case RUNNER_OP_STOP:
                    System.out.println("Received Stop Message");
                    reply =
                            RunnerReply.newBuilder()
                                    .setRunnerStatus(RunnerStatus.RUNNER_STATUS_FINISHED_OK)
                                    .build();
                    reply.writeDelimitedTo(output);
                    output.flush();
                    break mainLoop;
                case RUNNER_OP_RUN_TEST:
                    try {
                        this.runTests(output, message.getParams());
                    } catch (ClassNotFoundException e) {
                        RunnerReply.newBuilder()
                                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_FINISHED_ERROR)
                                .setMessage(e.toString())
                                .build()
                                .writeDelimitedTo(output);
                        output.flush();
                    }
                    break;
                default:
                    System.out.println("Received unrecognized message");
            }
        }

        if (mSocket != null) {
            mSocket.close();
            mSocket = null;
        }
        if (mServer != null) {
            mServer.close();
            mServer = null;
        }
    }

    private void runTests(OutputStream output, TestParameters params)
            throws ClassNotFoundException, IOException {
        List<Class<?>> klasses = this.getClasses(params);

        for (Class<?> klass : klasses) {
            System.out.println("Running class: " + klass);
            IsolationResultForwarder list = new IsolationResultForwarder(output);
            JUnitCore runnerCore = new JUnitCore();
            runnerCore.addListener(list);

            Request req = Request.aClass(klass);
            if (params.hasFilter()) {
                req = req.filterWith(new IsolationFilter(params.getFilter()));
            }

            if (req.getRunner() instanceof ErrorReportingRunner) {
                // TODO(b/147610871): Handle ErrorReportingRunner errors in the IsolationRunner
                // There needs to be an error of some sort here, but right now I
                // don't have a way to report an error for a single test class.
                System.err.println(
                        String.format("Found ErrorRunner when trying to run class: %s", klass));
            } else {
                Runner checkRunner;
                if (params.getDryRun()) {
                    checkRunner = new DryRunner(req.getRunner().getDescription());
                } else {
                    checkRunner = req.getRunner();
                }

                runnerCore.run(checkRunner);
            }
        }

        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_FINISHED_OK)
                .build()
                .writeDelimitedTo(output);
    }

    private List<Class<?>> getClasses(TestParameters params) {
        System.out.println("Excluded paths:");
        params.getExcludePathsList().stream().forEach(path -> System.out.println(path));
        return HostUtils.getJUnitClasses(
                new HashSet<>(params.getTestClassesList()),
                new HashSet<>(params.getTestJarAbsPathsList()),
                params.getExcludePathsList(),
                this.getClass().getClassLoader());
    }

    private static final class RunnerConfig {
        public static final int DEFAULT_PORT = 3000;
        public static final String DEFAULT_ADDRESS = "127.0.0.1";
        public static final int DEFAULT_TIMEOUT = 5 * 60 * 1000;

        private final int mPort;
        private final String mAddress;
        private final int mTimeout;

        public RunnerConfig(int port, String address, int timeout) {
            if (port > 0) {
                mPort = port;
            } else {
                mPort = RunnerConfig.DEFAULT_PORT;
            }

            if (address != null) {
                mAddress = address;
            } else {
                mAddress = RunnerConfig.DEFAULT_ADDRESS;
            }

            if (timeout > 0) {
                mTimeout = timeout;
            } else {
                mTimeout = RunnerConfig.DEFAULT_TIMEOUT;
            }
        }

        public int getPort() {
            return mPort;
        }

        public String getAddress() {
            return mAddress;
        }

        public int getTimeout() {
            return mTimeout;
        }
    }

    private static RunnerConfig parseFlags(String[] args)
            throws ParseException, NumberFormatException {
        Options options = new Options();

        Option portOption = new Option("p", "port", true, "Port to use for IPC");
        portOption.setRequired(false);
        options.addOption(portOption);

        Option addressOption = new Option("a", "address", true, "Address to use for IPC");
        addressOption.setRequired(false);
        options.addOption(addressOption);

        Option timeoutOption = new Option("t", "timeout", true, "Timeout to read from the socket");
        timeoutOption.setRequired(false);
        options.addOption(timeoutOption);

        CommandLineParser parser = new PosixParser();
        HelpFormatter formatter = new HelpFormatter();
        CommandLine cmd;

        cmd = parser.parse(options, args);

        String portStr = cmd.getOptionValue("p");
        String addressStr = cmd.getOptionValue("a");
        String timeoutStr = cmd.getOptionValue("t");

        int port = -1;
        String address = null;
        int timeout = -1;

        if (addressStr != null && !addressStr.isEmpty()) {
            address = addressStr;
        }
        if (portStr != null && !portStr.isEmpty()) {
            port = Integer.parseInt(portStr);
        }
        if (timeoutStr != null && !timeoutStr.isEmpty()) {
            timeout = Integer.parseInt(timeoutStr);
        }

        return new RunnerConfig(port, address, timeout);
    }
}
