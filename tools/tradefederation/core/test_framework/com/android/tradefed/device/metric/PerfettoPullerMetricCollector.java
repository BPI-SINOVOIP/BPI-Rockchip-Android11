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
package com.android.tradefed.device.metric;

import com.android.annotations.Nullable;
import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LargeOutputReceiver;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.Pair;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.ZipUtil;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Base implementation of {@link FilePullerDeviceMetricCollector} that allows
 * pulling the perfetto files from the device and collect the metrics from it.
 * Also used for converting the raw trace file into perfetto metric file.
 */
@OptionClass(alias = "perfetto-metric-collector")
public class PerfettoPullerMetricCollector extends FilePullerDeviceMetricCollector {

    private static final String LINE_SEPARATOR = "\\r?\\n";
    private static final char KEY_VALUE_SEPARATOR = ':';
    private static final String EXTRACTOR_STATUS = "trace_extractor_status";
    private static final String EXTRACTOR_SUCCESS = "1";
    private static final String EXTRACTOR_FAILURE = "0";
    private static final String EXTRACTOR_RUNTIME = "trace_extractor_runtime";
    private static final String RAW_TRACE_FILE_SIZE = "perfetto_trace_file_size_bytes";
    private static final String NSS_CACHE_ERROR = "base/nsscache-inl.h failed to lookup";

    public enum METRIC_FILE_FORMAT {
        text,
        binary,
        json,
    }

    @Option(name = "compress-perfetto",
            description = "If enabled retrieves the perfetto compressed content,"
                    + "decompress for processing and upload the compressed file. If"
                    + "this flag is not enabled uncompressed version of perfetto file is"
                    + "pulled, processed and uploaded.")
    private boolean mCompressPerfetto = false;

    @Option(name = "max-compressed-file-size", description = "Max size of the compressed"
            + " perfetto file. If the compressed file size exceeds the max then"
            + " post processing and uploading the compressed file will not happen.")
    private long mMaxCompressedFileSize = 10000L * 1024 * 1024;

    @Option(
            name = "compressed-trace-shell-timeout",
            description = "Timeout for retrieving compressed trace content through shell",
            isTimeVal = true)
    private long mCompressedTimeoutMs = TimeUnit.MINUTES.toMillis(20);

    @Option(
            name = "compress-response-timeout",
            description = "Timeout to receive the shell response when running the gzip command.",
            isTimeVal = true)
    private long mCompressResponseTimeoutMs = TimeUnit.SECONDS.toMillis(30);

    @Option(
            name = "decompress-perfetto-timeout",
            description = "Timeout to decompress perfetto compressed file.",
            isTimeVal = true)
    private long mDecompressTimeoutMs = TimeUnit.MINUTES.toMillis(20);

    @Option(
            name = "perfetto-binary-path",
            description = "Path to the script files used to analyze the trace files."
                    + "Used for collecting the key value metrics from the perfetto"
                    + "trace file.")
    @Deprecated
    private List<File> mScriptFiles = new ArrayList<>();

    @Option(
            name = "perfetto-binary-args",
            description = "Extra arguments to be passed to the binaries.")
    @Deprecated
    private List<String> mPerfettoBinaryArgs = new ArrayList<>();

    @Option(
            name = "perfetto-metric-prefix",
            description = "Prefix to be used with the metrics collected from perfetto.")
    @Deprecated
    private String mMetricPrefix = "perfetto";

    // List of process names passed to perfetto binary.
    @Option(
            name = "process-name",
            description =
            "Process names to be passed in perfetto script.")
    @Deprecated
    private Collection<String> mProcessNames = new ArrayList<String>();

    // Timeout for the script to process the trace files.
    // The default is arbitarily chosen to be 5 mins to prevent the test spending more time in
    // processing the files.
    @Option(
            name = "perfetto-script-timeout",
            description = "Timeout for the perfetto script.",
            isTimeVal = true)
    @Deprecated
    private long mScriptTimeoutMs = TimeUnit.MINUTES.toMillis(5);

    @Option(name = "convert-metric-file",
            description = "Convert the raw trace file to perfetto metric file.")
    private boolean mConvertToMetricFile = true;

