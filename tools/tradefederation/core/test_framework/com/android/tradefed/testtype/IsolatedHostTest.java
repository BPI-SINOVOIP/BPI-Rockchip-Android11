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
package com.android.tradefed.testtype;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.isolation.FilterSpec;
import com.android.tradefed.isolation.JUnitEvent;
import com.android.tradefed.isolation.RunnerMessage;
import com.android.tradefed.isolation.RunnerOp;
import com.android.tradefed.isolation.RunnerReply;
import com.android.tradefed.isolation.TestParameters;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.SystemUtil;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.ProcessBuilder.Redirect;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Implements a TradeFed runner that uses a subprocess to execute the tests in a low-dependency
 * environment instead of executing them on the main process.
 */
@OptionClass(alias = "isolated-host-test")
public class IsolatedHostTest
        implements IRemoteTest,
                IBuildReceiver,
                ITestAnnotationFilterReceiver,
                ITestFilterReceiver,
                ITestCollector {
    @Option(
            name = "class",
            description =
                    "The JUnit test classes to run, in the format <package>.<class>. eg."
                            + " \"com.android.foo.Bar\". This field can be repeated.",
            importance = Importance.IF_UNSET)
    private Set<String> mClasses = new LinkedHashSet<>();

    @Option(
            name = "method",
            description =
                    "The name of the method in the JUnit TestCase to run. " + "eg. \"testFooBar\"",
            importance = Importance.IF_UNSET)
    private String mMethodName;

    @Option(
            name = "jar",
            description = "The jars containing the JUnit test class to run.",
            importance = Importance.IF_UNSET)
    private Set<String> mJars = new HashSet<String>();

    @Option(
            name = "socket-timeout",
            description =
                    "The longest allowable time between messages from the subprocess before "
                            + "assuming that it has malfunctioned or died.",
            importance = Importance.IF_UNSET)
    private int mSocketTimeout = 1 * 60 * 1000;

    @Option(
            name = "include-annotation",
            description = "The set of annotations a test must have to be run.")
    private Set<String> mIncludeAnnotations = new HashSet<>();

    @Option(
            name = "exclude-annotation",
            description =
                    "The set of annotations to exclude tests from running. A test must have "
                            + "none of the annotations in this list to run.")
    private Set<String> mExcludeAnnotations = new HashSet<>();

    @Option(
            name = "java-flags",
            description =
                    "The set of flags to pass to the Java subprocess for complicated test "
                            + "needs.")
    private List<String> mJavaFlags = new ArrayList<>();

    @Option(
            name = "exclude-paths",
            description = "The (prefix) paths to exclude from searching in the jars.")
    private Set<String> mExcludePaths = new HashSet<>();

    private IBuildInfo mBuildInfo;
    private Set<String> mIncludeFilters = new HashSet<>();
    private Set<String> mExcludeFilters = new HashSet<>();
    private boolean mCollectTestsOnly = false;

    private static final String ROOT_DIR = "ROOT_DIR";
    private static final List<String> ISOLATION_JARS =
            new ArrayList<>(Arrays.asList("tradefed-isolation.jar"));
    private ServerSocket mServer = null;

    /** {@inheritDoc} */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        try {
            mServer = new ServerSocket(0);
            mServer.setSoTimeout(mSocketTimeout);

            ArrayList<String> cmdArgs =
                    new ArrayList<>(
                            List.of(
                                    SystemUtil.getRunningJavaBinaryPath().getAbsolutePath(),
                                    "-cp",
                                    this.compileClassPath()));
            cmdArgs.addAll(mJavaFlags);
            cmdArgs.addAll(
                    List.of(
                            "com.android.tradefed.isolation.IsolationRunner",
                            "-",
                            "--port",
                            Integer.toString(mServer.getLocalPort()),
                            "--address",
                            mServer.getInetAddress().getHostAddress(),
                            "--timeout",
                            Integer.toString(mSocketTimeout)));

            CLog.v(String.join(" ", cmdArgs));
            RunUtil runner = new RunUtil();
            Process isolationRunner = runner.runCmdInBackground(Redirect.INHERIT, cmdArgs);
            CLog.v("Started subprocess.");

            Socket socket = mServer.accept();
            socket.setSoTimeout(mSocketTimeout);
            CLog.v("Connected to subprocess.");

            List<String> testJarAbsPaths = getJarPaths(mJars);

            TestParameters.Builder paramsBuilder =
                    TestParameters.newBuilder()
                            .addAllTestClasses(mClasses)
                            .addAllTestJarAbsPaths(testJarAbsPaths)
                            .addAllExcludePaths(mExcludePaths)
                            .setDryRun(mCollectTestsOnly);

            if (!mIncludeFilters.isEmpty()
                    || !mExcludeFilters.isEmpty()
                    || !mIncludeAnnotations.isEmpty()
                    || !mExcludeAnnotations.isEmpty()) {
                paramsBuilder.setFilter(
                        FilterSpec.newBuilder()
                                .addAllIncludeFilters(mIncludeFilters)
                                .addAllExcludeFilters(mExcludeFilters)
                                .addAllIncludeAnnotations(mIncludeAnnotations)
                                .addAllExcludeAnnotations(mExcludeAnnotations));
            }
            this.executeTests(socket, listener, paramsBuilder.build());

            RunnerMessage.newBuilder()
                    .setCommand(RunnerOp.RUNNER_OP_STOP)
                    .build()
                    .writeDelimitedTo(socket.getOutputStream());
        } catch (IOException e) {
            listener.testRunFailed(e.getMessage());
        } catch (ClassNotFoundException e) {
            listener.testRunFailed(e.getMessage());
        }
    }

    /**
     * Creates a classpath for the subprocess that includes the needed jars to run the tests
     *
     * @return a string specifying the colon separated classpath.
     */
    private String compileClassPath() throws ClassNotFoundException {
        List<String> paths = new ArrayList<>();
        IDeviceBuildInfo build = (IDeviceBuildInfo) mBuildInfo;
        File testDir = build.getTestsDir();

        // This is a relatively hacky way to get around the fact that we don't have a consistent
        // way to locate tradefed related jars in all environments, so instead we dyn link to that
        // jar, and sniff where it is located.  It is very important that IsolationRunner does not
        // end up in the main tradefed.jar while this is in place.
        try {

            URI tradefedJarPath =
                    IsolatedHostTest.class
                            .getProtectionDomain()
                            .getCodeSource()
                            .getLocation()
                            .toURI();

            String isolationJarPath =
                    new File(tradefedJarPath).getParentFile().getAbsolutePath()
                            + "/tradefed-isolation.jar";

            paths.add(isolationJarPath);
        } catch (URISyntaxException e) {
            throw new RuntimeException(e);
        }

        for (String jar : mJars) {
            File f = FileUtil.findFile(testDir, jar);
            if (f != null && f.exists()) {
                paths.add(f.getAbsolutePath());
                paths.add(f.getParentFile().getAbsolutePath() + "/*");
            }
        }

        String jarClasspath = String.join(java.io.File.pathSeparator, paths);

        return jarClasspath;
    }

    /**
     * Runs the tests by talking to the subprocess assuming the setup is done.
     *
     * @param socket A socket connected to the subprocess control socket
     * @param listener The TradeFed invocation listener from run()
     * @param params The tests to run and their options
     * @throws IOException
     */
    private void executeTests(
            Socket socket, ITestInvocationListener listener, TestParameters params)
            throws IOException {
        RunnerMessage.newBuilder()
                .setCommand(RunnerOp.RUNNER_OP_RUN_TEST)
                .setParams(params)
                .build()
                .writeDelimitedTo(socket.getOutputStream());

        mainLoop:
        while (true) {
            try {
                RunnerReply reply = RunnerReply.parseDelimitedFrom(socket.getInputStream());
                switch (reply.getRunnerStatus()) {
                    case RUNNER_STATUS_FINISHED_OK:
                        CLog.v("Received message that runner finished successfully");
                        break mainLoop;
                    case RUNNER_STATUS_FINISHED_ERROR:
                        CLog.e("Received message that runner errored");
                        CLog.e("From Runner: " + reply.getMessage());
                        listener.testRunFailed(reply.getMessage());
                        break mainLoop;
                    case RUNNER_STATUS_STARTING:
                        CLog.v("Received message that runner is starting");
                        break;
                    default:
                        if (reply.hasTestEvent()) {
                            JUnitEvent event = reply.getTestEvent();
                            TestDescription desc;
                            switch (event.getTopic()) {
                                case TOPIC_FAILURE:
                                    desc =
                                            new TestDescription(
                                                    event.getClassName(), event.getMethodName());
                                    listener.testFailed(desc, event.getMessage());
                                    listener.testEnded(desc, new HashMap<String, String>());
                                    break;
                                case TOPIC_ASSUMPTION_FAILURE:
                                    desc =
                                            new TestDescription(
                                                    event.getClassName(), event.getMethodName());
                                    listener.testAssumptionFailure(desc, reply.getMessage());
                                    break;
                                case TOPIC_STARTED:
                                    desc =
                                            new TestDescription(
                                                    event.getClassName(), event.getMethodName());
                                    listener.testStarted(desc);
                                    break;
                                case TOPIC_FINISHED:
                                    desc =
                                            new TestDescription(
                                                    event.getClassName(), event.getMethodName());
                                    listener.testEnded(desc, new HashMap<String, String>());
                                    break;
                                case TOPIC_IGNORED:
                                    desc =
                                            new TestDescription(
                                                    event.getClassName(), event.getMethodName());
                                    listener.testIgnored(desc);
                                    break;
                                case TOPIC_RUN_STARTED:
                                    listener.testRunStarted(
                                            event.getClassName(), event.getTestCount());
                                    break;
                                case TOPIC_RUN_FINISHED:
                                    listener.testRunEnded(
                                            event.getElapsedTime(), new HashMap<String, String>());
                                    break;
                                default:
                            }
                        } else {
                        }
                }
            } catch (SocketTimeoutException e) {
                listener.testRunFailed(e.getMessage());
            }
        }
    }

    /**
     * Utility method to searh for absolute paths for JAR files. Largely the same as in the HostTest
     * implementation, but somewhat difficult to extract well due to the various method calls it
     * uses.
     */
    private List<String> getJarPaths(Set<String> jars) throws FileNotFoundException {
        Set<String> output = new HashSet<>();

        for (String jar : jars) {
            File jarFile = getJarFile(jar, mBuildInfo);
            output.add(jarFile.getAbsolutePath());
        }

        return output.stream().collect(Collectors.toList());
    }

    /**
     * Inspect several location where the artifact are usually located for different use cases to
     * find our jar.
     */
    private File getJarFile(String jarName, IBuildInfo buildInfo) throws FileNotFoundException {
        File jarFile = null;

        // Check tests dir
        if (buildInfo instanceof IDeviceBuildInfo) {
            IDeviceBuildInfo deviceBuildInfo = (IDeviceBuildInfo) buildInfo;
            File testDir = deviceBuildInfo.getTestsDir();
            jarFile = searchJarFile(testDir, jarName);
        }
        if (jarFile != null) {
            return jarFile;
        }

        // Check ROOT_DIR
        if (buildInfo.getBuildAttributes().get(ROOT_DIR) != null) {
            jarFile =
                    searchJarFile(new File(buildInfo.getBuildAttributes().get(ROOT_DIR)), jarName);
        }
        if (jarFile != null) {
            return jarFile;
        }
        throw new FileNotFoundException(String.format("Could not find jar: %s", jarName));
    }

    /** Looks for a jar file given a place to start and a filename. */
    private File searchJarFile(File baseSearchFile, String jarName) {
        if (baseSearchFile != null && baseSearchFile.isDirectory()) {
            File jarFile = FileUtil.findFile(baseSearchFile, jarName);
            if (jarFile != null && jarFile.isFile()) {
                return jarFile;
            }
        }
        return null;
    }

    /**
     * This allows us to pipe the subprocesses STDOUT and STDERR through CLog under appropriate log
     * levels. It uses a separate thread to handle each stream.
     */
    private class LogStreamHelper implements Runnable {
        private InputStream mStream = null;
        private boolean mIsErrorStream = false;

        public LogStreamHelper(InputStream stream, boolean isErrorStream) {
            mStream = stream;
        }

        @Override
        public void run() {
            if (mIsErrorStream) {
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(mStream))) {
                    reader.lines().forEach(line -> CLog.e("E/IsolationRunner: %s", line));
                } catch (Exception e) {
                    CLog.e(e);
                }
            } else {
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(mStream))) {
                    reader.lines().forEach(line -> CLog.v("V/IsolationRunner: %s", line));
                } catch (Exception e) {
                    CLog.e(e);
                }
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void setBuild(IBuildInfo build) {
        mBuildInfo = build;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void setCollectTestsOnly(boolean shouldCollectTest) {
        mCollectTestsOnly = shouldCollectTest;
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeAnnotation(String annotation) {
        mIncludeAnnotations.add(annotation);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeAnnotation(String notAnnotation) {
        mExcludeAnnotations.add(notAnnotation);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeAnnotation(Set<String> annotations) {
        mIncludeAnnotations.addAll(annotations);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeAnnotation(Set<String> notAnnotations) {
        mExcludeAnnotations.addAll(notAnnotations);
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeAnnotations() {
        return mIncludeAnnotations;
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeAnnotations() {
        return mExcludeAnnotations;
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeAnnotations() {
        mIncludeAnnotations.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeAnnotations() {
        mExcludeAnnotations.clear();
    }
}
