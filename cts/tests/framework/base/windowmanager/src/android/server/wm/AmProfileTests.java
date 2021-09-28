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

package android.server.wm;

import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.profileable.Components.PROFILEABLE_APP_ACTIVITY;
import static android.server.wm.profileable.Components.ProfileableAppActivity.COMMAND_WAIT_FOR_PROFILE_OUTPUT;
import static android.server.wm.profileable.Components.ProfileableAppActivity.OUTPUT_FILE_PATH;
import static android.server.wm.profileable.Components.ProfileableAppActivity.OUTPUT_DIR;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertEquals;

import android.content.ComponentName;
import android.content.Intent;
import android.platform.test.annotations.Presubmit;
import android.server.wm.CommandSession.ActivitySession;
import android.server.wm.CommandSession.DefaultLaunchProxy;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;

/**
 * Build/Install/Run:
 *     atest CtsWindowManagerDeviceTestCases:AmProfileTests
 *
 * Please talk to Android Studio team first if you want to modify or delete these tests.
 */
@Presubmit
public class AmProfileTests extends ActivityManagerTestBase {

    private static final String FIRST_WORD_NO_STREAMING = "*version\n";
    private static final String FIRST_WORD_STREAMING = "SLOW";  // Magic word set by runtime.

    @BeforeClass
    public static void setUpClass() {
        // Allow ProfileableAppActivity to monitor the path.
        executeShellCommand("mkdir -m 777 -p " + OUTPUT_DIR);
    }

    @AfterClass
    public static void tearDownClass() {
        executeShellCommand("rm -rf " + OUTPUT_DIR);
    }

    /**
     * Test am profile functionality with the following 3 configurable options:
     *    starting the activity before start profiling? yes;
     *    sampling-based profiling? no;
     *    using streaming output mode? no.
     */
    @Test
    public void testAmProfileStartNoSamplingNoStreaming() throws Exception {
        // am profile start ... , and the same to the following 3 test methods.
        testProfile(true, false, false);
    }

    /**
     * The following tests are similar to testAmProfileStartNoSamplingNoStreaming(),
     * only different in the three configuration options.
     */
    @Test
    public void testAmProfileStartNoSamplingStreaming() throws Exception {
        testProfile(true, false, true);
    }

    @Test
    public void testAmProfileStartSamplingNoStreaming() throws Exception {
        testProfile(true, true, false);
    }

    @Test
    public void testAmProfileStartSamplingStreaming() throws Exception {
        testProfile(true, true, true);
    }

    @Test
    public void testAmStartStartProfilerNoSamplingNoStreaming() throws Exception {
        // am start --start-profiler ..., and the same to the following 3 test methods.
        testProfile(false, false, false);
    }

    @Test
    public void testAmStartStartProfilerNoSamplingStreaming() throws Exception {
        testProfile(false, false, true);
    }

    @Test
    public void testAmStartStartProfilerSamplingNoStreaming() throws Exception {
        testProfile(false, true, false);
    }

    @Test
    public void testAmStartStartProfilerSamplingStreaming() throws Exception {
        testProfile(false, true, true);
    }

    private void testProfile(final boolean startActivityFirst, final boolean sampling,
            final boolean streaming) throws Exception {
        final ActivitySession activitySession;
        if (startActivityFirst) {
            activitySession = createManagedActivityClientSession().startActivity(
                    new Intent().setComponent(PROFILEABLE_APP_ACTIVITY));
            startProfiling(PROFILEABLE_APP_ACTIVITY.getPackageName(), sampling, streaming);
        } else {
            activitySession = startActivityProfiling(PROFILEABLE_APP_ACTIVITY, sampling, streaming);
        }

        // Go to home screen and then warm start the activity to generate some interesting trace.
        launchHomeActivity();
        launchActivity(PROFILEABLE_APP_ACTIVITY);

        executeShellCommand(getStopProfileCmd(PROFILEABLE_APP_ACTIVITY));

        activitySession.sendCommandAndWaitReply(COMMAND_WAIT_FOR_PROFILE_OUTPUT);
        verifyOutputFileFormat(streaming);
    }

    /** Starts profiler on a started process. */
    private static void startProfiling(String processName, boolean sampling, boolean streaming) {
        final StringBuilder builder = new StringBuilder("am profile start");
        appendProfileParameters(builder, sampling, streaming);
        builder.append(String.format(" %s %s", processName, OUTPUT_FILE_PATH));
        executeShellCommand(builder.toString());
    }

    /** Starts the activity with profiler. */
    private ActivitySession startActivityProfiling(ComponentName activityName, boolean sampling,
            boolean streaming) {
        return createManagedActivityClientSession().startActivity(new DefaultLaunchProxy() {

            @Override
            public boolean shouldWaitForLaunched() {
                // The shell command included "-W".
                return false;
            }

            @Override
            public void execute() {
                final StringBuilder builder = new StringBuilder();
                builder.append(String.format("am start -n %s -W -S --start-profiler %s",
                        getActivityName(activityName), OUTPUT_FILE_PATH));
                appendProfileParameters(builder, sampling, streaming);
                mLaunchInjector.setupShellCommand(builder);
                executeShellCommand(builder.toString());
            }
        });
    }

    private static void appendProfileParameters(StringBuilder builder, boolean sampling,
            boolean streaming) {
        if (sampling) {
            builder.append(" --sampling 1000");
        }
        if (streaming) {
            builder.append(" --streaming");
        }
    }

    private static String getStopProfileCmd(final ComponentName activityName) {
        return "am profile stop " + activityName.getPackageName();
    }

    private void verifyOutputFileFormat(final boolean streaming) throws Exception {
        // This is a hack. The am service has to write to /data/local/tmp because it doesn't have
        // access to the sdcard. The test cannot read from /data/local/tmp. This allows us to
        // scan the content to validate what is needed for this test.
        final String firstLine = executeShellCommand("head -1 " + OUTPUT_FILE_PATH);

        final String expectedFirstWord = streaming ? FIRST_WORD_STREAMING : FIRST_WORD_NO_STREAMING;
        assertThat(
                "data size", firstLine.length(), greaterThanOrEqualTo(expectedFirstWord.length()));
        final String actualFirstWord = firstLine.substring(0, expectedFirstWord.length());
        assertEquals("Unexpected first word", expectedFirstWord, actualFirstWord);

        // Clean up.
        executeShellCommand("rm -f " + OUTPUT_FILE_PATH);
    }
}
