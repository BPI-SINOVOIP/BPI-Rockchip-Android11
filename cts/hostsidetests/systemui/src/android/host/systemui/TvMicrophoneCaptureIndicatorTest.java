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

package android.host.systemui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import com.android.server.wm.ActivityRecordProto;
import com.android.server.wm.DisplayAreaProto;
import com.android.server.wm.DisplayContentProto;
import com.android.server.wm.RootWindowContainerProto;
import com.android.server.wm.TaskProto;
import com.android.server.wm.WindowContainerChildProto;
import com.android.server.wm.WindowContainerProto;
import com.android.server.wm.WindowManagerServiceDumpProto;
import com.android.server.wm.WindowStateProto;
import com.android.server.wm.WindowTokenProto;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@Ignore
@RunWith(DeviceJUnit4ClassRunner.class)
public class TvMicrophoneCaptureIndicatorTest extends BaseHostJUnit4Test {
    private static final String SHELL_AM_START_FG_SERVICE =
            "am start-foreground-service -n %s -a %s";
    private static final String SHELL_AM_FORCE_STOP =
            "am force-stop %s";
    private static final String SHELL_DUMPSYS_WINDOW = "dumpsys window --proto";
    private static final String SHELL_PID_OF = "pidof %s";

    private static final String FEATURE_LEANBACK_ONLY = "android.software.leanback_only";

    private static final String AUDIO_RECORDER_AR_PACKAGE_NAME =
            "android.systemui.cts.audiorecorder.audiorecord";
    private static final String AUDIO_RECORDER_MR_PACKAGE_NAME =
            "android.systemui.cts.audiorecorder.mediarecorder";
    private static final String AUDIO_RECORDER_AR_SERVICE_COMPONENT =
            AUDIO_RECORDER_AR_PACKAGE_NAME + "/.AudioRecorderService";
    private static final String AUDIO_RECORDER_MR_SERVICE_COMPONENT =
            AUDIO_RECORDER_MR_PACKAGE_NAME + "/.AudioRecorderService";
    private static final String AUDIO_RECORDER_ACTION_START =
            "android.systemui.cts.audiorecorder.ACTION_START";
    private static final String AUDIO_RECORDER_ACTION_STOP =
            "android.systemui.cts.audiorecorder.ACTION_STOP";
    private static final String AUDIO_RECORDER_ACTION_THROW =
            "android.systemui.cts.audiorecorder.ACTION_THROW";

