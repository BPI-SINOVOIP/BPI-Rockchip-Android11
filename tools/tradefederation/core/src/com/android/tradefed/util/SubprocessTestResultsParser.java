/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed.util;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.invoker.logger.TfObjectTracker;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.SubprocessEventHelper.BaseTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.FailedTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.LogAssociationEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestLogEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestModuleStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestStartedEventInfo;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.base.Splitter;
import com.google.common.base.Strings;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Extends {@link FileOutputStream} to parse the output before writing to the file so we can
 * generate the test events on the launcher side.
 */
public class SubprocessTestResultsParser implements Closeable {

    private ITestInvocationListener mListener;

    private TestDescription mCurrentTest = null;
    private IInvocationContext mCurrentModuleContext = null;

    private Pattern mPattern = null;
    private Map<String, EventHandler> mHandlerMap = null;
    private EventReceiverThread mEventReceiver = null;
    private IInvocationContext mContext = null;
    private Long mStartTime = null;
    // Ignore the testLog events, rely only on logAssociation
    private boolean mIgnoreTestLog = true;
    // Keep track of which files we received TEST_LOG event from.
    private Set<String> mTestLogged = new HashSet<>();

    /** Relevant test status keys. */
    public static class StatusKeys {
        public static final String INVOCATION_FAILED = "INVOCATION_FAILED";
        public static final String TEST_ASSUMPTION_FAILURE = "TEST_ASSUMPTION_FAILURE";
        public static final String TEST_ENDED = "TEST_ENDED";
        public static final String TEST_FAILED = "TEST_FAILED";
        public static final String TEST_IGNORED = "TEST_IGNORED";
        public static final String TEST_STARTED = "TEST_STARTED";
        public static final String TEST_RUN_ENDED = "TEST_RUN_ENDED";
        public static final String TEST_RUN_FAILED = "TEST_RUN_FAILED";
        public static final String TEST_RUN_STARTED = "TEST_RUN_STARTED";
        public static final String TEST_MODULE_STARTED = "TEST_MODULE_STARTED";
        public static final String TEST_MODULE_ENDED = "TEST_MODULE_ENDED";
        public static final String TEST_LOG = "TEST_LOG";
        public static final String LOG_ASSOCIATION = "LOG_ASSOCIATION";
        public static final String INVOCATION_STARTED = "INVOCATION_STARTED";
        public static final String INVOCATION_ENDED = "INVOCATION_ENDED";
    }

    /**
     * Internal receiver thread class with a socket.
     */
    private class EventReceiverThread extends Thread {
        private ServerSocket mSocket;
        // initial state: 1 permit available, joins that don't wait for connection will succeed
        private Semaphore mSemaphore = new Semaphore(1);
        private boolean mShouldParse = true;

        public EventReceiverThread() throws IOException {
            super("EventReceiverThread");
            mSocket = new ServerSocket(0);
        }

        protected int getLocalPort() {
            return mSocket.getLocalPort();
        }

        /** @return True if parsing completes before timeout (optionally waiting for connection). */
        boolean await(long millis, boolean waitForConnection) throws InterruptedException {
            // As only 1 permit is available prior to connecting, changing the number of permits
            // requested controls whether the receiver will wait for a connection.
            int permits = waitForConnection ? 2 : 1;
            if (mSemaphore.tryAcquire(permits, millis, TimeUnit.MILLISECONDS)) {
                mSemaphore.release(permits);
                return true;
            }
            return false;
        }

        public void cancel() throws IOException {
            if (mSocket != null) {
                mSocket.close();
            }
        }

        /**
         * When reaching some issues, we might want to terminate the buffer of the socket to spy
         * which events are still in the pipe.
         */
        public void stopParsing() {
            mShouldParse = false;
        }

