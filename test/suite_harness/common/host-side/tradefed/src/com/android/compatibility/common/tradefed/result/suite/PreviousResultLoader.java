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
package com.android.compatibility.common.tradefed.result.suite;

import com.android.annotations.VisibleForTesting;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildProvider;
import com.android.compatibility.common.tradefed.targetprep.BuildFingerPrintPreparer;
import com.android.compatibility.common.tradefed.testtype.retry.RetryFactoryTest;
import com.android.compatibility.common.util.ResultHandler;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInvocation;
import com.android.tradefed.invoker.proto.InvocationContext.Context;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.proto.ProtoResultParser;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.result.suite.SuiteResultHolder;
import com.android.tradefed.result.suite.XmlSuiteResultFormatter.RunHistory;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.suite.retry.ITestSuiteResultLoader;
import com.android.tradefed.util.TestRecordInterpreter;
import com.android.tradefed.util.proto.TestRecordProtoUtil;

import com.google.api.client.util.Strings;
import com.google.gson.Gson;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

/**
 * Implementation of {@link ITestSuiteResultLoader} to reload CTS previous results.
 */
public final class PreviousResultLoader implements ITestSuiteResultLoader {

    /** Usually associated with ro.build.fingerprint. */
    public static final String BUILD_FINGERPRINT = "build_fingerprint";
    /** Usally associated with ro.vendor.build.fingerprint. */
    public static final String BUILD_VENDOR_FINGERPRINT = "build_vendor_fingerprint";
    /**
     * Some suites have a business need to alter the original real device fingerprint value, in this
     * case we expect an "unaltered" version to be available to still do the original check.
     */
    public static final String BUILD_FINGERPRINT_UNALTERED = "build_fingerprint_unaltered";
    /** Used to get run history from the invocation context of last run. */
    public static final String RUN_HISTORY_KEY = "run_history";

    private static final String COMMAND_LINE_ARGS = "command_line_args";

    @Option(name = RetryFactoryTest.RETRY_OPTION,
            shortName = 'r',
            description = "retry a previous session's failed and not executed tests.",
            mandatory = true)
    private Integer mRetrySessionId = null;

    @Option(
        name = "fingerprint-property",
        description = "The property name to check for the fingerprint."
    )
    private String mFingerprintProperty = "ro.build.fingerprint";

    private TestRecord mTestRecord;
    private String mProtoPath = null;
    private IInvocationContext mPreviousContext;
    private String mExpectedFingerprint;
    private String mExpectedVendorFingerprint;
    private String mUnalteredFingerprint;

    private File mResultDir;

    private IBuildProvider mProvider;

    /**
     * The run history of last run (last run excluded) will be first parsed out and stored here, the
     * information of last run is added second. Then this object is serialized and added to the
     * configuration of the current test run.
     */
    private Collection<RunHistory> mRunHistories;

