/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.server.cts;

import static com.google.common.truth.Truth.assertThat;

import android.service.GraphicsStatsHistogramBucketProto;
import android.service.GraphicsStatsJankSummaryProto;
import android.service.GraphicsStatsProto;
import android.service.GraphicsStatsServiceDumpProto;

import com.google.common.collect.Range;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

public class GraphicsStatsValidationTest extends ProtoDumpTestCase {
    private static final String TAG = "GraphicsStatsValidationTest";

    private static final String DEVICE_SIDE_TEST_APK = "CtsGraphicsStatsApp.apk";
    private static final String DEVICE_SIDE_TEST_PACKAGE
            = "com.android.server.cts.device.graphicsstats";

    @Override
    protected void tearDown() throws Exception {
        getDevice().uninstallPackage(DEVICE_SIDE_TEST_PACKAGE);
        super.tearDown();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        installPackage(DEVICE_SIDE_TEST_APK, /* grantPermissions= */ true);
        turnScreenOn();
        // Ensure that we have a starting point for our stats
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".SimpleDrawFrameTests",
                "testNothing");
        // Kill to ensure that stats persist/merge across process death
        killTestApp();
    }

    private void turnScreenOn() throws Exception {
        getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
        getDevice().executeShellCommand("wm dismiss-keyguard");
    }

    public void testBasicDrawFrame() throws Exception {
        System.out.println("--------------------------------- testBasicDrawFrame BEGIN");
        GraphicsStatsProto[] results = runDrawTest("testDrawTenFrames");
        GraphicsStatsProto statsBefore = results[0];
        GraphicsStatsProto statsAfter = results[1];
        GraphicsStatsJankSummaryProto summaryBefore = statsBefore.getSummary();
        GraphicsStatsJankSummaryProto summaryAfter = statsAfter.getSummary();
        assertTrue(summaryAfter.getTotalFrames() > summaryBefore.getTotalFrames());

        System.out.println("summaryBefore: {\n" + summaryBefore + "}");
        System.out.println("summaryAfter: {\n" + summaryAfter + "}");
        System.out.println("statsBefore: {\n" + statsBefore + "}");
        System.out.println("statsAfter: {\n" + statsAfter + "}");

        int frameDelta = summaryAfter.getTotalFrames() - summaryBefore.getTotalFrames();
        int jankyDelta = summaryAfter.getJankyFrames() - summaryBefore.getJankyFrames();
        // We expect 11 frames to have been drawn (first frame + the 10 more explicitly requested)
        assertTrue(frameDelta >= 11);
        assertTrue(jankyDelta >= 1);
        int veryJankyDelta = countFramesAbove(statsAfter, 40) - countFramesAbove(statsBefore, 40);
        System.out.println("--------------------------------- testBasicDrawFrame END");
    }

    public void testJankyDrawFrame() throws Exception {
        GraphicsStatsProto[] results = runDrawTest("testDrawJankyFrames");
        GraphicsStatsProto statsBefore = results[0];
        GraphicsStatsProto statsAfter = results[1];
        GraphicsStatsJankSummaryProto summaryBefore = statsBefore.getSummary();
        GraphicsStatsJankSummaryProto summaryAfter = statsAfter.getSummary();
        assertTrue(summaryAfter.getTotalFrames() > summaryBefore.getTotalFrames());

        int frameDelta = summaryAfter.getTotalFrames() - summaryBefore.getTotalFrames();
        int jankyDelta = summaryAfter.getJankyFrames() - summaryBefore.getJankyFrames();
        // Test draws 50 frames + 1 initial frame. We expect 40 of them to be janky,
        // 10 of each of ANIMATION, LAYOUT, RECORD_DRAW, and MISSED_VSYNC

        assertThat(frameDelta).isAtLeast(50);
        assertThat(jankyDelta).isAtLeast(40);
        assertThat(jankyDelta).isLessThan(45);

        // Although our current stats don't distinguish between ANIMATION, LAYOUT, and RECORD_DRAW
        // so this will just be slowUi +30
        int slowUiDelta = summaryAfter.getSlowUiThreadCount() - summaryBefore.getSlowUiThreadCount();
        assertThat(slowUiDelta).isAtLeast(20);
        int missedVsyncDelta = summaryAfter.getMissedVsyncCount()
                - summaryBefore.getMissedVsyncCount();
        assertThat(missedVsyncDelta).isIn(Range.closed(10, 11));

        int veryJankyDelta = countFramesAbove(statsAfter, 60) - countFramesAbove(statsBefore, 60);
        // The 1st frame could be >40ms, but nothing after that should be
        assertThat(veryJankyDelta).isAtMost(2);
        int noGPUJank = countGPUFramesAbove(statsAfter, 60) - countGPUFramesAbove(statsBefore, 60);
        assertThat(noGPUJank).isEqualTo(0);
    }

    public void testDaveyDrawFrame() throws Exception {
        GraphicsStatsProto[] results = runDrawTest("testDrawDaveyFrames");
        GraphicsStatsProto statsBefore = results[0];
        GraphicsStatsProto statsAfter = results[1];
        GraphicsStatsJankSummaryProto summaryBefore = statsBefore.getSummary();
        GraphicsStatsJankSummaryProto summaryAfter = statsAfter.getSummary();
        assertTrue(summaryAfter.getTotalFrames() > summaryBefore.getTotalFrames());

        int frameDelta = summaryAfter.getTotalFrames() - summaryBefore.getTotalFrames();
        int jankyDelta = summaryAfter.getJankyFrames() - summaryBefore.getJankyFrames();
        // Test draws 40 frames + 1 initial frame. We expect 10 of them to be daveys,
        // 10 of them to be daveyjrs, and 20 to jank from missed vsync (from the davey/daveyjr prior to it)
        assertThat(frameDelta).isAtLeast(40);
        assertThat(jankyDelta).isAtLeast(20);
        assertThat(jankyDelta).isLessThan(25);

        int gt150msDelta = countFramesAbove(statsAfter, 150) - countFramesAbove(statsBefore, 150);
        assertThat(gt150msDelta).isIn(Range.closed(20, 22));
        int gt700msDelta = countFramesAbove(statsAfter, 700) - countFramesAbove(statsBefore, 700);
        assertEquals(10, gt700msDelta); // 10 daveys
    }

    private GraphicsStatsProto[] runDrawTest(String testName)  throws Exception  {
        return doRunDrawTest(testName, true);
    }

    private GraphicsStatsProto[] doRunDrawTest(String testName, boolean canRetry) throws Exception {
        killTestApp();
        turnScreenOn();
        GraphicsStatsProto statsBefore = fetchStats();
        assertNotNull(statsBefore);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".SimpleDrawFrameTests",  testName);
        killTestApp();
        GraphicsStatsProto statsAfter = fetchStats();
        assertNotNull(statsAfter);
        // If we get extremely unlucky a log rotate might have happened. If so we retry, but only once
        // It's a failure if this test takes >24 hours such that 2 rotates could happen while running
        // this test case, or more likely if stats are not being merged/persisted properly
        if (canRetry) {
            if (statsBefore.getStatsStart() != statsAfter.getStatsStart()) {
                return doRunDrawTest(testName, false);
            }
        } else {
            assertEquals(statsBefore.getStatsStart(), statsAfter.getStatsStart());
        }
        validate(statsBefore);
        validate(statsAfter);
        return new GraphicsStatsProto[] { statsBefore, statsAfter };
    }

    private void validate(GraphicsStatsProto proto) {
        assertNotNull(proto.getPackageName());
        assertFalse(proto.getPackageName().isEmpty());
        assertTrue(proto.getVersionCode() > 0);
        assertTrue(proto.getStatsStart() > 0);
        assertTrue(proto.getStatsEnd() > 0);
        assertTrue(proto.hasSummary());
        GraphicsStatsJankSummaryProto summary = proto.getSummary();
        assertTrue(summary.getTotalFrames() > 0);
        // Our test app won't produce that many frames, so we can assert this is a realistic
        // number. We cap it at 1,000,000 in case the test is repeated many, many times in one day
        assertTrue(summary.getTotalFrames() < 1000000);
        // We can't generically assert things about the janky frames, so just assert they fall into
        // valid ranges.
        assertTrue(summary.getJankyFrames() <= summary.getTotalFrames());
        assertTrue(summary.getMissedVsyncCount() <= summary.getJankyFrames());
        assertTrue(summary.getHighInputLatencyCount() <= summary.getTotalFrames());
        assertTrue(summary.getSlowUiThreadCount() <= summary.getJankyFrames());
        assertTrue(summary.getSlowBitmapUploadCount() <= summary.getJankyFrames());
        assertTrue(summary.getSlowDrawCount() <= summary.getJankyFrames());
        assertTrue(proto.getHistogramCount() > 0);
        assertTrue(proto.getGpuHistogramCount() > 0);
        assertTrue(proto.getPipeline() == GraphicsStatsProto.PipelineType.GL
            || proto.getPipeline() == GraphicsStatsProto.PipelineType.VULKAN);

        int histogramTotal = countTotalFrames(proto);
        assertEquals(summary.getTotalFrames(), histogramTotal);
    }

    private int countFramesAbove(GraphicsStatsProto proto, int thresholdMs) {
        int totalFrames = 0;
        for (GraphicsStatsHistogramBucketProto bucket : proto.getHistogramList()) {
            if (bucket.getRenderMillis() >= thresholdMs) {
                totalFrames += bucket.getFrameCount();
            }
        }
        return totalFrames;
    }

    private int countGPUFramesAbove(GraphicsStatsProto proto, int thresholdMs) {
        int totalFrames = 0;
        for (GraphicsStatsHistogramBucketProto bucket : proto.getGpuHistogramList()) {
            if (bucket.getRenderMillis() >= thresholdMs) {
                totalFrames += bucket.getFrameCount();
            }
        }
        return totalFrames;
    }

    private int countTotalFrames(GraphicsStatsProto proto) {
        return countFramesAbove(proto, 0);
    }

    private void killTestApp() throws Exception {
        getDevice().executeShellCommand("am kill " + DEVICE_SIDE_TEST_PACKAGE);
    }

    private GraphicsStatsProto fetchStats() throws Exception {
        GraphicsStatsServiceDumpProto serviceDumpProto = getDump(GraphicsStatsServiceDumpProto.parser(),
                "dumpsys graphicsstats --proto");
        List<GraphicsStatsProto> protos = filterPackage(serviceDumpProto, DEVICE_SIDE_TEST_PACKAGE);
        return findLatest(protos);
    }

    private List<GraphicsStatsProto> filterPackage(GraphicsStatsServiceDumpProto dump, String pkgName) {
        return filterPackage(dump.getStatsList(), pkgName);
    }

    private List<GraphicsStatsProto> filterPackage(List<GraphicsStatsProto> list, String pkgName) {
        ArrayList<GraphicsStatsProto> filtered = new ArrayList<>();
        for (GraphicsStatsProto proto : list) {
            if (pkgName.equals(proto.getPackageName())) {
                filtered.add(proto);
            }
        }
        return filtered;
    }

    private GraphicsStatsProto findLatest(List<GraphicsStatsProto> list) {
        if (list.size() == 0) { return null; }
        GraphicsStatsProto latest = list.get(0);
        Date latestDate = new Date();
        Date compareTo = new Date();
        latestDate.setTime(latest.getStatsEnd());
        for (int i = 1; i < list.size(); i++) {
            GraphicsStatsProto proto = list.get(i);
            compareTo.setTime(proto.getStatsEnd());
            if (compareTo.after(latestDate)) {
                latestDate.setTime(proto.getStatsEnd());
                latest = proto;
            }
        }
        return latest;
    }
}
