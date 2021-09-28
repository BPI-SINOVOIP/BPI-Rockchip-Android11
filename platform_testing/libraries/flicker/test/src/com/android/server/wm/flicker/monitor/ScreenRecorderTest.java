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

package com.android.server.wm.flicker.monitor;

import static android.os.SystemClock.sleep;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import java.io.File;
import java.nio.file.Path;

/**
 * Contains {@link ScreenRecorder} tests. To run this test: {@code atest
 * FlickerLibTest:ScreenRecorderTest}
 */
@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class ScreenRecorderTest {
    private static final String TEST_VIDEO_FILENAME = "test.mp4";
    private ScreenRecorder mScreenRecorder;
    private Path mSavedVideoPath = null;

    @Before
    public void setup() {
        mScreenRecorder = new ScreenRecorder();
    }

    @After
    public void teardown() {
        mScreenRecorder.getPath().toFile().delete();
        if (mSavedVideoPath != null) {
            mSavedVideoPath.toFile().delete();
        }
    }

    @Test
    public void videoIsRecorded() {
        mScreenRecorder.start();
        sleep(100);
        mScreenRecorder.stop();
        File file = mScreenRecorder.getPath().toFile();
        assertThat(file.exists()).isTrue();
    }

    @Test
    public void videoCanBeSaved() {
        mScreenRecorder.start();
        sleep(100);
        mScreenRecorder.stop();
        File file = mScreenRecorder.save(TEST_VIDEO_FILENAME).toFile();
        assertThat(file.exists()).isTrue();
    }
}