    @Option(
            name = "trace-processor-binary",
            description = "Path to the trace processor shell. This will"
                    + " override the trace-processor-name which is used to "
                    + " lookup in build artifacts. Used for converting the raw"
                    + " trace into perfetto metric file.")
    private File mTraceProcessorBinary = null;

    @Option(
            name = "trace-processor-name",
            description = "Trace processor name to look up from the test artifacts"
                    + " or module artifacts.")
    private String mTraceProcessorName = "trace_processor_shell";

    @Option(
            name = "trace-processor-run-metrics",
            description = "Comma separated list of metrics to extract from raw trace file."
                    + " For example android_mem.")
    private String mTraceProcessorMetrics = "android_mem";

    @Option(
            name = "trace-processor-output-format",
            description = "Trace processor output format. [binary|text|json]")
    private METRIC_FILE_FORMAT mTraceProcessorOutputFormat = METRIC_FILE_FORMAT.text;

    @Option(
            name = "trace-processor-timeout",
            description = "Timeout to convert the raw trace file to metric proto file.",
            isTimeVal = true)
    private long mTraceConversionTimeout = TimeUnit.MINUTES.toMillis(20);


    /**
     * Process the perfetto trace file for the additional metrics and add it to final metrics.
     * Decompress the perfetto file for processing if the compression was enabled.
     *
     * @param key the option key associated to the file that was pulled from the device.
     * @param metricFile the {@link File} pulled from the device matching the option key.
     * @param data where metrics will be stored.
     */
    @Override
    public void processMetricFile(String key, File metricFile,
            DeviceMetricData data) {
        File processSrcFile = metricFile;
        if (mCompressPerfetto) {
            processSrcFile = decompressFile(metricFile);
        }

        // Update the file size metrics.
        if (processSrcFile != null) {
            double perfettoFileSizeInBytes = processSrcFile.length();
            Metric.Builder metricDurationBuilder = Metric.newBuilder();
            metricDurationBuilder.getMeasurementsBuilder().setSingleDouble(
                    perfettoFileSizeInBytes);
            data.addMetric(RAW_TRACE_FILE_SIZE, metricDurationBuilder.setType(DataType.RAW));
        }

        // Convert to perfetto metric format.
        if (mConvertToMetricFile) {
            File convertedMetricFile = convertToMetricProto(processSrcFile);
            if (convertedMetricFile != null) {
                try (InputStreamSource source = new FileInputStreamSource(convertedMetricFile,
                        true)) {
                    testLog(convertedMetricFile.getName(), getLogDataType(), source);
                }
            }
        }

        if (processSrcFile != null) {
            // Extract the metrics from the trace file.
            for (File scriptFile : mScriptFiles) {
                // Apply necessary execute permissions to the script.
                FileUtil.chmodGroupRWX(scriptFile);

                List<String> commandArgsList = new ArrayList<String>();
                commandArgsList.add(scriptFile.getAbsolutePath());
                commandArgsList.addAll(mPerfettoBinaryArgs);
                commandArgsList.add("-trace_file");
                commandArgsList.add(processSrcFile.getAbsolutePath());

                if (!mProcessNames.isEmpty()) {
                    commandArgsList.add("-process_names");
                    commandArgsList.add(String.join(",", mProcessNames));
                }

                String traceExtractorStatus = EXTRACTOR_SUCCESS;

                double scriptDuration = 0;
                double scriptStartTime = System.currentTimeMillis();
                CommandResult cr = runHostCommand(mScriptTimeoutMs,
                        commandArgsList.toArray(new String[commandArgsList.size()]), null,
                        null);
                scriptDuration = System.currentTimeMillis() - scriptStartTime;

                // Update the script duration metrics.
                Metric.Builder metricDurationBuilder = Metric.newBuilder();
                metricDurationBuilder.getMeasurementsBuilder().setSingleDouble(scriptDuration);
                data.addMetric(
                        String.format("%s_%s", mMetricPrefix, EXTRACTOR_RUNTIME),
                        metricDurationBuilder.setType(DataType.RAW));

                // Adding temporary workaround to handle the NSS cache error.
                // TODO: Revert the NSS cache error handling after b/156924255 is fixed.
                if (CommandStatus.SUCCESS.equals(cr.getStatus()) ||
                        (CommandStatus.FAILED.equals(cr.getStatus()) &&
                                cr.getStdout().contains(NSS_CACHE_ERROR))) {
                    String[] metrics = cr.getStdout().split(LINE_SEPARATOR);

                    boolean isMetricStarted = false;
                    for (String metric : metrics) {
                        // Skip until the first metric line is parsed.
                        // Usually "trace-durations-ms" is the first metric from the output.
                        if(isMetricStarted || metric.contains("trace-duration-ms")) {
                            isMetricStarted = true;
                        } else {
                            continue;
                        }

                        Pair<String, String> kv = splitKeyValue(metric);

                        if (kv != null) {
                            Metric.Builder metricBuilder = Metric.newBuilder();
                            metricBuilder.getMeasurementsBuilder().setSingleString(kv.second);
                            data.addMetric(
                                    String.format("%s_%s", mMetricPrefix, kv.first),
                                    metricBuilder.setType(DataType.RAW));
                        } else {
                            CLog.e("Output %s not in the expected format.", metric);
                        }
                    }
                    CLog.i(cr.getStdout());
                } else {
                    traceExtractorStatus = EXTRACTOR_FAILURE;
                    CLog.e("Unable to parse the trace file %s due to %s - Status - %s ",
                            processSrcFile.getName(), cr.getStderr(), cr.getStatus());
                }

                if (mCompressPerfetto) {
                    processSrcFile.delete();
                }
                Metric.Builder metricStatusBuilder = Metric.newBuilder();
                metricStatusBuilder.getMeasurementsBuilder()
                        .setSingleString(traceExtractorStatus);
                data.addMetric(
                        String.format("%s_%s", mMetricPrefix, EXTRACTOR_STATUS),
                        metricStatusBuilder.setType(DataType.RAW));
            }
        }

        // Upload and delete the host trace file.
        try (InputStreamSource source = new FileInputStreamSource(metricFile, true)) {
            if (mCompressPerfetto) {
                if (processSrcFile != null) {
                    testLog(metricFile.getName(), LogDataType.GZIP, source);
                } else {
                    metricFile.delete();
                }

            } else {
                testLog(metricFile.getName(), LogDataType.PERFETTO, source);
            }
        }

    }

