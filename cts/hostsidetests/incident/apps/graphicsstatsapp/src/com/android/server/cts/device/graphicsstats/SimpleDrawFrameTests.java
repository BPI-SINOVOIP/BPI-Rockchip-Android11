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
package com.android.server.cts.device.graphicsstats;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.google.common.collect.Range;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Used by GraphicsStatsTest.
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class SimpleDrawFrameTests {
    private static final String TAG = "GraphicsStatsDeviceTest";

    @Rule
    public ActivityTestRule<DrawFramesActivity> mActivityRule =
            new ActivityTestRule<>(DrawFramesActivity.class);

    void runTest(final int frameCount) throws Throwable {
        runTest(new int[frameCount]);
    }

    void runTest(final int[] framesToDraw) throws Throwable {
        DrawFramesActivity activity = mActivityRule.getActivity();
        activity.waitForReady();
        int initialFrames = activity.getRenderedFramesCount();
        assertThat(initialFrames).isLessThan(5);
        assertThat(activity.getDroppedReportsCount()).isEqualTo(0);
        activity.drawFrames(framesToDraw);
        final int expectedFrameCount = initialFrames + framesToDraw.length;
        assertThat(activity.getRenderedFramesCount()).isIn(
                Range.closedOpen(expectedFrameCount, expectedFrameCount + 5));
        assertThat(activity.getDroppedReportsCount()).isEqualTo(0);
    }

    @Test
    public void testNothing() throws Throwable {
        DrawFramesActivity activity = mActivityRule.getActivity();
        activity.waitForReady();
        activity.drawFrames(new int[10]);
    }

    @Test
    public void testDrawTenFrames() throws Throwable {
        runTest(10);
    }

    @Test
    public void testDrawJankyFrames() throws Throwable {
        int[] frames = new int[50];
        for (int i = 0; i < 10; i++) {
            int indx = i * 5;
            frames[indx] = DrawFramesActivity.FRAME_JANK_RECORD_DRAW;
            frames[indx + 1] = DrawFramesActivity.FRAME_JANK_ANIMATION;
            frames[indx + 2] = DrawFramesActivity.FRAME_JANK_LAYOUT;
            frames[indx + 3] = DrawFramesActivity.FRAME_JANK_MISS_VSYNC;
        }
        runTest(frames);
    }

    @Test
    public void testDrawDaveyFrames() throws Throwable {
        int[] frames = new int[40];
        for (int i = 0; i < 10; i++) {
            int indx = i * 4;
            frames[indx] = DrawFramesActivity.FRAME_JANK_DAVEY;
            frames[indx + 2] = DrawFramesActivity.FRAME_JANK_DAVEY_JR;
        }
        runTest(frames);
    }
}
