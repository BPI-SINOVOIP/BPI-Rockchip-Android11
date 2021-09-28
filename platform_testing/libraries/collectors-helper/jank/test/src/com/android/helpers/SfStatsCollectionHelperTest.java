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
import static com.android.helpers.SfStatsCollectionHelper.SFSTATS_COMMAND_DISABLE_AND_CLEAR;
import static com.android.helpers.SfStatsCollectionHelper.SFSTATS_COMMAND_DUMP;
import static com.android.helpers.SfStatsCollectionHelper.SFSTATS_COMMAND_ENABLE_AND_CLEAR;
import static com.android.helpers.SfStatsCollectionHelper.SFSTATS_METRICS_PREFIX;
import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;

import android.support.test.uiautomator.UiDevice;
import java.io.IOException;
import java.util.Map;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

public class SfStatsCollectionHelperTest {
    private static final String SFSTATS_DUMP =
            "SurfaceFlinger TimeStats:\n"
                    + "statsStart = 1566336030\n"
                    + "statsEnd = 1566338516\n"
                    + "totalFrames = 5791\n"
                    + "missedFrames = 82\n"
                    + "clientCompositionFrames = 0\n"
                    + "displayOnTime = 2485421 ms\n"
                    + "displayConfigStats is as below:\n"
                    + "90fps=3700ms 60fps=297492ms\n"
                    + "totalP2PTime = 2674034 ms\n"
                    + "frameDuration histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=50 6ms=50 7ms=0 8ms=0 9ms=0\n"
                    + "renderEngineTiming histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=50 7ms=50 8ms=0 9ms=0\n"
                    + "presentToPresent histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5791 9ms=0\n"
                    + "\n"
                    + "layerName = com.breel.wallpapers19.DoodleWallpaperV1#0\n"
                    + "packageName = com.breel.wallpapers19.DoodleWallpaperV1\n"
                    + "totalFrames = 5448\n"
                    + "droppedFrames = 4\n"
                    + "averageFPS = 2.038\n"
                    + "present2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "latch2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "desired2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "acquire2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "post2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "post2acquire histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=5448 9ms=0\n"
                    + "\n"
                    + "layerName = com.google.android.nexuslauncher.NexusLauncherActivity#0\n"
                    + "packageName = com.google.android.nexuslauncher\n"
                    + "totalFrames = 264\n"
                    + "droppedFrames = 7\n"
                    + "averageFPS = 84.318\n"
                    + "present2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "latch2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "desired2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "acquire2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "post2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "post2acquire histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=264 9ms=0\n"
                    + "\n"
                    + "layerName = SurfaceView - com.mxtech.videoplayer.ad/com.mxtech.videoplayer.ad.ActivityScreen#0\n"
                    + "packageName = \n"
                    + "totalFrames = 2352\n"
                    + "droppedFrames = 0\n"
                    + "averageFPS = 59.999\n"
                    + "present2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0\n"
                    + "latch2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0\n"
                    + "desired2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0\n"
                    + "acquire2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0\n"
                    + "post2present histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0\n"
                    + "post2acquire histogram is as below:\n"
                    + "0ms=0 1ms=0 2ms=0 3ms=0 4ms=0 5ms=0 6ms=0 7ms=0 8ms=2352 9ms=0";

    private static final String LOG_TAG = SfStatsCollectionHelperTest.class.getSimpleName();

    private @Mock UiDevice mUiDevice;
    private SfStatsCollectionHelper mHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHelper = Mockito.spy(new SfStatsCollectionHelper());
        when(mHelper.getDevice()).thenReturn(mUiDevice);
    }

    /** Test track a single, valid package. */
    @Test
    public void testCollect_valuesMatch() throws Exception {
        mockDumpCommand();
        mockEnableAndClearCommand();
        mockDisableAndClearCommand();
        mHelper.startCollecting();
        Map<String, Double> metrics = mHelper.getMetrics();
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "statsStart".toUpperCase())))
                .isEqualTo(Double.valueOf(1566336030));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "statsEnd".toUpperCase())))
                .isEqualTo(Double.valueOf(1566338516));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "totalFrames".toUpperCase())))
                .isEqualTo(Double.valueOf(5791));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "missedFrames".toUpperCase())))
                .isEqualTo(Double.valueOf(82));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "clientCompositionFrames".toUpperCase())))
                .isEqualTo(Double.valueOf(0));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "displayOnTime".toUpperCase())))
                .isEqualTo(Double.valueOf(2485421));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "totalP2PTime".toUpperCase())))
                .isEqualTo(Double.valueOf(2674034));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "FRAME_CPU_DURATION_AVG")))
                .isEqualTo(Double.valueOf(5.5));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "GLOBAL",
                                        "RENDER_ENGINE_DURATION_AVG")))
                .isEqualTo(Double.valueOf(6.5));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.breel.wallpapers19.DoodleWallpaperV1#0",
                                        "TOTAL_FRAMES")))
                .isEqualTo(Double.valueOf(5448));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.breel.wallpapers19.DoodleWallpaperV1#0",
                                        "DROPPED_FRAMES")))
                .isEqualTo(Double.valueOf(4));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.breel.wallpapers19.DoodleWallpaperV1#0",
                                        "AVERAGE_FPS")))
                .isEqualTo(2.038);
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.google.android.nexuslauncher.NexusLauncherActivity#0",
                                        "TOTAL_FRAMES")))
                .isEqualTo(Double.valueOf(264));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.google.android.nexuslauncher.NexusLauncherActivity#0",
                                        "DROPPED_FRAMES")))
                .isEqualTo(Double.valueOf(7));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "com.google.android.nexuslauncher.NexusLauncherActivity#0",
                                        "AVERAGE_FPS")))
                .isEqualTo(84.318);
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "SurfaceView - com.mxtech.videoplayer.ad/com.mxtech.videoplayer.ad.ActivityScreen#0",
                                        "TOTAL_FRAMES")))
                .isEqualTo(Double.valueOf(2352));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "SurfaceView - com.mxtech.videoplayer.ad/com.mxtech.videoplayer.ad.ActivityScreen#0",
                                        "DROPPED_FRAMES")))
                .isEqualTo(Double.valueOf(0));
        assertThat(
                        metrics.get(
                                constructKey(
                                        SFSTATS_METRICS_PREFIX,
                                        "SurfaceView - com.mxtech.videoplayer.ad/com.mxtech.videoplayer.ad.ActivityScreen#0",
                                        "AVERAGE_FPS")))
                .isEqualTo(59.999);
        mHelper.stopCollecting();
    }

    private void mockEnableAndClearCommand() throws IOException {
        when(mUiDevice.executeShellCommand(SFSTATS_COMMAND_ENABLE_AND_CLEAR)).thenReturn("");
    }

    private void mockDumpCommand() throws IOException {
        when(mUiDevice.executeShellCommand(SFSTATS_COMMAND_DUMP)).thenReturn(SFSTATS_DUMP);
    }

    private void mockDisableAndClearCommand() throws IOException {
        when(mUiDevice.executeShellCommand(SFSTATS_COMMAND_DISABLE_AND_CLEAR)).thenReturn("");
    }
}
