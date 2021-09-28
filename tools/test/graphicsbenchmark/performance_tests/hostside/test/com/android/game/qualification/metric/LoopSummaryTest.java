package com.android.game.qualification.metric;

import static org.junit.Assert.assertEquals;

import com.android.game.qualification.CertificationRequirements;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;

import org.junit.Test;

import java.util.HashMap;

public class LoopSummaryTest {
    private static final double EPSILON = Math.ulp(1e9);
    private static final CertificationRequirements TEST_REQUIREMENTS =
            new CertificationRequirements(
                    "foo",
                    500,  /* 500ms */
                    0.0f,
                    10000);

    @Test
    public void testBasicLoop() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        builder.addFrameTime(1);
        LoopSummary summary = builder.build();
        assertEquals(1, summary.getDuration());
        assertEquals(0.0, summary.getJankRate(), EPSILON);
        assertEquals(1, summary.getAvgFrameTime(), EPSILON);
        assertEquals(1, summary.getMinFrameTime());
        assertEquals(1, summary.getMaxFrameTime());
        assertEquals(1e9, summary.getAvgFPS(), EPSILON);
        assertEquals(1e9, summary.getMinFPS(), EPSILON);
        assertEquals(1e9, summary.getMaxFPS(), EPSILON);
        assertEquals(1, summary.get90thPercentile());
        assertEquals(1, summary.get95thPercentile());
        assertEquals(1, summary.get99thPercentile());
        assertEquals(1.0, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testSimpleLoop() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        builder.addFrameTime(1);
        builder.addFrameTime(2);
        builder.addFrameTime(3);
        LoopSummary summary = builder.build();
        assertEquals(6, summary.getDuration());
        assertEquals(0.0, summary.getJankRate(), EPSILON);
        assertEquals(2, summary.getAvgFrameTime(), EPSILON);
        assertEquals(1, summary.getMinFrameTime());
        assertEquals(3, summary.getMaxFrameTime());
        assertEquals(1e9 / 2, summary.getAvgFPS(), EPSILON);
        assertEquals(1e9 / 3, summary.getMinFPS(), EPSILON);
        assertEquals(1e9 / 1, summary.getMaxFPS(), EPSILON);
        assertEquals(3, summary.get90thPercentile());
        assertEquals(3, summary.get95thPercentile());
        assertEquals(3, summary.get99thPercentile());
        assertEquals(1.0, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testLargeDataSetLoop() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        for (int i = 0; i < 1000; i++) {
            builder.addFrameTime(i + 1);
        }
        LoopSummary summary = builder.build();
        assertEquals(500500, summary.getDuration());
        assertEquals(0.0, summary.getJankRate(), EPSILON);
        assertEquals(500.5, summary.getAvgFrameTime(), EPSILON);
        assertEquals(1, summary.getMinFrameTime());
        assertEquals(1000, summary.getMaxFrameTime());
        assertEquals(1e9 / 500.5, summary.getAvgFPS(), EPSILON);
        assertEquals(1e9 / 1000, summary.getMinFPS(), EPSILON);
        assertEquals(1e9 / 1, summary.getMaxFPS(), EPSILON);
        assertEquals(900, summary.get90thPercentile());
        assertEquals(950, summary.get95thPercentile());
        assertEquals(990, summary.get99thPercentile());
        assertEquals(1.0, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testTargetPercentile() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        for (long i = 50_000_000; i <= 2_500_000_000L; i = i + 50_000_000) {
            builder.addFrameTime(i);
        }
        LoopSummary summary = builder.build();
        assertEquals(0.2, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testTargetPercentileOne() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        builder.addFrameTime(0);
        LoopSummary summary = builder.build();
        assertEquals(1.0, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testTargetPercentileZero() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        builder.addFrameTime(500_000_001);
        LoopSummary summary = builder.build();
        assertEquals(0.0, summary.getTargetPercentile(), EPSILON);
    }

    @Test
    public void testJankRate() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 500_000_000);
        builder.addFrameTime(1);
        builder.addFrameTime(2);
        builder.addFrameTime(3);
        builder.addFrameTime(749_999_999);
        builder.addFrameTime(750_000_000);
        builder.addFrameTime(499_999_995);
        LoopSummary summary = builder.build();
        assertEquals(0.5, summary.getJankRate(), EPSILON);
    }

    @Test
    public void testParseRunMetrics() {
        LoopSummary.Builder builder = new LoopSummary.Builder(TEST_REQUIREMENTS, 1);
        builder.addFrameTime(1);
        builder.addFrameTime(2);
        builder.addFrameTime(3);
        LoopSummary expected = builder.build();

        IInvocationContext context = new InvocationContext();
        DeviceMetricData runData = new DeviceMetricData(context);
        expected.addToMetricData(runData, 0, MetricSummary.TimeType.PRESENT);

        HashMap<String, MetricMeasurement.Metric> metrics = new HashMap<>();
        runData.addToMetrics(metrics);

        LoopSummary summary = LoopSummary.parseRunMetrics(
                context,
                MetricSummary.TimeType.PRESENT,
                0,
                metrics);

        assertEquals(expected, summary);

        assertEquals(6, summary.getDuration());
        assertEquals(0.0, summary.getJankRate(), EPSILON);
        assertEquals(2, summary.getAvgFrameTime(), EPSILON);
        assertEquals(1, summary.getMinFrameTime());
        assertEquals(3, summary.getMaxFrameTime());
        assertEquals(1e9 / 2, summary.getAvgFPS(), EPSILON);
        assertEquals(1e9 / 3, summary.getMinFPS(), EPSILON);
        assertEquals(1e9 / 1, summary.getMaxFPS(), EPSILON);
        assertEquals(3, summary.get90thPercentile());
        assertEquals(3, summary.get95thPercentile());
        assertEquals(3, summary.get99thPercentile());
        assertEquals(1.0, summary.getTargetPercentile(), EPSILON);
    }
}