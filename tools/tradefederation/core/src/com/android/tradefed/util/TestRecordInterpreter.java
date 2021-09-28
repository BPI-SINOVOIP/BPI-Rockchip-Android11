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
package com.android.tradefed.util;

import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.proto.ProtoResultParser;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;

/**
 * Utility to convert a {@link TestRecord} proto into a more easily manipulable format in Tradefed.
 */
public class TestRecordInterpreter {

    /**
     * Convert the {@link TestRecord} to a {@link CollectingTestListener}.
     *
     * @param record A full {@link TestRecord}.
     * @return A populated {@link CollectingTestListener} with the results.
     */
    public static CollectingTestListener interpreteRecord(TestRecord record) {
        CollectingTestListener listener = new CollectingTestListener();
        ProtoResultParser parser = new ProtoResultParser(listener, null, true);
        parser.processFinalizedProto(record);
        return listener;
    }
}
