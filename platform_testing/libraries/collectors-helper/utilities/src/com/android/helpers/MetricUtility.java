package com.android.helpers;

import android.app.Instrumentation;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;

/**
 * MetricUtility consist of basic utility methods to construct the metrics
 * reported at the end of the test.
 */
public class MetricUtility {

    private static final String TAG = MetricUtility.class.getSimpleName();
    private static final String KEY_JOIN = "_";
    private static final String METRIC_SEPARATOR = ",";

    public static final int BUFFER_SIZE = 1024;

    /**
     * Append the given array of string to construct the final key used to track the metrics.
     *
     * @param keys to append using KEY_JOIN
     */
    public static String constructKey(String... keys) {
        return String.join(KEY_JOIN, keys);
    }

    /**
     * Add metric to the result map. If metric key already exist append the new metric.
     *
     * @param metricKey Unique key to track the metric.
     * @param metric metric to track.
     * @param resultMap map of all the metrics.
     */
    public static void addMetric(String metricKey, long metric, Map<String,
            StringBuilder> resultMap) {
        resultMap.compute(metricKey, (key, value) -> (value == null) ?
                new StringBuilder().append(metric) : value.append(METRIC_SEPARATOR).append(metric));
    }

    /**
     * Add metric to the result map. If metric key already exist increment the value by 1.
     *
     * @param metricKey Unique key to track the metric.
     * @param resultMap map of all the metrics.
     */
    public static void addMetric(String metricKey, Map<String,
            Integer> resultMap) {
        resultMap.compute(metricKey, (key, value) -> (value == null) ? 1 : value + 1);
    }

    /**
     * Turn executeShellCommand into a blocking operation.
     *
     * @param command shell command to be executed.
     * @param instr used to run the shell command.
     * @return byte array of execution result
     */
    public static byte[] executeCommandBlocking(String command, Instrumentation instr) {
        try (InputStream is = new ParcelFileDescriptor.AutoCloseInputStream(instr.getUiAutomation()
                .executeShellCommand(command));
                ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buf = new byte[BUFFER_SIZE];
            int length;
            Log.i(TAG, "Start reading the data");
            while ((length = is.read(buf)) >= 0) {
                out.write(buf, 0, length);
            }
            Log.i(TAG, "Stop reading the data");
            return out.toByteArray();
        } catch (IOException e) {
            Log.e(TAG, "Error executing: " + command, e);
            return null;
        }
    }

}
