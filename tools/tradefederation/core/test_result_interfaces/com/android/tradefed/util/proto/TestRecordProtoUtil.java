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

import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.util.ByteArrayList;

import com.google.protobuf.CodedInputStream;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/** Utility to read the {@link TestRecord} proto from a file. */
public class TestRecordProtoUtil {

    /**
     * Pick a 4MB default size to allow the buffer to grow for big protobuf. The default value could
     * fail in some cases.
     */
    private static final int DEFAULT_SIZE_BYTES = 4 * 1024 * 1024;

    /**
     * Read {@link TestRecord} from a file and return it.
     *
     * @param protoFile The {@link File} containing the record
     * @return a {@link TestRecord} created from the file.
     * @throws IOException, InvalidProtocolBufferException
     */
    public static TestRecord readFromFile(File protoFile) throws IOException {
        return readFromFile(protoFile, true);
    }

    /**
     * Read {@link TestRecord} from a file and return it.
     *
     * @param protoFile The {@link File} containing the record
     * @param readDelimited True if the proto file in delimited format. False if the file is default
     *     format.
     * @throws IOException, InvalidProtocolBufferException
     */
    public static TestRecord readFromFile(File protoFile, boolean readDelimited)
            throws IOException {
        TestRecord record = null;
        try (InputStream stream = new FileInputStream(protoFile)) {
            CodedInputStream is = CodedInputStream.newInstance(stream);
            if (readDelimited) {
                is.setSizeLimit(Integer.MAX_VALUE);
                ByteArrayList data = new ByteArrayList(DEFAULT_SIZE_BYTES);
                while (!is.isAtEnd()) {
                    int size = is.readRawVarint32();
                    byte[] dataByte = is.readRawBytes(size);
                    data.addAll(dataByte);
                }
                record = TestRecord.parseFrom(data.getContents());
                data.clear();
            } else {
                record = TestRecord.parseFrom(is);
            }
        }
        return record;
    }
}