    @Override
    public void init() {
        IBuildInfo info = null;
        try {
            info = getProvider().getBuild();
        } catch (BuildRetrievalError e) {
            throw new RuntimeException(e);
        }
        CompatibilityBuildHelper helperBuild = new CompatibilityBuildHelper(info);
        mResultDir = null;
        try {
            CLog.logAndDisplay(LogLevel.DEBUG, "Start loading the record protobuf.");
            mResultDir =
                    ResultHandler.getResultDirectory(helperBuild.getResultsDir(), mRetrySessionId);
            File protoDir = new File(mResultDir, CompatibilityProtoResultReporter.PROTO_DIR);
            // Check whether we have multiple protos or one
            if (new File(protoDir, CompatibilityProtoResultReporter.PROTO_FILE_NAME).exists()) {
                mTestRecord =
                        TestRecordProtoUtil.readFromFile(
                                new File(
                                        protoDir,
                                        CompatibilityProtoResultReporter.PROTO_FILE_NAME));
            } else if (new File(protoDir, CompatibilityProtoResultReporter.PROTO_FILE_NAME + "0")
                    .exists()) {
                // Use proto0 to get the basic information since it should be the invocation proto.
                mTestRecord =
                        TestRecordProtoUtil.readFromFile(
                                new File(
                                        protoDir,
                                        CompatibilityProtoResultReporter.PROTO_FILE_NAME + "0"));
                mProtoPath =
                        new File(protoDir, CompatibilityProtoResultReporter.PROTO_FILE_NAME)
                                .getAbsolutePath();
            } else {
                throw new RuntimeException("Could not find any test-record.pb to load.");
            }

            CLog.logAndDisplay(LogLevel.DEBUG, "Done loading the record protobuf.");
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        Context contextProto = null;
        try {
            contextProto = mTestRecord.getDescription().unpack(Context.class);
        } catch (InvalidProtocolBufferException e) {
            throw new RuntimeException(e);
        }
        mPreviousContext = InvocationContext.fromProto(contextProto);

        mRunHistories = new ArrayList<>();
        String runHistoryJSON =
                mPreviousContext.getAttributes().getUniqueMap().get(RUN_HISTORY_KEY);
        if (runHistoryJSON != null) {
            Gson gson = new Gson();
            RunHistory[] runHistories = gson.fromJson(runHistoryJSON, RunHistory[].class);
            Collections.addAll(mRunHistories, runHistories);
        }

        // Validate the fingerprint
        // TODO: Use fingerprint argument from TestRecord but we have to deal with suite namespace
        // for example: cts:build_fingerprint instead of just build_fingerprint.
        // And update run history.
        try {
            CLog.logAndDisplay(LogLevel.DEBUG, "Start parsing previous test_results.xml");
            CertificationResultXml xmlParser = new CertificationResultXml();
            SuiteResultHolder holder = xmlParser.parseResults(mResultDir, true);
            CLog.logAndDisplay(LogLevel.DEBUG, "Done parsing previous test_results.xml");
            mExpectedFingerprint = holder.context.getAttributes()
                    .getUniqueMap().get(BUILD_FINGERPRINT);
            if (mExpectedFingerprint == null) {
                throw new IllegalArgumentException(
                        String.format(
                                "Could not find the %s field in the loaded result.",
                                BUILD_FINGERPRINT));
            }
            /** If available in the report, collect the vendor fingerprint too. */
            mExpectedVendorFingerprint =
                    holder.context.getAttributes().getUniqueMap().get(BUILD_VENDOR_FINGERPRINT);
            if (mExpectedVendorFingerprint == null) {
                throw new IllegalArgumentException(
                        String.format(
                                "Could not find the %s field in the loaded result.",
                                BUILD_VENDOR_FINGERPRINT));
            }
            // Some cases will have an unaltered fingerprint
            mUnalteredFingerprint =
                    holder.context.getAttributes().getUniqueMap().get(BUILD_FINGERPRINT_UNALTERED);

            // Add the information of last test run to a run history list.
            RunHistory newRun = new RunHistory();
            newRun.startTime = holder.startTime;
            newRun.endTime = holder.endTime;
            newRun.passedTests = holder.passedTests;
            newRun.failedTests = holder.failedTests;
            newRun.commandLineArgs =
                    com.google.common.base.Strings.nullToEmpty(
                            holder.context.getAttributes().getUniqueMap().get(COMMAND_LINE_ARGS));
            newRun.hostName = holder.hostName;
            mRunHistories.add(newRun);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public String getCommandLine() {
        List<String> command = mPreviousContext.getAttributes().get(
                TestInvocation.COMMAND_ARGS_KEY);
        if (command == null) {
            throw new RuntimeException("Couldn't find the command_line_args.");
        }
        return command.get(0);
    }

    @Override
    public CollectingTestListener loadPreviousResults() {
        if (mProtoPath != null) {
            int index = 0;
            CollectingTestListener results = new CollectingTestListener();
            ProtoResultParser parser = new ProtoResultParser(results, null, true);
            while (new File(mProtoPath + index).exists()) {
                try {
                    parser.processFileProto(new File(mProtoPath + index));
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
                index++;
            }
            return results;
        }
        return TestRecordInterpreter.interpreteRecord(mTestRecord);
    }

    @Override
    public final void cleanUp() {
        if (mTestRecord != null) {
            mTestRecord = null;
        }
    }

    @Override
    public final void customizeConfiguration(IConfiguration config) {
        // This is specific to Compatibility checking and does not work for multi-device.
        List<ITargetPreparer> preparers = config.getTargetPreparers();
        List<ITargetPreparer> newList = new ArrayList<>();
        // Add the fingerprint checker first to ensure we check it before rerunning the config.
        BuildFingerPrintPreparer fingerprintChecker = new BuildFingerPrintPreparer();
        fingerprintChecker.setExpectedFingerprint(mExpectedFingerprint);
        fingerprintChecker.setExpectedVendorFingerprint(mExpectedVendorFingerprint);
        fingerprintChecker.setFingerprintProperty(mFingerprintProperty);
        if (!Strings.isNullOrEmpty(mUnalteredFingerprint)) {
            fingerprintChecker.setUnalteredFingerprint(mUnalteredFingerprint);
        }
        newList.add(fingerprintChecker);
        newList.addAll(preparers);
        config.setTargetPreparers(newList);

        // Add the file copier last to copy from previous sesssion
        List<ITestInvocationListener> listeners = config.getTestInvocationListeners();
        PreviousSessionFileCopier copier = new PreviousSessionFileCopier();
        copier.setPreviousSessionDir(mResultDir);
        listeners.add(copier);

        // Add run history to the configuration so it will be augmented in the next test run.
        Gson gson = new Gson();
        config.getCommandOptions()
                .getInvocationData()
                .put(RUN_HISTORY_KEY, gson.toJson(mRunHistories));
    }

    @VisibleForTesting
    protected void setProvider(IBuildProvider provider) {
        mProvider = provider;
    }

    private IBuildProvider getProvider() {
        if (mProvider == null) {
            mProvider = new CompatibilityBuildProvider();
        }
        return mProvider;
    }
}