    /**
     * Converts the raw trace file into perfetto metric file.
     *
     * @param perfettoRawTraceFile Raw perfetto trace file that needs to be
     *        converted.
     * @return File converted perfetto metric file.
     */
    private File convertToMetricProto(File perfettoRawTraceFile) {

        // Use absolute path to the trace file if it is available otherwise
        // resolve the trace processor name from the test or module artifacts.
        if (mTraceProcessorBinary == null) {
            mTraceProcessorBinary = getFileFromTestArtifacts(mTraceProcessorName);
        }

        File metricOutputFile = null;
        if (mTraceProcessorBinary == null) {
            CLog.e("Failed to locate the trace processor shell binary file.");
            return metricOutputFile;
        }

        FileUtil.chmodGroupRWX(mTraceProcessorBinary);
        List<String> commandArgsList = new ArrayList<String>();
        commandArgsList.add(mTraceProcessorBinary.getAbsolutePath());

        // Comma separated list of metrics to extract.
        if (!mTraceProcessorMetrics.isEmpty()) {
            commandArgsList.add("--run-metrics");
            commandArgsList.add(mTraceProcessorMetrics);
        }
        // Metric file output format.
        commandArgsList.add("--metrics-output=" + mTraceProcessorOutputFormat);
        commandArgsList.add(perfettoRawTraceFile.getAbsolutePath());

        try {
            metricOutputFile = FileUtil.createTempFile(
                    "metric_" + getRawTraceFileName(perfettoRawTraceFile.getName()), "");
        } catch (IOException e) {
            CLog.e("Not able to create metric perfetto output file.");
            CLog.e(e);
            return null;
        }

        // Running the trace conversion.
        CLog.i("Run the trace convertor.");
        boolean isConversionSuccess = true;
        try (FileOutputStream outStream = new FileOutputStream(metricOutputFile);
                ByteArrayOutputStream errStream = new ByteArrayOutputStream()) {
            CommandResult conversionResult = runHostCommand(mTraceConversionTimeout,
                    commandArgsList.toArray(new String[commandArgsList
                            .size()]),
                    outStream, errStream);
            if (!CommandStatus.SUCCESS.equals(conversionResult.getStatus())) {
                CLog.e("Unable to convert the raw trace - %s to metric file due to"
                        + " %s - Status - %s ", perfettoRawTraceFile.getName(),
                        errStream.toString(), conversionResult.getStatus());
                isConversionSuccess = false;
            } else if (mTraceProcessorOutputFormat.equals(METRIC_FILE_FORMAT.text)) {
                CLog.i("Compressing the perfetto metric text proto.");
                File compressedFile = getCompressedFile(metricOutputFile);
                metricOutputFile.delete();
                return compressedFile;
            }
        } catch (FileNotFoundException e) {
            CLog.e("Not able to find the result metric file to write the "
                    + "metric output.");
            CLog.e(e);
            isConversionSuccess = false;
        } catch (IOException e1) {
            CLog.e("Unable to close the streams.");
            CLog.e(e1);
            isConversionSuccess = false;
        } finally {
            if (!isConversionSuccess) {
                metricOutputFile.delete();
                return null;
            }
        }
        return metricOutputFile;
    }


