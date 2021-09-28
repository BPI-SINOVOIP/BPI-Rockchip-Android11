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
import static com.android.helpers.JankCollectionHelper.GFXINFO_COMMAND_GET;
import static com.android.helpers.JankCollectionHelper.GFXINFO_COMMAND_RESET;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.TOTAL_FRAMES;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.JANKY_FRAMES_COUNT;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.JANKY_FRAMES_PRCNT;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.FRAME_TIME_50TH;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.FRAME_TIME_90TH;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.FRAME_TIME_95TH;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.FRAME_TIME_99TH;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_MISSED_VSYNC;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_HIGH_INPUT_LATENCY;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_SLOW_UI_THREAD;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_SLOW_BITMAP_UPLOADS;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_SLOW_DRAW;
import static com.android.helpers.JankCollectionHelper.GfxInfoMetric.NUM_FRAME_DEADLINE_MISSED;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.support.test.uiautomator.UiDevice;
import androidx.test.runner.AndroidJUnit4;

import java.io.IOException;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

/** Android Unit tests for {@link JankCollectionHelper}. */
@RunWith(AndroidJUnit4.class)
public class JankCollectionHelperTest {
    private static final String GFXINFO_RESET_FORMAT =
            "\n\n** Graphics info for pid 9999 [%s] **"
                    + "\n"
                    + "\nTotal frames rendered: 0"
                    + "\nJanky frames: 0 (00.00%%)"
                    + "\n50th percentile: 0ms"
                    + "\n90th percentile: 0ms"
                    + "\n95th percentile: 0ms"
                    + "\n99th percentile: 0ms"
                    + "\nNumber Missed Vsync: 0"
                    + "\nNumber High input latency: 0"
                    + "\nNumber Slow UI thread: 0"
                    + "\nNumber Slow bitmap uploads: 0"
                    + "\nNumber Slow issue draw commands: 0"
                    + "\nNumber Frame deadline missed: 0";
    private static final String GFXINFO_GET_FORMAT =
            "\n\n** Graphics info for pid 9999 [%s] **"
                    + "\n"
                    + "\nTotal frames rendered: 900"
                    + "\nJanky frames: 300 (33.33%%)"
                    + "\n50th percentile: 150ms"
                    + "\n90th percentile: 190ms"
                    + "\n95th percentile: 195ms"
                    + "\n99th percentile: 199ms"
                    + "\nNumber Missed Vsync: 1"
                    + "\nNumber High input latency: 2"
                    + "\nNumber Slow UI thread: 3"
                    + "\nNumber Slow bitmap uploads: 4"
                    + "\nNumber Slow issue draw commands: 5"
                    + "\nNumber Frame deadline missed: 6";

