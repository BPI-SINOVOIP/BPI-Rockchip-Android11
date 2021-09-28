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
package com.android.tradefed.result.proto;

import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.util.StreamUtil;

import java.io.IOException;
import java.net.Socket;

/** An implementation of {@link ProtoResultReporter} */
public final class StreamProtoResultReporter extends ProtoResultReporter {

    public static final String PROTO_REPORT_PORT_OPTION = "proto-report-port";

    @Option(
        name = PROTO_REPORT_PORT_OPTION,
        description = "the port where to connect to send the protos."
    )
    private Integer mReportPort = null;

    private Socket mReportSocket = null;
    private boolean mPrintedMessage = false;

    public void setProtoReportPort(Integer portValue) {
        mReportPort = portValue;
    }

    public Integer getProtoReportPort() {
        return mReportPort;
    }

    @Override
    public void processStartInvocation(
            TestRecord invocationStartRecord, IInvocationContext context) {
        writeRecordToSocket(invocationStartRecord);
    }

    @Override
    public void processTestModuleStarted(TestRecord moduleStartRecord) {
        writeRecordToSocket(moduleStartRecord);
    }

    @Override
    public void processTestModuleEnd(TestRecord moduleRecord) {
        writeRecordToSocket(moduleRecord);
    }

    @Override
    public void processTestRunStarted(TestRecord runStartedRecord) {
        writeRecordToSocket(runStartedRecord);
    }

    @Override
    public void processTestRunEnded(TestRecord runRecord, boolean moduleInProgress) {
        writeRecordToSocket(runRecord);
    }

    @Override
    public void processTestCaseStarted(TestRecord testCaseStartedRecord) {
        writeRecordToSocket(testCaseStartedRecord);
    }

    @Override
    public void processTestCaseEnded(TestRecord testCaseRecord) {
        writeRecordToSocket(testCaseRecord);
    }

    @Override
    public void processFinalProto(TestRecord finalRecord) {
        writeRecordToSocket(finalRecord);
        StreamUtil.close(mReportSocket);
    }

    private void writeRecordToSocket(TestRecord record) {
        if (mReportPort == null) {
            if (!mPrintedMessage) {
                CLog.d("No port set. Skipping the reporter.");
                mPrintedMessage = true;
            }
            return;
        }
        try {
            if (mReportSocket == null) {
                mReportSocket = new Socket("localhost", mReportPort);
            }
            record.writeDelimitedTo(mReportSocket.getOutputStream());
        } catch (IOException e) {
            CLog.e(e);
        }
    }
}