    /**
     * Pull the file from the specified path in the device. Pull the compressed content of the
     * perfetto file if the compress perfetto option is enabled.
     *
     * @param device which has the file.
     * @param remoteFilePath location in the device.
     * @return compressed or decompressed version of perfetto file based on mCompressPerfetto
     *         option is set or not.
     * @throws DeviceNotAvailableException
     */
    @Override
    protected File retrieveFile(ITestDevice device, String remoteFilePath)
            throws DeviceNotAvailableException {
        if (!mCompressPerfetto) {
            return super.retrieveFile(device, remoteFilePath);
        }
        File perfettoCompressedFile = null;
        try {
            String filePathInDevice = remoteFilePath;
            CLog.i("Retrieving the compressed perfetto trace content from device.");
            LargeOutputReceiver compressedOutputReceiver = new LargeOutputReceiver(
                    "perfetto_compressed_temp",
                    device.getSerialNumber(), mMaxCompressedFileSize);
            device.executeShellCommand(
                    String.format("gzip -c %s", filePathInDevice),
                    compressedOutputReceiver,
                    mCompressedTimeoutMs, mCompressResponseTimeoutMs, TimeUnit.MILLISECONDS, 1);
            compressedOutputReceiver.flush();
            compressedOutputReceiver.cancel();

            // Copy to temp file which will be used for decompression, perfetto
            // metrics extraction and uploading the file later.
            try (InputStreamSource largeStreamSrc = compressedOutputReceiver.getData();
                    InputStream inputStream = largeStreamSrc.createInputStream()) {
                perfettoCompressedFile = FileUtil.createTempFile(
                        "perfetto_compressed", ".gz");
                FileOutputStream outStream = new FileOutputStream(
                        perfettoCompressedFile);
                byte[] buffer = new byte[4096];
                int bytesRead = -1;
                while ((bytesRead = inputStream.read(buffer)) > -1) {
                    outStream.write(buffer, 0, bytesRead);
                }
                StreamUtil.close(outStream);
                CLog.i("Successfully copied the compressed content from device to"
                        + " host.");
            } catch (IOException e) {
                if (perfettoCompressedFile != null) {
                    perfettoCompressedFile.delete();
                }
                CLog.e("Failed to copy compressed perfetto to temporary file.");
                CLog.e(e);
            } finally {
                compressedOutputReceiver.delete();
            }
        } catch (DeviceNotAvailableException e) {
            CLog.e(
                    "Exception when retrieveing compressed perfetto trace file '%s' "
                            + "from %s", remoteFilePath, device.getSerialNumber());
            CLog.e(e);
        }
        return perfettoCompressedFile;
    }

