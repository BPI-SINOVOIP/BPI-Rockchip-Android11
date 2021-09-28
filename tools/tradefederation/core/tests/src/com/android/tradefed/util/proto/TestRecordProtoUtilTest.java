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
package com.android.tradefed.util.proto;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.result.proto.TestRecordProto.TestStatus;
import com.android.tradefed.util.FileUtil;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/** Unit tests for {@link TestRecordProtoUtil}. */
@RunWith(JUnit4.class)
public class TestRecordProtoUtilTest {

    @Test
    public void testRead() throws IOException {
        File protoFile = null;
        try {
            protoFile = dumpTestRecord();
            TestRecord record = TestRecordProtoUtil.readFromFile(protoFile);
            assertEquals(5, record.getAttemptId());
            assertEquals(TestStatus.ASSUMPTION_FAILURE, record.getStatus());
        } finally {
            FileUtil.deleteFile(protoFile);
        }
    }

    private File dumpTestRecord() throws IOException {
        TestRecord.Builder builder = TestRecord.newBuilder();
        builder.setAttemptId(5);
        builder.setStatus(TestStatus.ASSUMPTION_FAILURE);
        File protoFile = FileUtil.createTempFile("test-proto", "." + LogDataType.PB.getFileExt());
        TestRecord record = builder.build();
        try (OutputStream stream = new FileOutputStream(protoFile)) {
            record.writeDelimitedTo(stream);
            return protoFile;
        }
    }
}
