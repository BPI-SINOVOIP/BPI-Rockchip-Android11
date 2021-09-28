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
package com.android.helpers;

import static com.android.helpers.MetricUtility.constructKey;

import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;

/**
 * An {@link ICollectorHelper} for collecting SurfaceFlinger time stats.
 *
 * <p>This parses the output of {@code dumpsys SurfaceFlinger --timestats} and returns a collection
 * of both global metrics and metrics tracked for each layer.
 */
public class SfStatsCollectionHelper implements ICollectorHelper<Double> {

    private static final String LOG_TAG = SfStatsCollectionHelper.class.getSimpleName();

    private static final Pattern KEY_VALUE_PATTERN =
            Pattern.compile("^(\\w+)\\s+=\\s+(\\d+\\.?\\d*|.*).*");
    private static final Pattern HISTOGRAM_PATTERN =
            Pattern.compile("([^\\n]+)\\n((\\d+ms=\\d+\\s+)+)");

    private static final String FRAME_DURATION_KEY = "frameDuration histogram is as below:";
    private static final String RENDER_ENGINE_KEY = "renderEngineTiming histogram is as below:";

    @VisibleForTesting static final String SFSTATS_METRICS_PREFIX = "SFSTATS";

    @VisibleForTesting static final String SFSTATS_COMMAND = "dumpsys SurfaceFlinger --timestats ";

    @VisibleForTesting
    static final String SFSTATS_COMMAND_ENABLE_AND_CLEAR = SFSTATS_COMMAND + "-enable -clear";

    @VisibleForTesting static final String SFSTATS_COMMAND_DUMP = SFSTATS_COMMAND + "-dump";

    @VisibleForTesting
    static final String SFSTATS_COMMAND_DISABLE_AND_CLEAR = SFSTATS_COMMAND + "-disable -clear";

    private UiDevice mDevice;

    @Override
    public boolean startCollecting() {
        try {
            getDevice().executeShellCommand(SFSTATS_COMMAND_ENABLE_AND_CLEAR);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Encountered exception enabling dumpsys SurfaceFlinger.", e);
            throw new RuntimeException(e);
        }
        return true;
    }

    @Override
    public Map<String, Double> getMetrics() {
        Map<String, Double> results = new HashMap<>();
        String output;
        try {
            output = getDevice().executeShellCommand(SFSTATS_COMMAND_DUMP);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Encountered exception calling dumpsys SurfaceFlinger.", e);
            throw new RuntimeException(e);
        }
        String[] blocks = output.split("\n\n");

        HashMap<String, String> globalPairs = getStatPairs(blocks[0]);
        Map<String, Histogram> histogramPairs = getHistogramPairs(blocks[0]);

        for (String key : globalPairs.keySet()) {
            String metricKey = constructKey(SFSTATS_METRICS_PREFIX, "GLOBAL", key.toUpperCase());
            results.put(metricKey, Double.valueOf(globalPairs.get(key)));
        }

        if (histogramPairs.containsKey(FRAME_DURATION_KEY)) {
            results.put(
                    constructKey(SFSTATS_METRICS_PREFIX, "GLOBAL", "FRAME_CPU_DURATION_AVG"),
                    histogramPairs.get(FRAME_DURATION_KEY).mean());
        }

        if (histogramPairs.containsKey(RENDER_ENGINE_KEY)) {
            results.put(
                    constructKey(SFSTATS_METRICS_PREFIX, "GLOBAL", "RENDER_ENGINE_DURATION_AVG"),
                    histogramPairs.get(RENDER_ENGINE_KEY).mean());
        }

        for (int i = 1; i < blocks.length; i++) {
            HashMap<String, String> layerPairs = getStatPairs(blocks[i]);
            String layerName = layerPairs.get("layerName");
            String totalFrames = layerPairs.get("totalFrames");
            String droppedFrames = layerPairs.get("droppedFrames");
            String averageFPS = layerPairs.get("averageFPS");
            results.put(
                    constructKey(SFSTATS_METRICS_PREFIX, layerName, "TOTAL_FRAMES"),
                    Double.valueOf(totalFrames));
            results.put(
                    constructKey(SFSTATS_METRICS_PREFIX, layerName, "DROPPED_FRAMES"),
                    Double.valueOf(droppedFrames));
            results.put(
                    constructKey(SFSTATS_METRICS_PREFIX, layerName, "AVERAGE_FPS"),
                    Double.valueOf(averageFPS));
        }

        return results;
    }

