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

import com.android.tradefed.testtype.mobly.MoblyYamlResultHandlerFactory.Type;

import java.util.Map;

/** Mobly yaml result 'Record' element handler. */
public class MoblyYamlResultRecordHandler implements IMoblyYamlResultHandler {

    private static final String BEGIN_TIME = "Begin Time";
    private static final String END_TIME = "End Time";
    private static final String RESULT = "Result";
    private static final String STACKTRACE = "Stacktrace";
    private static final String TEST_CLASS = "Test Class";
    private static final String TEST_NAME = "Test Name";

    @Override
    public Record handle(Map<String, Object> docMap) {
        Record.Builder builder = Record.builder();
        builder.setTestClass(String.valueOf(docMap.get(TEST_CLASS)));
        builder.setTestName(String.valueOf(docMap.get(TEST_NAME)));
        builder.setResult(RecordResult.valueOf((String) docMap.get(RESULT)));
        builder.setBeginTime(String.valueOf(docMap.get(BEGIN_TIME)));
        builder.setEndTime(String.valueOf(docMap.get(END_TIME)));
        builder.setStackTrace(String.valueOf(docMap.get(STACKTRACE)));
        return builder.build();
    }

    public static class Record implements ITestResult {
        private final String mTestClass;
        private final String mTestName;
        private final RecordResult mResult;
        private final long mBeginTime;
        private final long mEndTime;
        private final String mStacktrace;

        private Record(
                String testClass,
                String testName,
                RecordResult result,
                String beginTime,
                String endTime,
                String stacktrace) {
            mTestClass = testClass;
            mTestName = testName;
            mResult = result;
            mBeginTime = Long.parseLong(beginTime);
            mEndTime = Long.parseLong(endTime);
            mStacktrace = stacktrace;
        }

        @Override
        public Type getType() {
            return Type.RECORD;
        }

        public long getBeginTime() {
            return mBeginTime;
        }

        public String getTestClass() {
            return mTestClass;
        }

        public String getTestName() {
            return mTestName;
        }

        public String getStackTrace() {
            return mStacktrace;
        }

        public long getEndTime() {
            return mEndTime;
        }

        public RecordResult getResult() {
            return mResult;
        }

        public static Builder builder() {
            return new Builder();
        }

        public static class Builder {
            private String mTestClass;
            private String mTestName;
            private RecordResult mResult;
            private String mBeginTime;
            private String mEndTime;
            private String mStacktrace;

            public Builder() {}

            public Builder setTestClass(String testClass) {
                mTestClass = testClass;
                return this;
            }

            public Builder setTestName(String testName) {
                mTestName = testName;
                return this;
            }

            public Builder setResult(RecordResult result) {
                mResult = result;
                return this;
            }

            public Builder setBeginTime(String beginTime) {
                mBeginTime = beginTime;
                return this;
            }

            public Builder setEndTime(String endTime) {
                mEndTime = endTime;
                return this;
            }

            public Builder setStackTrace(String stacktrace) {
                mStacktrace = stacktrace;
                return this;
            }

            public Record build() {
                return new Record(
                        mTestClass, mTestName, mResult, mBeginTime, mEndTime, mStacktrace);
            }
        }
    }

    public enum RecordResult {
        PASS,
        FAIL,
        ERROR
    }
}