        @Override
        public void run() {
            Socket client = null;
            BufferedReader in = null;
            try {
                client = mSocket.accept();
                mSemaphore.acquire(); // connected: 0 permits available, all joins will wait
                in = new BufferedReader(new InputStreamReader(client.getInputStream()));
                String event = null;
                while ((event = in.readLine()) != null) {
                    try {
                        if (mShouldParse) {
                            CLog.d("received event: '%s'", event);
                            parse(event);
                        } else {
                            CLog.d("Skipping parsing of event: '%s'", event);
                        }
                    } catch (JSONException e) {
                        CLog.e(e);
                    }
                }
            } catch (IOException | InterruptedException e) {
                CLog.e(e);
            } finally {
                StreamUtil.close(in);
                mSemaphore.release(2); // finished: 2 permits available, all joins succeed
            }
            CLog.d("EventReceiverThread done.");
        }
    }

    /**
     * Wait for the event receiver to finish processing events. Will wait even if a connection
     * wasn't established, i.e. processing hasn't begun yet.
     *
     * @param millis timeout in milliseconds.
     * @return True if receiver thread terminate before timeout, False otherwise.
     */
    public boolean joinReceiver(long millis) {
        return joinReceiver(millis, true);
    }

    /**
     * Wait for the event receiver to finish processing events.
     *
     * @param millis timeout in milliseconds.
     * @param waitForConnection False to skip waiting if a connection was never established.
     * @return True if receiver thread terminate before timeout, False otherwise.
     */
    public boolean joinReceiver(long millis, boolean waitForConnection) {
        if (mEventReceiver != null) {
            try {
                CLog.i("Waiting for events to finish being processed.");
                if (!mEventReceiver.await(millis, waitForConnection)) {
                    mEventReceiver.stopParsing();
                    CLog.e("Event receiver thread did not complete. Some events may be missing.");
                    return false;
                }
            } catch (InterruptedException e) {
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        return true;
    }

    /**
     * Returns the socket receiver that was open. -1 if none.
     */
    public int getSocketServerPort() {
        if (mEventReceiver != null) {
            return mEventReceiver.getLocalPort();
        }
        return -1;
    }

    /** Whether or not to ignore testLog events and only rely on logAssociation. */
    public void setIgnoreTestLog(boolean ignoreTestLog) {
        mIgnoreTestLog = ignoreTestLog;
    }

    @Override
    public void close() throws IOException {
        if (mEventReceiver != null) {
            mEventReceiver.cancel();
        }
    }

    /**
     * Constructor for the result parser
     *
     * @param listener {@link ITestInvocationListener} where to report the results
     * @param streaming if True, a socket receiver will be open to receive results.
     * @param context a {@link IInvocationContext} information about the invocation
     */
    public SubprocessTestResultsParser(
            ITestInvocationListener listener, boolean streaming, IInvocationContext context)
            throws IOException {
        this(listener, context);
        if (streaming) {
            mEventReceiver = new EventReceiverThread();
            mEventReceiver.start();
        }
    }

    /**
     * Constructor for the result parser
     *
     * @param listener {@link ITestInvocationListener} where to report the results
     * @param context a {@link IInvocationContext} information about the invocation
     */
    public SubprocessTestResultsParser(
            ITestInvocationListener listener, IInvocationContext context) {
        mListener = listener;
        mContext = context;
        StringBuilder sb = new StringBuilder();
        sb.append(StatusKeys.INVOCATION_FAILED).append("|");
        sb.append(StatusKeys.TEST_ASSUMPTION_FAILURE).append("|");
        sb.append(StatusKeys.TEST_ENDED).append("|");
        sb.append(StatusKeys.TEST_FAILED).append("|");
        sb.append(StatusKeys.TEST_IGNORED).append("|");
        sb.append(StatusKeys.TEST_STARTED).append("|");
        sb.append(StatusKeys.TEST_RUN_ENDED).append("|");
        sb.append(StatusKeys.TEST_RUN_FAILED).append("|");
        sb.append(StatusKeys.TEST_RUN_STARTED).append("|");
        sb.append(StatusKeys.TEST_MODULE_STARTED).append("|");
        sb.append(StatusKeys.TEST_MODULE_ENDED).append("|");
        sb.append(StatusKeys.TEST_LOG).append("|");
        sb.append(StatusKeys.LOG_ASSOCIATION).append("|");
        sb.append(StatusKeys.INVOCATION_STARTED).append("|");
        sb.append(StatusKeys.INVOCATION_ENDED);
        String patt = String.format("(.*)(%s)( )(.*)", sb.toString());
        mPattern = Pattern.compile(patt);

        // Create Handler map for each event
        mHandlerMap = new HashMap<String, EventHandler>();
        mHandlerMap.put(StatusKeys.INVOCATION_FAILED, new InvocationFailedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_ASSUMPTION_FAILURE,
                new TestAssumptionFailureEventHandler());
        mHandlerMap.put(StatusKeys.TEST_ENDED, new TestEndedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_FAILED, new TestFailedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_IGNORED, new TestIgnoredEventHandler());
        mHandlerMap.put(StatusKeys.TEST_STARTED, new TestStartedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_RUN_ENDED, new TestRunEndedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_RUN_FAILED, new TestRunFailedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_RUN_STARTED, new TestRunStartedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_MODULE_STARTED, new TestModuleStartedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_MODULE_ENDED, new TestModuleEndedEventHandler());
        mHandlerMap.put(StatusKeys.TEST_LOG, new TestLogEventHandler());
        mHandlerMap.put(StatusKeys.LOG_ASSOCIATION, new LogAssociationEventHandler());
        mHandlerMap.put(StatusKeys.INVOCATION_STARTED, new InvocationStartedEventHandler());
        mHandlerMap.put(StatusKeys.INVOCATION_ENDED, new InvocationEndedEventHandler());
    }

    public void parseFile(File file) {
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(file));
        } catch (FileNotFoundException e) {
            CLog.e(e);
            throw new RuntimeException(e);
        }
        ArrayList<String> listString = new ArrayList<String>();
        String line = null;
        try {
            while ((line = reader.readLine()) != null) {
                listString.add(line);
            }
            reader.close();
        } catch (IOException e) {
            CLog.e(e);
            throw new RuntimeException(e);
        }
        processNewLines(listString.toArray(new String[listString.size()]));
    }