    @Override
    public boolean stopCollecting() {
        try {
            getDevice().executeShellCommand(SFSTATS_COMMAND_DISABLE_AND_CLEAR);
        } catch (Exception e) {
            Log.e(LOG_TAG, "Encountered exception disabling dumpsys SurfaceFlinger.", e);
            throw new RuntimeException(e);
        }
        return true;
    }

    /** Returns the {@link UiDevice} under test. */
    @VisibleForTesting
    protected UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    /**
     * Returns a map of key-value pairs for every line of timestats for each layer handled by
     * SurfaceFlinger as well as some global SurfaceFlinger stats. An output line like {@code
     * totalFrames = 42} would get parsed and be accessable as {@code pairs.get("totalFrames") =>
     * "42"}
     */
    private HashMap<String, String> getStatPairs(String block) {
        HashMap<String, String> pairs = new HashMap<>();
        String[] lines = block.split("\n");
        for (int j = 0; j < lines.length; j++) {
            Matcher keyValueMatcher = KEY_VALUE_PATTERN.matcher(lines[j].trim());
            if (keyValueMatcher.matches()) {
                pairs.put(keyValueMatcher.group(1), keyValueMatcher.group(2));
            }
        }
        return pairs;
    }

    /**
     * Returns a map of {@link Histogram} instances emitted by SurfaceFlinger stats.
     *
     * <p>Input must be of the format defined by the {@link HISTOGRAM_PATTERN} regex. Example input
     * may include:
     *
     * <pre>{@code
     * Sample key:
     * 0ms=0 1ms=1 2ms=4 3ms=9 4ms=16
     * }</pre>
     *
     * <p>The corresponding output would include "Sample key:" as the key for a {@link Histogram}
     * instance constructed from the string {@code 0ms=0 1ms=1 2ms=4 3ms=9 4ms=16}.
     */
    private Map<String, Histogram> getHistogramPairs(String block) {
        Map<String, Histogram> pairs = new HashMap<>();
        Matcher histogramMatcher = HISTOGRAM_PATTERN.matcher(block);
        while (histogramMatcher.find()) {
            String key = histogramMatcher.group(1);
            String histogramString = histogramMatcher.group(2);
            Histogram histogram = new Histogram();
            Stream.of(histogramString.split("\\s+"))
                    .forEach(
                            bucket ->
                                    histogram.put(
                                            Integer.valueOf(
                                                    bucket.substring(0, bucket.indexOf("ms"))),
                                            Long.valueOf(
                                                    bucket.substring(bucket.indexOf("=") + 1))));
            pairs.put(key, histogram);
        }
        return pairs;
    }

    /** Representation for a statistical histogram */
    private static final class Histogram {
        private final Map<Integer, Long> internalMap;

        /** Constructs a histogram instance. */
        Histogram() {
            internalMap = new HashMap<>();
        }

        /**
         * Puts a (key, value) pair in the histogram.
         *
         * <p>The key would represent the particular bucket that the value is inserted into.
         */
        Histogram put(Integer key, Long value) {
            internalMap.put(key, value);
            return this;
        }

        /**
         * Computes the mean of the histogram
         *
         * @return 0 if the histogram is empty, the true mean otherwise.
         */
        double mean() {
            long count = internalMap.values().stream().mapToLong(v -> v).sum();
            if (count <= 0) {
                return 0.0;
            }
            long numerator =
                    internalMap
                            .entrySet()
                            .stream()
                            .mapToLong(entry -> entry.getKey() * entry.getValue())
                            .sum();
            return (double) numerator / count;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof Histogram)) {
                return false;
            }

            Histogram other = (Histogram) obj;

            return internalMap.equals(other.internalMap);
        }

        @Override
        public int hashCode() {
            return internalMap.hashCode();
        }
    }
}
