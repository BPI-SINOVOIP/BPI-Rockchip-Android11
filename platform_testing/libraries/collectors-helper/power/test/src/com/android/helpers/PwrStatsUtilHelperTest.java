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

import android.support.test.uiautomator.UiDevice;


import java.io.IOException;
import java.util.Map;

import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doReturn;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.MockitoAnnotations;

@RunWith(JUnit4.class)
public final class PwrStatsUtilHelperTest {
    private @Spy PwrStatsUtilHelper mHelper;
    private @Mock UiDevice mUiDevice;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void successfulRun() {
        final String pid_output = "pid = 11386\n";
        final String metric_output =
                "elapsed time: 14.8746s\n"
                        + "SLPI__Sleep=14505\n"
                        + "SLPI_ISLAND__uImage=14498\n"
                        + "SoC__CXSD=0\n"
                        + "SoC__AOSD=0\n";
        try {
            doReturn(metric_output).when(mHelper).executeShellCommand(matches("kill -INT \\d+"));
            doReturn(pid_output).when(mHelper).executeShellCommand(matches("pwrstats_util -d .*"));
        } catch (IOException e) {
            assertTrue(e.toString(), false);
        }

        // Validate startCollecting()
        assertTrue(mHelper.startCollecting());
        assertNotNull(mHelper.mLogFile);
        assertTrue(
                "Expecting PID " + mHelper.mUtilPid + " to be greater than 0",
                (mHelper.mUtilPid > 0));

        // Validate getMetrics()
        Map<String, Long> metrics = mHelper.getMetrics();
        assertEquals(metrics.size(), 4);

        // Validate that stopCollecting() works when run after getMetrics()
        assertTrue(mHelper.stopCollecting());
    }
}