    private static final String SHELL_AR_START_REC = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_AR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_START);
    private static final String SHELL_AR_STOP_REC = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_AR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_STOP);
    private static final String SHELL_MR_START_REC = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_MR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_START);
    private static final String SHELL_MR_STOP_REC = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_MR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_STOP);
    private static final String SHELL_AR_FORCE_STOP = String.format(SHELL_AM_FORCE_STOP,
            AUDIO_RECORDER_AR_PACKAGE_NAME);
    private static final String SHELL_MR_FORCE_STOP = String.format(SHELL_AM_FORCE_STOP,
            AUDIO_RECORDER_MR_PACKAGE_NAME);
    private static final String SHELL_AR_THROW = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_AR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_THROW);
    private static final String SHELL_MR_THROW = String.format(SHELL_AM_START_FG_SERVICE,
            AUDIO_RECORDER_MR_SERVICE_COMPONENT,
            AUDIO_RECORDER_ACTION_THROW);

    private static final String WINDOW_TITLE_MIC_INDICATOR = "MicrophoneCaptureIndicator";

    private static final long ONE_SECOND = 1000L;
    private static final long THREE_SECONDS = 3 * ONE_SECOND;
    private static final long FIVE_SECONDS = 5 * ONE_SECOND;
    private static final long THREE_HUNDRED_MILLISECONDS = (long) (0.3 * ONE_SECOND);

    @Test
    public void testIndicatorShownWhileRecordingUsingAudioRecordApi() throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_AR_PACKAGE_NAME, SHELL_AR_START_REC,
                SHELL_AR_STOP_REC);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingMediaRecorderApi() throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_MR_PACKAGE_NAME, SHELL_MR_START_REC,
                SHELL_MR_STOP_REC);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingAudioRecordApiAndForceStopped()
            throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_AR_PACKAGE_NAME, SHELL_AR_START_REC,
                SHELL_AR_FORCE_STOP);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingMediaRecorderApiAndForceStopped()
            throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_MR_PACKAGE_NAME, SHELL_MR_START_REC,
                SHELL_MR_FORCE_STOP);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingAudioRecordApiAndCrashed() throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_AR_PACKAGE_NAME, SHELL_AR_START_REC,
                SHELL_AR_THROW);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingMediaRecorderApiAndCrashed() throws Exception {
        runSimpleStartStopTestRoutine(AUDIO_RECORDER_MR_PACKAGE_NAME, SHELL_MR_START_REC,
                SHELL_MR_THROW);
    }

    @Test
    public void testIndicatorShownWhileRecordingUsingBothApisSimultaneously() throws Exception {
        assumeTrue("Not running on a Leanback (TV) device",
                getDevice().hasFeature(FEATURE_LEANBACK_ONLY));

        // Check that the indicator isn't shown initially
        assertIndicatorInvisible();

        // Start recording using MediaRecorder API
        getDevice().executeShellCommand(SHELL_MR_START_REC);

        // Wait for the application to be launched
        waitForProcessToComeAlive(AUDIO_RECORDER_MR_PACKAGE_NAME);

        // Wait for a second, and then check that the indicator is shown
        Thread.sleep(ONE_SECOND);
        assertIndicatorVisible();

        // Start recording using AudioRecord API
        getDevice().executeShellCommand(SHELL_AR_START_REC);

        // Wait for the application to be launched
        waitForProcessToComeAlive(AUDIO_RECORDER_AR_PACKAGE_NAME);

        // Check that the indicator is still shown
        assertIndicatorVisible();

        // Check 3 more times that the indicator remains shown
        for (int i = 0; i < 3; i++) {
            Thread.sleep(ONE_SECOND);
            assertIndicatorVisible();
        }

        // Stop recording using MediaRecorder API
        getDevice().executeShellCommand(SHELL_MR_STOP_REC);

        // check that the indicator is still shown
        assertIndicatorVisible();

        // Check 3 more times that the indicator remains shown
        for (int i = 0; i < 3; i++) {
            Thread.sleep(ONE_SECOND);
            assertIndicatorVisible();
        }

        // Stop recording using AudioRecord API
        getDevice().executeShellCommand(SHELL_AR_STOP_REC);

        // Wait for five seconds and make sure that the indicator is not shown
        Thread.sleep(FIVE_SECONDS);
        assertIndicatorInvisible();
    }

    @After
    public void tearDown() throws Exception {
        // Kill both apps
        getDevice().executeShellCommand(SHELL_AR_FORCE_STOP);
        getDevice().executeShellCommand(SHELL_MR_FORCE_STOP);
    }

    private void runSimpleStartStopTestRoutine(String packageName, String startCommand,
            String stopCommand) throws Exception {
        assumeTrue("Not running on a Leanback (TV) device",
                getDevice().hasFeature(FEATURE_LEANBACK_ONLY));

        // Check that the indicator isn't shown initially
        assertIndicatorInvisible();

        // Start recording using AudioRecord API
        getDevice().executeShellCommand(startCommand);

        // Wait for the application to be launched
        waitForProcessToComeAlive(packageName);

        // Wait for a second, and then check that the indicator is shown, repeat 2 more times
        for (int i = 0; i < 3; i++) {
            Thread.sleep(ONE_SECOND);
            assertIndicatorVisible();
        }

        // Stop recording (this may either send a command to the app to stop recording or command
        // to crash or force-stop the app)
        getDevice().executeShellCommand(stopCommand);

        // Wait for five seconds and make sure that the indicator is not shown
        Thread.sleep(FIVE_SECONDS);
        assertIndicatorInvisible();
    }

    private void waitForProcessToComeAlive(String appPackageName) throws Exception {
        final String pidofCommand = String.format(SHELL_PID_OF, appPackageName);

        long waitTime = 0;
        while (waitTime < THREE_SECONDS) {
            Thread.sleep(THREE_HUNDRED_MILLISECONDS);

            final String pid = getDevice().executeShellCommand(pidofCommand).trim();
            if (!pid.isEmpty()) {
                // Process is running
                return;
            }
            waitTime += THREE_HUNDRED_MILLISECONDS;
        }

        fail("The process for " + appPackageName
                + " should have come alive within 3 secs of launching the app.");
    }

    private void assertIndicatorVisible() throws Exception {
        final WindowStateProto window = getMicCaptureIndicatorWindow();

        assertNotNull("\"MicrophoneCaptureIndicator\" window does not exist", window);
        assertTrue("\"MicrophoneCaptureIndicator\" window is not visible",
                window.getIsVisible());
        assertTrue("\"MicrophoneCaptureIndicator\" window is not on screen",
                window.getIsOnScreen());
    }

    private void assertIndicatorInvisible() throws Exception {
        final WindowStateProto window = getMicCaptureIndicatorWindow();
        if (window == null) {
            // If window is not present, that's fine, there is no need to check anything else.
            return;
        }

        assertFalse("\"MicrophoneCaptureIndicator\" window shouldn't be visible",
                window.getIsVisible());
        assertFalse("\"MicrophoneCaptureIndicator\" window shouldn't be present on screen",
                window.getIsOnScreen());
    }

    private WindowStateProto getMicCaptureIndicatorWindow() throws Exception {
        final WindowManagerServiceDumpProto dump = getDump();
        final RootWindowContainerProto rootWindowContainer = dump.getRootWindowContainer();
        final WindowContainerProto windowContainer = rootWindowContainer.getWindowContainer();

        final List<WindowStateProto> windows = new ArrayList<>();
        collectWindowStates(windowContainer, windows);

        for (WindowStateProto window : windows) {
            final String title = window.getIdentifier().getTitle();
            if (WINDOW_TITLE_MIC_INDICATOR.equals(title)) {
                return window;
            }
        }
        return null;
    }

    private WindowManagerServiceDumpProto getDump() throws Exception {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        getDevice().executeShellCommand(SHELL_DUMPSYS_WINDOW, receiver);
        return WindowManagerServiceDumpProto.parser().parseFrom(receiver.getOutput());
    }

    /**
     * This methods implements a DFS that goes through a tree of window containers and collects all
     * the WindowStateProto-s.
     *
     * WindowContainer is generic class that can hold windows directly or through its children in a
     * hierarchy form. WindowContainer's children are WindowContainer as well. This forms a tree of
     * WindowContainers.
     *
     * There are a few classes that extend WindowContainer: Task, DisplayContent, WindowToken etc.
     * The one we are interested in is WindowState.
     * Since Proto does not have concept of inheritance, {@link TaskProto}, {@link WindowTokenProto}
     * etc hold a reference to a {@link WindowContainerProto} (in java code would be {@code super}
     * reference).
     * {@link WindowContainerProto} may a have a number of children of type
     * {@link WindowContainerChildProto}, which represents a generic child of a WindowContainer: a
     * WindowContainer can have multiple children of different types stored as a
     * {@link WindowContainerChildProto}, but each instance of {@link WindowContainerChildProto} can
     * only contain a single type.
     *
     * For details see /frameworks/base/core/proto/android/server/windowmanagerservice.proto
     */
    private void collectWindowStates(WindowContainerProto windowContainer, List<WindowStateProto> out) {
        if (windowContainer == null) return;

        final List<WindowContainerChildProto> children = windowContainer.getChildrenList();
        for (WindowContainerChildProto child : children) {
            if (child.hasWindowContainer()) {
                collectWindowStates(child.getWindowContainer(), out);
            } else if (child.hasDisplayContent()) {
                final DisplayContentProto displayContent = child.getDisplayContent();
                for (WindowTokenProto windowToken : displayContent.getOverlayWindowsList()) {
                    collectWindowStates(windowToken.getWindowContainer(), out);
                }
                if (displayContent.hasRootDisplayArea()) {
                    final DisplayAreaProto displayArea = displayContent.getRootDisplayArea();
                    collectWindowStates(displayArea.getWindowContainer(), out);
                }
                collectWindowStates(displayContent.getWindowContainer(), out);
            } else if (child.hasDisplayArea()) {
                final DisplayAreaProto displayArea = child.getDisplayArea();
                collectWindowStates(displayArea.getWindowContainer(), out);
            } else if (child.hasTask()) {
                final TaskProto task = child.getTask();
                collectWindowStates(task.getWindowContainer(), out);
            } else if (child.hasActivity()) {
                final ActivityRecordProto activity = child.getActivity();
                if (activity.hasWindowToken()) {
                    final WindowTokenProto windowToken = activity.getWindowToken();
                    collectWindowStates(windowToken.getWindowContainer(), out);
                }
            } else if (child.hasWindowToken()) {
                final WindowTokenProto windowToken = child.getWindowToken();
                collectWindowStates(windowToken.getWindowContainer(), out);
            } else if (child.hasWindow()) {
                final WindowStateProto window = child.getWindow();
                // We found a Window!
                out.add(window);
                // ... but still aren't done
                collectWindowStates(window.getWindowContainer(), out);
            }
        }
    }
}
