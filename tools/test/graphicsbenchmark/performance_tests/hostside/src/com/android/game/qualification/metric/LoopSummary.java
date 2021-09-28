package com.android.game.qualification.metric;

import com.android.annotations.Nullable;
import com.android.game.qualification.CertificationRequirements;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Directionality;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

/**
 * Summary of frame time metrics for a single loop.
 */
public class LoopSummary {
    private long count;
    private long totalTimeNs;
    private double jankRate;
    private long minFrameTime;
    private long maxFrameTime;
    private double avgFrameTime;
    private long percentile90;
    private long percentile95;
    private long percentile99;
    private double targetPercentile;

    public static LoopSummary parseRunMetrics(
            IInvocationContext context,
            MetricSummary.TimeType type,
            int runIndex,
            HashMap<String, Metric> runMetrics) {
        return new LoopSummary(
                getMetricLongValue(context, type, runIndex, "frame_count", runMetrics),
                getMetricLongValue(context, type, runIndex, "duration", runMetrics),
                getMetricDoubleValue(context, type, runIndex, "jank_rate", runMetrics),
                getMetricLongValue(context, type, runIndex, "min_frametime", runMetrics),
                getMetricLongValue(context, type, runIndex, "max_frametime", runMetrics),
                getMetricDoubleValue(context, type, runIndex, "frametime", runMetrics),
                getMetricLongValue(context, type, runIndex, "90th_percentile", runMetrics),
                getMetricLongValue(context, type, runIndex, "95th_percentile", runMetrics),
                getMetricLongValue(context, type, runIndex, "99th_percentile", runMetrics),
                getMetricDoubleValue(context, type, runIndex, "target_percentile", runMetrics));
    }

    void addToMetricData(DeviceMetricData runData, int index, MetricSummary.TimeType type) {
        runData.addMetric(
                getMetricKey(type, index, "frame_count"),
                Metric.newBuilder()
                        .setType(MetricMeasurement.DataType.PROCESSED)
                        .setMeasurements(
                                MetricMeasurement.Measurements.newBuilder()
                                        .setSingleInt(getCount())));
        runData.addMetric(
                getMetricKey(type, index, "duration"),
                getNsMetric(getDuration()));
        runData.addMetric(
                getMetricKey(type, index, "jank_rate"),
                Metric.newBuilder()
                        .setType(DataType.PROCESSED)
                        .setMeasurements(Measurements.newBuilder().setSingleDouble(getJankRate())));
        runData.addMetric(
                getMetricKey(type, index, "min_frametime"),
                getNsMetric(getMinFrameTime()));
        runData.addMetric(
                getMetricKey(type, index, "max_frametime"),
                getNsMetric(getMaxFrameTime()));
        runData.addMetric(
                getMetricKey(type, index, "frametime"),
                getNsMetric(getAvgFrameTime()));
        runData.addMetric(
                getMetricKey(type, index, "90th_percentile"),
                getNsMetric(get90thPercentile()));
        runData.addMetric(
                getMetricKey(type, index, "95th_percentile"),
                getNsMetric(get95thPercentile()));
        runData.addMetric(
                getMetricKey(type, index, "99th_percentile"),
                getNsMetric(get99thPercentile()));
        runData.addMetric(
                getMetricKey(type, index, "target_percentile"),
                Metric.newBuilder()
                        .setType(DataType.PROCESSED)
                        .setMeasurements(
                                Measurements.newBuilder().setSingleDouble(getTargetPercentile())));
    }

    private LoopSummary(
            long count,
            long totalTimeNs,
            double jankRate,
            long minFrameTime,
            long maxFrameTime,
            double avgFrameTime,
            long percentile90,
            long percentile95,
            long percentile99,
            double targetPercentile) {
        this.count = count;
        this.totalTimeNs = totalTimeNs;
        this.jankRate = jankRate;
        this.minFrameTime = minFrameTime;
        this.maxFrameTime = maxFrameTime;
        this.avgFrameTime = avgFrameTime;
        this.percentile90 = percentile90;
        this.percentile95 = percentile95;
        this.percentile99 = percentile99;
        this.targetPercentile = targetPercentile;
    }

