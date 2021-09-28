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

package com.android.tradefed.testtype.mobly;

import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.proto.TestRecordProto;
import com.android.tradefed.testtype.mobly.IMoblyYamlResultHandler.ITestResult;
import com.android.tradefed.testtype.mobly.MoblyYamlResultHandlerFactory.InvalidResultTypeException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

import com.android.tradefed.testtype.mobly.MoblyYamlResultSummaryHandler.Summary;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;

import java.io.InputStream;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.SafeConstructor;

/** Mobly yaml test results parser. */
public class MoblyYamlResultParser {
    private static final String TYPE = "Type";
    private ImmutableList.Builder<ITestInvocationListener> mListenersBuilder =
            new ImmutableList.Builder();
    private final String mRunName;
    private ImmutableList.Builder<ITestResult> mResultCacheBuilder = new ImmutableList.Builder();
    private int mTestCount;
    private long mRunStartTime;
    private long mRunEndTime;

    public MoblyYamlResultParser(ITestInvocationListener listener, String runName) {
        mListenersBuilder.add(listener);
        mRunName = runName;
    }

    public void parse(InputStream inputStream)
            throws InvalidResultTypeException, IllegalAccessException, InstantiationException {
        Yaml yaml = new Yaml(new SafeConstructor());
        for (Object doc : yaml.loadAll(inputStream)) {
            Map<String, Object> docMap = (Map<String, Object>) doc;
            mResultCacheBuilder.add(parseDocumentMap(docMap));
        }
        reportToListeners(mListenersBuilder.build(), mResultCacheBuilder.build());
    }

    @VisibleForTesting
    protected ITestResult parseDocumentMap(Map<String, Object> docMap)
            throws InvalidResultTypeException, IllegalAccessException, InstantiationException {
        LogUtil.CLog.i("Parsed object: %s", docMap.toString());
        String docType = String.valueOf(docMap.get(TYPE));
        LogUtil.CLog.d("Parsing result type: %s", docType);
        IMoblyYamlResultHandler resultHandler =
                new MoblyYamlResultHandlerFactory().getHandler(docType);
        ITestResult testResult = resultHandler.handle(docMap);
        if ("Summary".equals(docType)) {
            mTestCount = ((Summary) testResult).getExecuted();
        }
        return testResult;
    }

    @VisibleForTesting
    protected void reportToListeners(
            List<ITestInvocationListener> listeners,
            List<IMoblyYamlResultHandler.ITestResult> resultCache) {
        for (ITestInvocationListener listener : listeners) {
            listener.testRunStarted(mRunName, mTestCount);
        }
        for (IMoblyYamlResultHandler.ITestResult result : resultCache) {
            switch (result.getType()) {
                case RECORD:
                    MoblyYamlResultRecordHandler.Record record =
                            (MoblyYamlResultRecordHandler.Record) result;
                    TestDescription testDescription =
                            new TestDescription(record.getTestClass(), record.getTestName());
                    FailureDescription failureDescription =
                            FailureDescription.create(
                                    record.getStackTrace(),
                                    TestRecordProto.FailureStatus.TEST_FAILURE);
                    mRunStartTime =
                            mRunStartTime == 0L
                                    ? record.getBeginTime()
                                    : Math.min(mRunStartTime, record.getBeginTime());
                    mRunEndTime = Math.max(mRunEndTime, record.getEndTime());
                    for (ITestInvocationListener listener : listeners) {
                        listener.testStarted(testDescription, record.getBeginTime());
                        if (record.getResult() != MoblyYamlResultRecordHandler.RecordResult.PASS) {
                            listener.testFailed(testDescription, failureDescription);
                        }
                        listener.testEnded(
                                testDescription,
                                record.getEndTime(),
                                new HashMap<String, String>());
                    }
                    break;
                case USER_DATA:
                    long timestamp =
                            ((MoblyYamlResultUserDataHandler.UserData) result).getTimeStamp();
                    mRunStartTime =
                            mRunStartTime == 0L ? timestamp : Math.min(mRunStartTime, timestamp);
                    break;
                case CONTROLLER_INFO:
                    mRunEndTime =
                            Math.max(
                                    mRunEndTime,
                                    ((MoblyYamlResultControllerInfoHandler.ControllerInfo) result)
                                            .getTimeStamp());
                    break;
                case TEST_NAME_LIST:
                    // Do nothing
                    break;
                default:
                    // Do nothing
            }
        }
        for (ITestInvocationListener listener : listeners) {
            listener.testRunEnded(mRunEndTime - mRunStartTime, new HashMap<String, String>());
        }
    }
}
