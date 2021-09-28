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

package com.android.nn.benchmark.util;

import android.util.Log;

import com.android.nn.benchmark.core.BenchmarkResult;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;

/**
 * Serialize benchmark results into CSV file for further processing.
 */
public class CSVWriter implements AutoCloseable {
    private static final String TAG = CSVWriter.class.getSimpleName();
    private final BufferedWriter writer;

    public CSVWriter(File csvFile) throws IOException {
        writer = new BufferedWriter(new FileWriter(csvFile, true));
    }

    static final String RESULT_FORMAT_COMMENT = "#testInfo,backendType" +
            ",iterations,totalTimeSec,maxSingleError,testSetSize,evaluatorsCount" +
            ",timeFreqStartSec,timeFreqStepSec,evaluatorKey1,evaluatorKey1,..." +
            ",timeFreqBucket1,...";

    String deviceInfoCsvLine() {
        SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMdd_HHmmss");
        StringBuilder sb = new StringBuilder();
        sb.append(sdf.format(new Date())).append(',');
        sb.append(android.os.Build.DISPLAY).append('\n');
        return sb.toString();
    }

    public void write(BenchmarkResult benchmarkResult) throws IOException {
        writer.write(benchmarkResult.toCsvLine());
    }

    public void writeHeader() throws IOException {
        writer.write(deviceInfoCsvLine());
        writer.write(RESULT_FORMAT_COMMENT);
        writer.write('\n');
    }

    @Override
    public void close() {
        try {
            writer.close();
        } catch (IOException e) {
            Log.e(TAG, "Failure to close writer", e);
        }
    }
}