    /**
     * Decompress the file to a temporary file in the host.
     *
     * @param compressedFile file to be decompressed.
     * @return decompressed file used for postprocessing.
     */
    private File decompressFile(File compressedFile) {
        File decompressedFile = null;
        try {
            decompressedFile = FileUtil.createTempFile("perfetto_decompressed", ".pb");
        } catch (IOException e) {
            CLog.e("Not able to create decompressed perfetto file.");
            CLog.e(e);
            return null;
        }
        // Keep the original file for uploading.
        List<String> decompressArgsList = new ArrayList<String>();
        decompressArgsList.add("gzip");
        decompressArgsList.add("-k");
        decompressArgsList.add("-c");
        decompressArgsList.add("-d");
        decompressArgsList.add(compressedFile.getAbsolutePath());

        // Decompress perfetto trace file.
        CLog.i("Start decompressing the perfetto trace file.");
        try (FileOutputStream outStream = new FileOutputStream(decompressedFile);
                ByteArrayOutputStream errStream = new ByteArrayOutputStream()) {
            CommandResult decompressResult = runHostCommand(mDecompressTimeoutMs,
                    decompressArgsList.toArray(new String[decompressArgsList
                            .size()]), outStream, errStream);

            if (!CommandStatus.SUCCESS.equals(decompressResult.getStatus())) {
                CLog.e("Unable decompress the metric file %s due to %s - Status - %s ",
                        compressedFile.getName(), errStream.toString(),
                        decompressResult.getStatus());
                decompressedFile.delete();
                return null;
            }
        } catch (FileNotFoundException e) {
            CLog.e("Not able to find the decompressed file to copy the"
                    + " decompressed contents.");
            CLog.e(e);
            return null;
        } catch (IOException e1) {
            CLog.e("Unable to close the streams.");
            CLog.e(e1);
        }
        CLog.i("Successfully decompressed the perfetto trace file.");
        return decompressedFile;
    }

    @Override
    public void processMetricDirectory(String key, File metricDirectory, DeviceMetricData runData) {
        // Implement if all the files under specific directory have to be post processed.
    }

    /**
     * Run a host command with the given array of command args.
     *
     * @param commandArgs args to be used to construct the host command.
     * @param stdout output of the command.
     * @param stderr error message if any from the command.
     * @return return the command results.
     */
    @VisibleForTesting
    CommandResult runHostCommand(long timeOut, String[] commandArgs, OutputStream stdout,
            OutputStream stderr) {
        if (stdout != null && stderr != null) {
            return RunUtil.getDefault().runTimedCmd(timeOut, stdout, stderr, commandArgs);
        }
        return RunUtil.getDefault().runTimedCmd(timeOut, commandArgs);
    }

    @VisibleForTesting
    @Nullable
    static Pair<String, String> splitKeyValue(String s) {
        // Expected script test output format.
        // Key1:Value1
        // Key2:Value2
        int separatorIdx = s.lastIndexOf(KEY_VALUE_SEPARATOR);
        if (separatorIdx > 0 && separatorIdx + 1 < s.length()) {
            return new Pair<>(s.substring(0, separatorIdx), s.substring(separatorIdx + 1));
        }
        return null;
    }

    /**
     * Get the log data type based on the output metric perfetto file.
     *
     * @return LogDataType type of the file used for uploading the artifacts.
     */
    private LogDataType getLogDataType() {
        // text option in perfetto trace processor means text proto.
        if(mTraceProcessorOutputFormat.equals(METRIC_FILE_FORMAT.text)) {
            return LogDataType.ZIP;
        } else if(mTraceProcessorOutputFormat.equals(METRIC_FILE_FORMAT.binary)) {
            return LogDataType.PB;
        } else {
            return LogDataType.TEXT;
        }
    }

    /**
     * Extract the raw trace file name used for constructing the output
     * perfetto metric file name
     *
     * @param rawTraceFileName
     * @return String name of the raw trace file name excluding the UUID.
     */
    private String getRawTraceFileName(String rawTraceFileName) {
        // For example return perfetto_<test_name>-1_ from
        // perfetto_<test_name>-1_13388308985625987330.pb excluding the UID.
        int lastIndex = rawTraceFileName.lastIndexOf("_");
        if (lastIndex != -1) {
            return rawTraceFileName.substring(0, lastIndex + 1);
        }
        return rawTraceFileName;
    }

    /**
     * Retrieve the current build info.
     *
     * @return BuildInfo which has access to test artifacts directory.
     */
    @VisibleForTesting
    IBuildInfo getCurrentBuildInfo() {
        return getBuildInfos().get(0);
    }

    /**
     * Compress the given file.
     *
     * @return File compressed version of the file.
     */
    @VisibleForTesting
    File getCompressedFile(File metricOutputFile) throws IOException {
        return ZipUtil.createZip(metricOutputFile,
                metricOutputFile.getName());
    }
}