    private @Mock UiDevice mUiDevice;
    private JankCollectionHelper mHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHelper = Mockito.spy(new JankCollectionHelper());
        when(mHelper.getDevice()).thenReturn(mUiDevice);
    }

    /** Test track a single, valid package. */
    @Test
    public void testCollect_valuesMatch() throws Exception {
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg1"));
        mockGetCommand("pkg1", String.format(GFXINFO_GET_FORMAT, "pkg1"));

        mHelper.addTrackedPackages("pkg1");
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        assertThat(metrics.get(buildMetricKey("pkg1", TOTAL_FRAMES.getMetricId())))
                .isEqualTo(900.0);
        assertThat(metrics.get(buildMetricKey("pkg1", JANKY_FRAMES_COUNT.getMetricId())))
                .isEqualTo(300.0);
        assertThat(metrics.get(buildMetricKey("pkg1", JANKY_FRAMES_PRCNT.getMetricId())))
                .isEqualTo(33.33);
        assertThat(metrics.get(buildMetricKey("pkg1", FRAME_TIME_50TH.getMetricId())))
                .isEqualTo(150.0);
        assertThat(metrics.get(buildMetricKey("pkg1", FRAME_TIME_90TH.getMetricId())))
                .isEqualTo(190.0);
        assertThat(metrics.get(buildMetricKey("pkg1", FRAME_TIME_95TH.getMetricId())))
                .isEqualTo(195.0);
        assertThat(metrics.get(buildMetricKey("pkg1", FRAME_TIME_99TH.getMetricId())))
                .isEqualTo(199.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_MISSED_VSYNC.getMetricId())))
                .isEqualTo(1.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_HIGH_INPUT_LATENCY.getMetricId())))
                .isEqualTo(2.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_SLOW_UI_THREAD.getMetricId())))
                .isEqualTo(3.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_SLOW_BITMAP_UPLOADS.getMetricId())))
                .isEqualTo(4.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_SLOW_DRAW.getMetricId()))).isEqualTo(5.0);
        assertThat(metrics.get(buildMetricKey("pkg1", NUM_FRAME_DEADLINE_MISSED.getMetricId())))
                .isEqualTo(6.0);
        mHelper.stopCollecting();
    }

    /** Test track a single, valid package. */
    @Test
    public void testCollect_singlePackage() throws Exception {
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg1"));
        mockGetCommand("pkg1", String.format(GFXINFO_GET_FORMAT, "pkg1"));

        mHelper.addTrackedPackages("pkg1");
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        for (String key : metrics.keySet()) {
            assertWithMessage("All keys must contains the single watched package name.")
                    .that(key)
                    .contains("pkg1");
        }
        mHelper.stopCollecting();
    }

    /** Test track multiple valid packages. */
    @Test
    public void testCollect_multiPackage() throws Exception {
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg1"));
        mockGetCommand("pkg1", String.format(GFXINFO_GET_FORMAT, "pkg1"));
        mockResetCommand("pkg2", String.format(GFXINFO_RESET_FORMAT, "pkg2"));
        mockGetCommand("pkg2", String.format(GFXINFO_GET_FORMAT, "pkg2"));
        mockResetCommand("pkg3", String.format(GFXINFO_RESET_FORMAT, "pkg3"));
        mockGetCommand("pkg3", String.format(GFXINFO_GET_FORMAT, "pkg3"));

        mHelper.addTrackedPackages("pkg1", "pkg2");
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        // Assert against all keys that they only match expected packages.
        for (String key : metrics.keySet()) {
            assertWithMessage("All keys must contains one of the 2 watched package names.")
                    .that(key)
                    .containsMatch(".*pkg(1|2).*");
            assertWithMessage("The unwatched package should not be included in metrics.")
                    .that(key)
                    .doesNotContain("pkg3");
        }
        // Assert that it contains keys for both packages being watched.
        assertThat(metrics).containsKey(buildMetricKey("pkg1", TOTAL_FRAMES.getMetricId()));
        assertThat(metrics).containsKey(buildMetricKey("pkg2", TOTAL_FRAMES.getMetricId()));
        mHelper.stopCollecting();
    }

    /** Test track all packages when unspecified. */
    @Test
    public void testCollect_allPackages() throws Exception {
        String resetOutput =
                String.join(
                        "\n",
                        String.format(GFXINFO_RESET_FORMAT, "pkg1"),
                        String.format(GFXINFO_RESET_FORMAT, "pkg2"),
                        String.format(GFXINFO_RESET_FORMAT, "pkg3"));
        String getOutput =
                String.join(
                        "\n",
                        String.format(GFXINFO_GET_FORMAT, "pkg1"),
                        String.format(GFXINFO_GET_FORMAT, "pkg2"),
                        String.format(GFXINFO_GET_FORMAT, "pkg3"));
        mockResetCommand("", resetOutput);
        mockGetCommand("", getOutput);

        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        // Assert against all keys that they only match expected packages.
        for (String key : metrics.keySet()) {
            assertWithMessage("All keys must contains one of the output package names.")
                    .that(key)
                    .containsMatch(".*pkg(1|2|3).*");
        }
        // Assert that it contains keys for all packages being watched.
        assertThat(metrics).containsKey(buildMetricKey("pkg1", TOTAL_FRAMES.getMetricId()));
        assertThat(metrics).containsKey(buildMetricKey("pkg2", TOTAL_FRAMES.getMetricId()));
        assertThat(metrics).containsKey(buildMetricKey("pkg3", TOTAL_FRAMES.getMetricId()));
        mHelper.stopCollecting();
    }

    /** Test that it collects available fields, even if some are missing. */
    @Test
    public void testCollect_ignoreMissingFields() throws Exception {
        String missingResets =
                "\n\n** Graphics info for pid 9999 [pkg1] **"
                        + "\n"
                        + "\nTotal frames rendered: 0"
                        + "\nJanky frames: 0 (00.00%%)"
                        + "\nNumber Missed Vsync: 0"
                        + "\nNumber High input latency: 0"
                        + "\nNumber Slow UI thread: 0"
                        + "\nNumber Slow bitmap uploads: 0"
                        + "\nNumber Slow issue draw commands: 0"
                        + "\nNumber Frame deadline missed: 0";
        String missingGets =
                "\n\n** Graphics info for pid 9999 [pkg1] **"
                        + "\n"
                        + "\nTotal frames rendered: 900"
                        + "\nJanky frames: 300 (33.33%)"
                        + "\nNumber Missed Vsync: 1"
                        + "\nNumber High input latency: 2"
                        + "\nNumber Slow UI thread: 3"
                        + "\nNumber Slow bitmap uploads: 4"
                        + "\nNumber Slow issue draw commands: 5"
                        + "\nNumber Frame deadline missed: 6";

        mockResetCommand("pkg1", missingResets);
        mockGetCommand("pkg1", missingGets);

        mHelper.addTrackedPackages("pkg1");
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        assertThat(metrics.keySet())
                .containsExactly(
                        buildMetricKey("pkg1", TOTAL_FRAMES.getMetricId()),
                        buildMetricKey("pkg1", JANKY_FRAMES_COUNT.getMetricId()),
                        buildMetricKey("pkg1", JANKY_FRAMES_PRCNT.getMetricId()),
                        buildMetricKey("pkg1", NUM_MISSED_VSYNC.getMetricId()),
                        buildMetricKey("pkg1", NUM_HIGH_INPUT_LATENCY.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_UI_THREAD.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_BITMAP_UPLOADS.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_DRAW.getMetricId()),
                        buildMetricKey("pkg1", NUM_FRAME_DEADLINE_MISSED.getMetricId()));
        mHelper.stopCollecting();
    }

    /** Test that it collects known fields, even if some are unknown. */
    @Test
    public void testCollect_ignoreUnknownField() throws Exception {
        String extraFields =
                "\nWhatever: 1"
                        + "\nWhateverClose: 2"
                        + "\nWhateverNotSo: 3"
                        + "\nWhateverBlahs: 4";
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT + extraFields, "pkg1"));
        mockGetCommand("pkg1", String.format(GFXINFO_GET_FORMAT + extraFields, "pkg1"));

        mHelper.addTrackedPackages("pkg1");
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        assertThat(metrics.keySet())
                .containsExactly(
                        buildMetricKey("pkg1", TOTAL_FRAMES.getMetricId()),
                        buildMetricKey("pkg1", JANKY_FRAMES_COUNT.getMetricId()),
                        buildMetricKey("pkg1", JANKY_FRAMES_PRCNT.getMetricId()),
                        buildMetricKey("pkg1", FRAME_TIME_50TH.getMetricId()),
                        buildMetricKey("pkg1", FRAME_TIME_90TH.getMetricId()),
                        buildMetricKey("pkg1", FRAME_TIME_95TH.getMetricId()),
                        buildMetricKey("pkg1", FRAME_TIME_99TH.getMetricId()),
                        buildMetricKey("pkg1", NUM_MISSED_VSYNC.getMetricId()),
                        buildMetricKey("pkg1", NUM_HIGH_INPUT_LATENCY.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_UI_THREAD.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_BITMAP_UPLOADS.getMetricId()),
                        buildMetricKey("pkg1", NUM_SLOW_DRAW.getMetricId()),
                        buildMetricKey("pkg1", NUM_FRAME_DEADLINE_MISSED.getMetricId()));
        mHelper.stopCollecting();
    }

    /** Test that it continues resetting even if certain packages throw for some reason. */
    @Test
    public void testCollect_delayExceptions_onReset() throws Exception {
        // Package 1 is problematic to reset, but package 2 and 3 are good.
        String cmd = String.format(GFXINFO_COMMAND_RESET, "pkg1");
        when(mUiDevice.executeShellCommand(cmd)).thenThrow(new RuntimeException());
        mockResetCommand("pkg2", String.format(GFXINFO_RESET_FORMAT, "pkg2"));
        mockResetCommand("pkg3", String.format(GFXINFO_RESET_FORMAT, "pkg3"));

        mHelper.addTrackedPackages("pkg1", "pkg2", "pkg3");
        try {
            mHelper.startCollecting();
            fail("Should have thrown an exception resetting pkg1.");
        } catch (Exception e) {
            // assert that all of the packages were reset and pass.
            InOrder inOrder = inOrder(mUiDevice);
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg1"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg2"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg3"));
        }
    }

    /** Test that it continues collecting even if certain packages throw for some reason. */
    @Test
    public void testCollect_delayExceptions_onGet() throws Exception {
        // Package 1 is problematic to reset, but package 2 and 3 are good.
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg1"));
        mockResetCommand("pkg2", String.format(GFXINFO_RESET_FORMAT, "pkg2"));
        mockResetCommand("pkg3", String.format(GFXINFO_RESET_FORMAT, "pkg3"));
        String cmd = String.format(GFXINFO_COMMAND_GET, "pkg1");
        when(mUiDevice.executeShellCommand(cmd)).thenThrow(new RuntimeException());
        mockGetCommand("pkg2", String.format(GFXINFO_GET_FORMAT, "pkg2"));
        mockGetCommand("pkg3", String.format(GFXINFO_GET_FORMAT, "pkg3"));

        mHelper.addTrackedPackages("pkg1", "pkg2", "pkg3");
        try {
            mHelper.startCollecting();
            mHelper.getMetrics();
            fail("Should have thrown an exception getting pkg1.");
        } catch (Exception e) {
            // assert that all of the packages were reset and gotten and pass.
            InOrder inOrder = inOrder(mUiDevice);
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg1"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg2"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_RESET, "pkg3"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_GET, "pkg1"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_GET, "pkg2"));
            inOrder.verify(mUiDevice)
                    .executeShellCommand(String.format(GFXINFO_COMMAND_GET, "pkg3"));
        }
    }

    /** Test that it fails if the {@code gfxinfo} metrics cannot be cleared. */
    @Test
    public void testFailures_cannotClear() throws Exception {
        String cmd = String.format(JankCollectionHelper.GFXINFO_COMMAND_RESET, "");
        when(mUiDevice.executeShellCommand(cmd)).thenReturn("");
        try {
            mHelper.startCollecting();
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // pass
        }
    }

    /** Test that it fails when encountering an {@code IOException} on reset. */
    @Test
    public void testFailures_ioFailure() throws Exception {
        String cmd = String.format(JankCollectionHelper.GFXINFO_COMMAND_RESET, "");
        when(mUiDevice.executeShellCommand(cmd)).thenThrow(new IOException());
        try {
            mHelper.startCollecting();
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // pass
        }
    }

    /** Test that it fails when the package does not show up on reset. */
    @Test
    public void testFailures_noPackageOnReset() throws Exception {
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg2"));

        mHelper.addTrackedPackages("pkg1");
        try {
            mHelper.startCollecting();
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // pass
        }
    }

    /** Test that it fails when the package does not show up on get. */
    @Test
    public void testFailures_noPackageOnGet() throws Exception {
        mockResetCommand("pkg1", String.format(GFXINFO_RESET_FORMAT, "pkg1"));
        mockGetCommand("pkg1", String.format(GFXINFO_GET_FORMAT, "pkg2"));

        mHelper.addTrackedPackages("pkg1");
        try {
            mHelper.startCollecting();
            mHelper.getMetrics();
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // pass
        }
    }

    private String buildMetricKey(String pkg, String id) {
        return constructKey(JankCollectionHelper.GFXINFO_METRICS_PREFIX, pkg, id);
    }

    private void mockResetCommand(String pkg, String output) throws IOException {
        String cmd = String.format(GFXINFO_COMMAND_RESET, pkg.isEmpty() ? "--" : pkg);
        when(mUiDevice.executeShellCommand(cmd)).thenReturn(output);
    }

    private void mockGetCommand(String pkg, String output) throws IOException {
        String cmd = String.format(GFXINFO_COMMAND_GET, pkg);
        when(mUiDevice.executeShellCommand(cmd)).thenReturn(output);
    }
}