    /**
     * call parse on each line of the array to extract the events if any.
     */
    public void processNewLines(String[] lines) {
        for (String line : lines) {
            try {
                parse(line);
            } catch (JSONException e) {
                CLog.e("Exception while parsing");
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
    }

    /**
     * Parse a line, if it matches one of the events, handle it.
     */
    private void parse(String line) throws JSONException {
        Matcher matcher = mPattern.matcher(line);
        if (matcher.find()) {
            EventHandler handler = mHandlerMap.get(matcher.group(2));
            if (handler != null) {
                handler.handleEvent(matcher.group(4));
            } else {
                CLog.w("No handler found matching: %s", matcher.group(2));
            }
        }
    }

    private void checkCurrentTestId(String className, String testName) {
        if (mCurrentTest == null) {
            mCurrentTest = new TestDescription(className, testName);
            CLog.w("Calling a test event without having called testStarted.");
        }
    }

    /**
     * Interface for event handling
     */
    interface EventHandler {
        public void handleEvent(String eventJson) throws JSONException;
    }

    private class TestRunStartedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            TestRunStartedEventInfo rsi = new TestRunStartedEventInfo(new JSONObject(eventJson));
            if (rsi.mAttempt != null) {
                mListener.testRunStarted(
                        rsi.mRunName, rsi.mTestCount, rsi.mAttempt, rsi.mStartTime);
            } else {
                mListener.testRunStarted(rsi.mRunName, rsi.mTestCount);
            }
        }
    }