    public long getCount() {
        return count;
    }

    public long getDuration() {
        return totalTimeNs;
    }

    public double getJankRate() {
        return jankRate;
    }

    public long getMinFrameTime() {
        return minFrameTime;
    }

    public long getMaxFrameTime() {
        return maxFrameTime;
    }

    public double getAvgFrameTime() {
        return avgFrameTime;
    }

    public double getMinFPS() {
        return 1.0e9 / maxFrameTime;
    }

    public double getMaxFPS() {
        return 1.0e9 / minFrameTime;
    }

    public double getAvgFPS() {
        return 1.0e9 / avgFrameTime;
    }

    public long get90thPercentile() {
        return percentile90;
    }

    public long get95thPercentile() {
        return percentile95;
    }

    public long get99thPercentile() {
        return percentile99;
    }

    public double getTargetPercentile() {
        return targetPercentile;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        LoopSummary that = (LoopSummary) o;
        return count == that.count &&
                totalTimeNs == that.totalTimeNs &&
                Double.compare(that.jankRate, jankRate) == 0 &&
                minFrameTime == that.minFrameTime &&
                maxFrameTime == that.maxFrameTime &&
                Double.compare(that.avgFrameTime, avgFrameTime) == 0 &&
                percentile90 == that.percentile90 &&
                percentile95 == that.percentile95 &&
                percentile99 == that.percentile99 &&
                Double.compare(that.targetPercentile, targetPercentile) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                count,
                totalTimeNs,
                jankRate,
                minFrameTime,
                maxFrameTime,
                avgFrameTime,
                percentile90,
                percentile95,
                percentile99,
                targetPercentile);
    }

    public String toString() {
        return String.format(
                "duration: %.3f ms\n"
                        + "Jank Rate: %7.3f/s\n"
                        + "avg Frame Time: %7.3f ms\t\tavg FPS = %.3f fps\n"
                        + "max Frame Time: %7.3f ms\n"
                        + "min Frame Time: %7.3f ms\n"
                        + "90th Percentile Frame Time: %7.3f ms\n"
                        + "95th Percentile Frame Time: %7.3f ms\n"
                        + "99th Percentile Frame Time: %7.3f ms\n"
                        + "Percentile below target: %7.3f\n",
                nsToMs(getDuration()),
                getJankRate(),
                nsToMs(getAvgFrameTime()), getAvgFPS(),
                nsToMs(getMaxFrameTime()),
                nsToMs(getMinFrameTime()),
                nsToMs(get90thPercentile()),
                nsToMs(get95thPercentile()),
                nsToMs(get99thPercentile()),
                targetPercentile * 100);
    }

    static class Builder {
        @Nullable
        private final CertificationRequirements mRequirements;
        private final long mVSyncPeriodNs;
        private long totalTimeNs = 0;
        private double jankScore;
        private List<Long> frameTimes = new ArrayList<>();

        public Builder(@Nullable CertificationRequirements requirements, long VSyncPeriodNs) {
            mRequirements = requirements;
            mVSyncPeriodNs = VSyncPeriodNs;
        }

        public LoopSummary build() {
            if (frameTimes.isEmpty()) {
                return new LoopSummary(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            frameTimes.sort(Comparator.naturalOrder());
            int size = frameTimes.size();
            long targetFrameTime = mRequirements == null ? 0 : msToNs(mRequirements.getFrameTime());

            // Find the percentage of frames below the target.
            // Allow for small amount of slack because frame times have some variability.  Frames
            // that misses the target should be off by at least 1 VSYNC period, so frames that are
            // just slightly above the target is still considered to be within target.
            final long slack = (long)(0.1 * mVSyncPeriodNs);
            int index = Collections.binarySearch(frameTimes, targetFrameTime + slack);
            // binarySearch return a negative number if exact match is not found.
            index = index < 0 ? -index - 1: index + 1;

            return new LoopSummary(
                    frameTimes.size(),
                    totalTimeNs,
                    jankScore * 1000000000 / totalTimeNs,
                    frameTimes.get(0),
                    frameTimes.get(frameTimes.size() - 1),
                    (double)totalTimeNs / frameTimes.size(),
                    frameTimes.get((int)Math.ceil(size * 0.90) - 1),
                    frameTimes.get((int)Math.ceil(size * 0.95) - 1),
                    frameTimes.get((int)Math.ceil(size * 0.99) - 1),
                    (double)index / size);
        }

        public void addFrameTime(long frameTimeNs) {
            if (mRequirements != null) {
                long targetFrameTime = msToNs(mRequirements.getFrameTime());
                long roundedFrameTimeNs =
                        Math.round(frameTimeNs / (double)mVSyncPeriodNs) * mVSyncPeriodNs;
                if (roundedFrameTimeNs > targetFrameTime) {
                    double score = (roundedFrameTimeNs - targetFrameTime) / targetFrameTime;
                    jankScore += score;
                }
            }
            totalTimeNs = totalTimeNs + frameTimeNs;
            frameTimes.add(frameTimeNs);
        }

        private static long msToNs(float value) {
            return (long) (value * 1e6f);
        }
    }

    private static double getMetricDoubleValue(
            IInvocationContext context,
            MetricSummary.TimeType type,
            int runIndex,
            String metric,
            HashMap<String, Metric> runMetrics) {
        Metric m = runMetrics.get(
                getActualMetricKey(context, type, runIndex, metric));
        if (!m.hasMeasurements()) {
            throw new RuntimeException();
        }
        return m.getMeasurements().getSingleDouble();
    }

    private static long getMetricLongValue(
            IInvocationContext context,
            MetricSummary.TimeType type,
            int runIndex,
            String metric,
            HashMap<String, Metric> runMetrics) {
        Metric m = runMetrics.get(
                getActualMetricKey(context, type, runIndex, metric));
        if (!m.hasMeasurements()) {
            throw new RuntimeException();
        }
        return m.getMeasurements().getSingleInt();
    }

    private static Metric.Builder getNsMetric(long value) {
        return Metric.newBuilder()
                .setUnit("ns")
                .setDirection(MetricMeasurement.Directionality.DOWN_BETTER)
                .setType(MetricMeasurement.DataType.PROCESSED)
                .setMeasurements(MetricMeasurement.Measurements.newBuilder().setSingleInt(value));
    }

    private static Metric.Builder getNsMetric(double value) {
        return Metric.newBuilder()
                .setUnit("ns")
                .setDirection(Directionality.DOWN_BETTER)
                .setType(DataType.PROCESSED)
                .setMeasurements(Measurements.newBuilder().setSingleDouble(value));
    }

    private static String getActualMetricKey(
            IInvocationContext context, MetricSummary.TimeType type, int loopIndex, String label) {
        // DeviceMetricData automatically add the deviceName to the metric key if there are more
        // than one devices.  We don't really want or care about the device in the metric data, but
        // we need to get the actual key that was added in order to parse it correctly.
        if (context.getDevices().size() > 1) {
            String deviceName = context.getDeviceName(context.getDevices().get(0));
            return String.format("{%s}:%s", deviceName, getMetricKey(type, loopIndex, label));
        }
        return getMetricKey(type, loopIndex, label);
    }

    private static String getMetricKey(MetricSummary.TimeType type, int loopIndex, String label) {
        return "run_" + loopIndex + "." + type.name().toLowerCase(Locale.US) + "_" + label;
    }

    private static double nsToMs(long value) {
        return value / 1e6;
    }

    private static double nsToMs(double value) {
        return value / 1e6;
    }
}
