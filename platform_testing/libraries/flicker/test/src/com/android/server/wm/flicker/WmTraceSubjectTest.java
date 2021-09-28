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

package com.android.server.wm.flicker;

import static com.android.server.wm.flicker.TestFileUtils.readTestFile;
import static com.android.server.wm.flicker.WmTraceSubject.assertThat;

import androidx.test.runner.AndroidJUnit4;

import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

/**
 * Contains {@link WmTraceSubject} tests. To run this test: {@code atest
 * FlickerLibTest:WmTraceSubjectTest}
 */
@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class WmTraceSubjectTest {
    private static WindowManagerTrace readWmTraceFromFile(String relativePath) {
        try {
            return WindowManagerTrace.parseFrom(readTestFile(relativePath));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void testCanTransitionInAppWindow() {
        WindowManagerTrace trace = readWmTraceFromFile("wm_trace_openchrome2.pb");

        assertThat(trace)
                .showsAppWindowOnTop(
                        "com.google.android.apps.nexuslauncher/" + ".NexusLauncherActivity")
                .forRange(174684850717208L, 174685957511016L);
        assertThat(trace)
                .showsAppWindowOnTop("com.google.android.apps.nexuslauncher/.NexusLauncherActivity")
                .then()
                .showsAppWindowOnTop("com.android.chrome")
                .forAllEntries();
    }
}