    private class TestRunFailedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            TestRunFailedEventInfo rfi = new TestRunFailedEventInfo(new JSONObject(eventJson));
            if (rfi.mFailure != null) {
                mListener.testRunFailed(rfi.mFailure);
            } else {
                mListener.testRunFailed(rfi.mReason);
            }
        }
    }

    private class TestRunEndedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            try {
                TestRunEndedEventInfo rei = new TestRunEndedEventInfo(new JSONObject(eventJson));
                // TODO: Parse directly as proto.
                mListener.testRunEnded(
                        rei.mTime, TfMetricProtoUtil.upgradeConvert(rei.mRunMetrics));
            } finally {
                mCurrentTest = null;
            }
        }
    }

    private class InvocationFailedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            InvocationFailedEventInfo ifi =
                    new InvocationFailedEventInfo(new JSONObject(eventJson));
            if (ifi.mFailure != null) {
                mListener.invocationFailed(ifi.mFailure);
            } else {
                mListener.invocationFailed(ifi.mCause);
            }
        }
    }

    private class TestStartedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            TestStartedEventInfo bti = new TestStartedEventInfo(new JSONObject(eventJson));
            mCurrentTest = new TestDescription(bti.mClassName, bti.mTestName);
            if (bti.mStartTime != null) {
                mListener.testStarted(mCurrentTest, bti.mStartTime);
            } else {
                mListener.testStarted(mCurrentTest);
            }
        }
    }

    private class TestFailedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            FailedTestEventInfo fti = new FailedTestEventInfo(new JSONObject(eventJson));
            checkCurrentTestId(fti.mClassName, fti.mTestName);
            mListener.testFailed(mCurrentTest, fti.mTrace);
        }
    }

    private class TestEndedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            try {
                TestEndedEventInfo tei = new TestEndedEventInfo(new JSONObject(eventJson));
                checkCurrentTestId(tei.mClassName, tei.mTestName);
                if (tei.mEndTime != null) {
                    mListener.testEnded(
                            mCurrentTest,
                            tei.mEndTime,
                            TfMetricProtoUtil.upgradeConvert(tei.mRunMetrics));
                } else {
                    mListener.testEnded(
                            mCurrentTest, TfMetricProtoUtil.upgradeConvert(tei.mRunMetrics));
                }
            } finally {
                mCurrentTest = null;
            }
        }
    }

    private class TestIgnoredEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            BaseTestEventInfo baseTestIgnored = new BaseTestEventInfo(new JSONObject(eventJson));
            checkCurrentTestId(baseTestIgnored.mClassName, baseTestIgnored.mTestName);
            mListener.testIgnored(mCurrentTest);
        }
    }

    private class TestAssumptionFailureEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            FailedTestEventInfo FailedAssumption =
                    new FailedTestEventInfo(new JSONObject(eventJson));
            checkCurrentTestId(FailedAssumption.mClassName, FailedAssumption.mTestName);
            mListener.testAssumptionFailure(mCurrentTest, FailedAssumption.mTrace);
        }
    }

    private class TestModuleStartedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            TestModuleStartedEventInfo module =
                    new TestModuleStartedEventInfo(new JSONObject(eventJson));
            mCurrentModuleContext = module.mModuleContext;
            mListener.testModuleStarted(module.mModuleContext);
        }
    }

    private class TestModuleEndedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            if (mCurrentModuleContext == null) {
                CLog.w("Calling testModuleEnded when testModuleStarted was not called.");
            }
            mListener.testModuleEnded();
            mCurrentModuleContext = null;
        }
    }

    private class TestLogEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            TestLogEventInfo logInfo = new TestLogEventInfo(new JSONObject(eventJson));
            if (mIgnoreTestLog) {
                FileUtil.deleteFile(logInfo.mDataFile);
                return;
            }
            String name = String.format("subprocess-%s", logInfo.mDataName);
            try (InputStreamSource data = new FileInputStreamSource(logInfo.mDataFile, true)) {
                mListener.testLog(name, logInfo.mLogType, data);
                mTestLogged.add(logInfo.mDataName);
            }
        }
    }

    private class LogAssociationEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            LogAssociationEventInfo assosInfo =
                    new LogAssociationEventInfo(new JSONObject(eventJson));
            LogFile file = assosInfo.mLoggedFile;
            if (Strings.isNullOrEmpty(file.getPath())) {
                CLog.e("Log '%s' was registered but without a path.", assosInfo.mDataName);
                return;
            }
            File path = new File(file.getPath());
            String name = String.format("subprocess-%s", assosInfo.mDataName);
            if (Strings.isNullOrEmpty(file.getUrl()) && path.exists()) {
                if (mTestLogged.contains(assosInfo.mDataName)) {
                    CLog.d(
                            "Already called testLog on %s, ignoring the logAssociation.",
                            assosInfo.mDataName);
                    return;
                }
                LogDataType type = file.getType();
                // File might have already been compressed
                File toLog = path;
                File extractedDir = null;
                if (file.getPath().endsWith(LogDataType.ZIP.getFileExt())) {
                    // If file type is compressed, then keep that type.
                    if (!file.getType().isCompressed()) {
                        try {
                            extractedDir = ZipUtil2.extractZipToTemp(path, assosInfo.mDataName);
                            File[] files = extractedDir.listFiles();
                            if (files.length == 1) {
                                toLog = files[0];
                            } else {
                                type = LogDataType.ZIP;
                            }
                        } catch (IOException e) {
                            CLog.e(e);
                        }
                    }
                }
                try (InputStreamSource source = new FileInputStreamSource(toLog)) {
                    CLog.d("Logging %s from subprocess: %s ", assosInfo.mDataName, toLog.getPath());
                    mListener.testLog(name, type, source);
                }
                FileUtil.recursiveDelete(extractedDir);
                FileUtil.deleteFile(path);
            } else {
                CLog.d(
                        "Logging %s from subprocess. url: %s, path: %s",
                        name, file.getUrl(), file.getPath());
                if (mListener instanceof ILogSaverListener) {
                    ((ILogSaverListener) mListener).logAssociation(name, assosInfo.mLoggedFile);
                }
            }
        }
    }

    private class InvocationStartedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            InvocationStartedEventInfo eventStart =
                    new InvocationStartedEventInfo(new JSONObject(eventJson));
            if (mContext.getTestTag() == null || "stub".equals(mContext.getTestTag())) {
                mContext.setTestTag(eventStart.mTestTag);
            }
            mStartTime = eventStart.mStartTime;
        }
    }

    private class InvocationEndedEventHandler implements EventHandler {
        @Override
        public void handleEvent(String eventJson) throws JSONException {
            JSONObject json = new JSONObject(eventJson);
            InvocationEndedEventInfo eventEnd = new InvocationEndedEventInfo(json);
            // Add build attributes to the primary build (the first build
            // provider of the running configuration).
            List<IBuildInfo> infos = mContext.getBuildInfos();
            if (!infos.isEmpty()) {
                Map<String, String> attributes = eventEnd.mBuildAttributes;
                for (InvocationMetricKey key : InvocationMetricKey.values()) {
                    if (!attributes.containsKey(key.toString())) {
                        continue;
                    }
                    String val = attributes.remove(key.toString());
                    if (key.shouldAdd()) {
                        try {
                            InvocationMetricLogger.addInvocationMetrics(key, Long.parseLong(val));
                        } catch (NumberFormatException e) {
                            CLog.d("Key %s doesn't have a number value, was: %s.", key, val);
                            // If it's not a number then, let the string concatenate
                            InvocationMetricLogger.addInvocationMetrics(key, val);
                        }
                    } else {
                        InvocationMetricLogger.addInvocationMetrics(key, val);
                    }
                }
                if (attributes.containsKey(TfObjectTracker.TF_OBJECTS_TRACKING_KEY)) {
                    String val = attributes.get(TfObjectTracker.TF_OBJECTS_TRACKING_KEY);
                    for (String pair : Splitter.on(",").split(val)) {
                        if (!pair.contains("=")) {
                            continue;
                        }
                        String[] pairSplit = pair.split("=");
                        try {
                            TfObjectTracker.directCount(pairSplit[0], Long.parseLong(pairSplit[1]));
                        } catch (NumberFormatException e) {
                            CLog.e(e);
                            continue;
                        }
                    }
                    attributes.remove(TfObjectTracker.TF_OBJECTS_TRACKING_KEY);
                }
                infos.get(0).addBuildAttributes(attributes);
            }
        }
    }

    /**
     * Returns the start time associated with the invocation start event from the subprocess
     * invocation.
     */
    public Long getStartTime() {
        return mStartTime;
    }

    /** Returns the test that is currently in progress. */
    public TestDescription getCurrentTest() {
        return mCurrentTest;
    }
}
