/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package android.server.wm;

import static android.app.ActivityTaskManager.INVALID_STACK_ID;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_UNDEFINED;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.server.wm.ComponentNameUtils.getActivityName;
import static android.server.wm.ComponentNameUtils.getWindowName;
import static android.server.wm.StateLogger.logAlways;
import static android.server.wm.StateLogger.logE;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowManager.LayoutParams.TYPE_KEYGUARD_DIALOG;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.ComponentName;
import android.graphics.Rect;
import android.util.SparseArray;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Predicate;

/** Window Manager State helper class with assert and wait functions. */
public class WindowManagerStateHelper extends WindowManagerState {

    /**
     * Compute AM and WM state of device, check sanity and bounds.
     * WM state will include only visible windows, stack and task bounds will be compared.
     *
     * @param componentNames array of activity names to wait for.
     */
    public void computeState(ComponentName... componentNames) {
        waitForValidState(Arrays.stream(componentNames)
                .map(WaitForValidActivityState::new)
                .toArray(WaitForValidActivityState[]::new));
    }

    /**
     * Compute AM and WM state of device, check sanity and bounds.
     * WM state will include only visible windows, stack and task bounds will be compared.
     *
     * @param waitForActivitiesVisible array of activity names to wait for.
     */
    public void computeState(WaitForValidActivityState... waitForActivitiesVisible) {
        waitForValidState(waitForActivitiesVisible);
    }

    /**
     * Wait for the activities to appear and for valid state in AM and WM.
     *
     * @param activityNames name list of activities to wait for.
     */
    public void waitForValidState(ComponentName... activityNames) {
        waitForValidState(Arrays.stream(activityNames)
                .map(WaitForValidActivityState::new)
                .toArray(WaitForValidActivityState[]::new));

    }

    /**
     * Wait for the activities to appear in proper stacks and for valid state in AM and WM.
     * @param waitForActivitiesVisible  array of activity states to wait for.
     */
    void waitForValidState(WaitForValidActivityState... waitForActivitiesVisible) {
        if (!Condition.waitFor("valid stacks and activities states", () -> {
            // TODO: Get state of AM and WM at the same time to avoid mismatches caused by
            // requesting dump in some intermediate state.
            computeState();
            return !(shouldWaitForSanityCheck()
                    || shouldWaitForValidStacks()
                    || shouldWaitForActivities(waitForActivitiesVisible)
                    || shouldWaitForWindows());
        })) {
            logE("***Waiting for states failed: " + Arrays.toString(waitForActivitiesVisible));
        }
    }

    void waitForAllStoppedActivities() {
        if (!Condition.waitFor("all started activities have been removed", () -> {
            computeState();
            return !containsStartedActivities();
        })) {
            fail("All started activities have been removed");
        }
    }

    /**
     * Compute AM and WM state of device, wait for the activity records to be added, and
     * wait for debugger window to show up.
     *
     * This should only be used when starting with -D (debugger) option, where we pop up the
     * waiting-for-debugger window, but real activity window won't show up since we're waiting
     * for debugger.
     */
    void waitForDebuggerWindowVisible(ComponentName activityName) {
        Condition.waitFor("debugger window", () -> {
            computeState();
            return !shouldWaitForDebuggerWindow(activityName)
                    && !shouldWaitForActivityRecords(activityName);
        });
    }

    void waitForHomeActivityVisible() {
        ComponentName homeActivity = getHomeActivityName();
        // Sometimes this function is called before we know what Home Activity is
        if (homeActivity == null) {
            logAlways("Computing state to determine Home Activity");
            computeState();
            homeActivity = getHomeActivityName();
        }
        assertNotNull("homeActivity should not be null", homeActivity);
        waitForValidState(homeActivity);
    }

    void waitForRecentsActivityVisible() {
        if (isHomeRecentsComponent()) {
            waitForHomeActivityVisible();
        } else {
            waitForWithAmState(WindowManagerState::isRecentsActivityVisible,
                    "recents activity to be visible");
        }
    }

    void waitForKeyguardShowingAndNotOccluded() {
        waitForWithAmState(state -> state.getKeyguardControllerState().keyguardShowing
                        && !state.getKeyguardControllerState().isKeyguardOccluded(DEFAULT_DISPLAY),
                "Keyguard showing");
    }

    void waitForKeyguardShowingAndOccluded() {
        waitForWithAmState(state -> state.getKeyguardControllerState().keyguardShowing
                        && state.getKeyguardControllerState().isKeyguardOccluded(DEFAULT_DISPLAY),
                "Keyguard showing and occluded");
    }

    void waitForAodShowing() {
        waitForWithAmState(state -> state.getKeyguardControllerState().aodShowing, "AOD showing");
    }

    void waitForKeyguardGone() {
        waitForWithAmState(state -> !state.getKeyguardControllerState().keyguardShowing,
                "Keyguard gone");
    }

    void waitAndAssertKeyguardGone() {
        assertTrue("Keyguard must be gone",
                waitForWithAmState(
                        state -> !state.getKeyguardControllerState().keyguardShowing,
                        "Keyguard gone"));
    }

    /** Wait for specific rotation for the default display. Values are Surface#Rotation */
    void waitForRotation(int rotation) {
        waitForWithAmState(state -> state.getRotation() == rotation, "Rotation: " + rotation);
    }

    /**
     * Wait for specific orientation for the default display.
     * Values are ActivityInfo.ScreenOrientation
     */
    void waitForLastOrientation(int orientation) {
        waitForWithAmState(state -> state.getLastOrientation() == orientation,
                "LastOrientation: " + orientation);
    }

    void waitAndAssertLastOrientation(String message, int screenOrientation) {
        if (screenOrientation != getLastOrientation()) {
            waitForLastOrientation(screenOrientation);
        }
        assertEquals(message, screenOrientation, getLastOrientation());
    }

    /**
     * Wait for orientation for the Activity
     */
    void waitForActivityOrientation(ComponentName activityName, int orientation) {
        waitForWithAmState(amState -> {
            final ActivityTask task = amState.getTaskByActivity(activityName);
            if (task == null) {
                return false;
            }
            return task.mFullConfiguration.orientation == orientation;
        }, "orientation of " + getActivityName(activityName) + " to be " + orientation);
    }

    void waitForDisplayUnfrozen() {
        waitForWithAmState(state -> !state.isDisplayFrozen(), "Display unfrozen");
    }

    public void waitForActivityState(ComponentName activityName, String activityState) {
        waitForWithAmState(state -> state.hasActivityState(activityName, activityState),
                "state of " + getActivityName(activityName) + " to be " + activityState);
    }

    public void waitForActivityRemoved(ComponentName activityName) {
        waitFor((amState) -> !amState.containsActivity(activityName)
                && !amState.containsWindow(getWindowName(activityName)),
                getActivityName(activityName) + " to be removed");
    }

    void waitAndAssertActivityRemoved(ComponentName activityName) {
        waitForActivityRemoved(activityName);
        assertNotExist(activityName);
    }

    void waitForFocusedStack(int windowingMode, int activityType) {
        waitForWithAmState(state ->
                        (activityType == ACTIVITY_TYPE_UNDEFINED
                                || state.getFocusedStackActivityType() == activityType)
                        && (windowingMode == WINDOWING_MODE_UNDEFINED
                                || state.getFocusedStackWindowingMode() == windowingMode),
                "focused stack");
    }

    void waitForPendingActivityContain(ComponentName activity) {
        waitForWithAmState(state -> state.pendingActivityContain(activity),
                getActivityName(activity) + " in pending list");
    }

    void waitForAppTransitionIdleOnDisplay(int displayId) {
        waitForWithAmState(
                state -> WindowManagerState.APP_STATE_IDLE.equals(
                        state.getDisplay(displayId).getAppTransitionState()),
                "app transition idle on Display " + displayId);
    }

    void waitAndAssertNavBarShownOnDisplay(int displayId) {
        assertTrue(waitForWithAmState(
                state -> state.getAndAssertSingleNavBarWindowOnDisplay(displayId) != null,
                "navigation bar #" + displayId + " show"));
    }

    void waitAndAssertKeyguardShownOnSecondaryDisplay(int displayId) {
        assertTrue("KeyguardDialog must be shown on secondary display " + displayId,
                waitForWithAmState(
                        state -> isKeyguardOnSecondaryDisplay(state, displayId),
                        "keyguard window to show"));
    }

    void waitAndAssertKeyguardGoneOnSecondaryDisplay(int displayId) {
        assertTrue("KeyguardDialog must be gone on secondary display " + displayId,
                waitForWithAmState(
                        state -> !isKeyguardOnSecondaryDisplay(state, displayId),
                        "keyguard window to dismiss"));
    }

    void waitForWindowSurfaceDisappeared(String windowName) {
        waitForWithAmState(state -> {
            return !state.isWindowSurfaceShown(windowName);
        }, windowName + "'s surface is disappeared");
    }

    public boolean waitForWithAmState(Predicate<WindowManagerState> waitCondition,
            String message) {
        return waitFor((amState) -> waitCondition.test(amState), message);
    }

    public void waitWindowingModeTopFocus(int windowingMode, boolean topFocus, String message) {
        waitForWithAmState(amState -> {
            final ActivityTask stack = amState.getStandardStackByWindowingMode(windowingMode);
            return stack != null
                    && topFocus == (amState.getFocusedStackId() == stack.getRootTaskId());
        }, message);
    }

    /** @return {@code true} if the wait is successful; {@code false} if timeout occurs. */
    boolean waitFor(Predicate<WindowManagerState> waitCondition, String message) {
        return Condition.waitFor(message, () -> {
            computeState();
            return waitCondition.test(this);
        });
    }

    /**
     * @return true if should wait for valid stacks state.
     */
    private boolean shouldWaitForValidStacks() {
        final int stackCount = getStackCount();
        if (stackCount == 0) {
            logAlways("***stackCount=" + stackCount);
            return true;
        }
        final int resumedActivitiesCount = getResumedActivitiesCount();
        if (!getKeyguardControllerState().keyguardShowing && resumedActivitiesCount < 1) {
            logAlways("***resumedActivitiesCount=" + resumedActivitiesCount);
            return true;
        }
        if (getFocusedActivity() == null) {
            logAlways("***focusedActivity=null");
            return true;
        }
        return false;
    }

    void waitAndAssertAppFocus(String appPackageName, long waitTime) {
        final Condition<String> condition = new Condition<>(appPackageName + " to be focused");
        Condition.waitFor(condition.setResultSupplier(() -> {
            computeState();
            return getFocusedApp();
        }).setResultValidator(focusedAppName -> {
            return focusedAppName != null && appPackageName.equals(
                    ComponentName.unflattenFromString(focusedAppName).getPackageName());
        }).setOnFailure(focusedAppName -> {
            fail("Timed out waiting for focus on app "
                    + appPackageName + ", last was " + focusedAppName);
        }).setRetryIntervalMs(100).setRetryLimit((int) waitTime / 100));
    }

    /**
     * @return true if should wait for some activities to become visible.
     */
    private boolean shouldWaitForActivities(WaitForValidActivityState... waitForActivitiesVisible) {
        if (waitForActivitiesVisible == null || waitForActivitiesVisible.length == 0) {
            return false;
        }
        // If the caller is interested in us waiting for some particular activity windows to be
        // visible before compute the state. Check for the visibility of those activity windows
        // and for placing them in correct stacks (if requested).
        boolean allActivityWindowsVisible = true;
        boolean tasksInCorrectStacks = true;
        for (final WaitForValidActivityState state : waitForActivitiesVisible) {
            final ComponentName activityName = state.activityName;
            final String windowName = state.windowName;
            final int stackId = state.stackId;
            final int windowingMode = state.windowingMode;
            final int activityType = state.activityType;

            final List<WindowState> matchingWindowStates =
                    getMatchingVisibleWindowState(windowName);
            boolean activityWindowVisible = !matchingWindowStates.isEmpty();
            if (!activityWindowVisible) {
                logAlways("Activity window not visible: " + windowName);
                allActivityWindowsVisible = false;
            } else if (activityName != null
                    && !isActivityVisible(activityName)) {
                logAlways("Activity not visible: " + getActivityName(activityName));
                allActivityWindowsVisible = false;
            } else {
                // Check if window is already the correct state requested by test.
                boolean windowInCorrectState = false;
                for (WindowState ws : matchingWindowStates) {
                    if (stackId != INVALID_STACK_ID && ws.getStackId() != stackId) {
                        continue;
                    }
                    if (!ws.isWindowingModeCompatible(windowingMode)) {
                        continue;
                    }
                    if (activityType != ACTIVITY_TYPE_UNDEFINED
                            && ws.getActivityType() != activityType) {
                        continue;
                    }
                    windowInCorrectState = true;
                    break;
                }

                if (!windowInCorrectState) {
                    logAlways("Window in incorrect stack: " + state);
                    tasksInCorrectStacks = false;
                }
            }
        }
        return !allActivityWindowsVisible || !tasksInCorrectStacks;
    }

    /**
     * @return true if should wait valid windows state.
     */
    private boolean shouldWaitForWindows() {
        if (getFrontWindow() == null) {
            logAlways("***frontWindow=null");
            return true;
        }
        if (getFocusedWindow() == null) {
            logAlways("***focusedWindow=null");
            return true;
        }
        if (getFocusedApp() == null) {
            logAlways("***focusedApp=null");
            return true;
        }

        return false;
    }

    private boolean shouldWaitForDebuggerWindow(ComponentName activityName) {
        List<WindowState> matchingWindowStates =
                getMatchingVisibleWindowState(activityName.getPackageName());
        for (WindowState ws : matchingWindowStates) {
            if (ws.isDebuggerWindow()) {
                return false;
            }
        }
        logAlways("Debugger window not available yet");
        return true;
    }

    private boolean shouldWaitForActivityRecords(ComponentName... activityNames) {
        // Check if the activity records we're looking for is already added.
        for (final ComponentName activityName : activityNames) {
            if (!isActivityVisible(activityName)) {
                logAlways("ActivityRecord " + getActivityName(activityName) + " not visible yet");
                return true;
            }
        }
        return false;
    }

    private boolean shouldWaitForSanityCheck() {
        try {
            assertSanity();
        } catch (Throwable t) {
            logAlways("Waiting for sanity check: " + t.toString());
            return true;
        }
        return false;
    }

    void assertSanity() {
        assertThat("Must have stacks", getStackCount(), greaterThan(0));
        // TODO: Update when keyguard will be shown on multiple displays
        if (!getKeyguardControllerState().keyguardShowing) {
            assertThat("There should be at least one resumed activity in the system.",
                    getResumedActivitiesCount(), greaterThanOrEqualTo(1));
        }
        assertNotNull("Must have focus activity.", getFocusedActivity());

        for (ActivityTask aStack : getRootTasks()) {
            final int stackId = aStack.mRootTaskId;
            for (ActivityTask aTask : aStack.getTasks()) {
                assertEquals("Stack can only contain its own tasks", stackId, aTask.mRootTaskId);
            }
        }

        assertNotNull("Must have front window.", getFrontWindow());
        assertNotNull("Must have focused window.", getFocusedWindow());
        assertNotNull("Must have app.", getFocusedApp());
    }

    void assertContainsStack(String msg, int windowingMode, int activityType) {
        assertTrue(msg, containsStack(windowingMode, activityType));
    }

    void assertDoesNotContainStack(String msg, int windowingMode, int activityType) {
        assertFalse(msg, containsStack(windowingMode, activityType));
    }

    public void assertFrontStack(String msg, int windowingMode, int activityType) {
        assertFrontStackOnDisplay(msg, windowingMode, activityType, DEFAULT_DISPLAY);
    }

    void assertFrontStackOnDisplay(String msg, int windowingMode, int activityType, int displayId) {
        if (windowingMode != WINDOWING_MODE_UNDEFINED) {
            assertEquals(msg, windowingMode,
                    getFrontStackWindowingMode(displayId));
        }
        if (activityType != ACTIVITY_TYPE_UNDEFINED) {
            assertEquals(msg, activityType, getFrontStackActivityType(displayId));
        }
    }

    void assertFrontStackActivityType(String msg, int activityType) {
        assertEquals(msg, activityType, getFrontStackActivityType(DEFAULT_DISPLAY));
    }

    void assertFocusedStack(String msg, int stackId) {
        assertEquals(msg, stackId, getFocusedStackId());
    }

    void assertFocusedStack(String msg, int windowingMode, int activityType) {
        if (windowingMode != WINDOWING_MODE_UNDEFINED) {
            assertEquals(msg, windowingMode, getFocusedStackWindowingMode());
        }
        if (activityType != ACTIVITY_TYPE_UNDEFINED) {
            assertEquals(msg, activityType, getFocusedStackActivityType());
        }
    }

    public void assertFocusedActivity(final String msg, final ComponentName activityName) {
        final String activityComponentName = getActivityName(activityName);
        assertEquals(msg, activityComponentName, getFocusedActivity());
        assertEquals(msg, activityComponentName, getFocusedApp());
    }

    void assertFocusedAppOnDisplay(final String msg, final ComponentName activityName,
            final int displayId) {
        final String activityComponentName = getActivityName(activityName);
        assertEquals(msg, activityComponentName, getDisplay(displayId).getFocusedApp());
    }

    void assertNotFocusedActivity(String msg, ComponentName activityName) {
        assertNotEquals(msg, getFocusedActivity(), getActivityName(activityName));
        assertNotEquals(msg, getFocusedApp(), getActivityName(activityName));
    }

    public void assertResumedActivity(final String msg, final ComponentName activityName) {
        assertEquals(msg, getActivityName(activityName),
                getFocusedActivity());
    }

    /** Asserts that each display has correct resumed activity. */
    public void assertResumedActivities(final String msg,
            Consumer<SparseArray<ComponentName>> resumedActivitiesMapping) {
        final SparseArray<ComponentName> resumedActivities = new SparseArray<>();
        resumedActivitiesMapping.accept(resumedActivities);
        for (int i = 0; i < resumedActivities.size(); i++) {
            final int displayId = resumedActivities.keyAt(i);
            final ComponentName activityComponent = resumedActivities.valueAt(i);
            assertEquals("Error asserting resumed activity on display " + displayId + ": " + msg,
                    activityComponent != null ? getActivityName(activityComponent) : null,
                    getResumedActivityOnDisplay(displayId));
        }
    }

    void assertNotResumedActivity(String msg, ComponentName activityName) {
        assertNotEquals(msg, getFocusedActivity(), getActivityName(activityName));
    }

    void assertFocusedWindow(String msg, String windowName) {
        assertEquals(msg, windowName, getFocusedWindow());
    }

    void assertNotFocusedWindow(String msg, String windowName) {
        assertNotEquals(msg, getFocusedWindow(), windowName);
    }

    void assertNotExist(final ComponentName activityName) {
        final String windowName = getWindowName(activityName);
        assertFalse("Activity=" + getActivityName(activityName) + " must NOT exist.",
                containsActivity(activityName));
        assertFalse("Window=" + windowName + " must NOT exits.",
                containsWindow(windowName));
    }

    public void waitAndAssertVisibilityGone(final ComponentName activityName) {
        // Sometimes the surface can be shown due to the late animation.
        // Wait for the animation is done.
        waitForWindowSurfaceDisappeared(getWindowName(activityName));
        assertVisibility(activityName, false);
    }

    public void assertVisibility(final ComponentName activityName, final boolean visible) {
        final String windowName = getWindowName(activityName);
        // Check existence of activity and window.
        assertTrue("Activity=" + getActivityName(activityName) + " must exist.",
                containsActivity(activityName));
        assertTrue("Window=" + windowName + " must exist.", containsWindow(windowName));

        // Check visibility of activity and window.
        assertEquals("Activity=" + getActivityName(activityName) + " must" + (visible ? "" : " NOT")
                + " be visible.", visible, isActivityVisible(activityName));
        assertEquals("Window=" + windowName + " must" + (visible ? "" : " NOT")
                        + " have shown surface.",
                visible, isWindowSurfaceShown(windowName));
    }

    void assertHomeActivityVisible(boolean visible) {
        final ComponentName homeActivity = getHomeActivityName();
        assertNotNull(homeActivity);
        assertVisibility(homeActivity, visible);
    }

    /**
     * Asserts that the device default display minimim width is larger than the minimum task width.
     */
    void assertDeviceDefaultDisplaySize(String errorMessage) {
        computeState();
        final int minTaskSizePx = defaultMinimalTaskSize(DEFAULT_DISPLAY);
        final WindowManagerState.DisplayContent display = getDisplay(DEFAULT_DISPLAY);
        final Rect displayRect = display.getDisplayRect();
        if (Math.min(displayRect.width(), displayRect.height()) < minTaskSizePx) {
            fail(errorMessage);
        }
    }

    public void assertKeyguardShowingAndOccluded() {
        assertTrue("Keyguard is showing",
                getKeyguardControllerState().keyguardShowing);
        assertTrue("Keyguard is occluded",
                getKeyguardControllerState().isKeyguardOccluded(DEFAULT_DISPLAY));
    }

    public void assertKeyguardShowingAndNotOccluded() {
        assertTrue("Keyguard is showing",
                getKeyguardControllerState().keyguardShowing);
        assertFalse("Keyguard is not occluded",
                getKeyguardControllerState().isKeyguardOccluded(DEFAULT_DISPLAY));
    }

    public void assertKeyguardGone() {
        assertFalse("Keyguard is not shown",
                getKeyguardControllerState().keyguardShowing);
    }

    void assertKeyguardShownOnSecondaryDisplay(int displayId) {
        assertTrue("KeyguardDialog must be shown on display " + displayId,
                isKeyguardOnSecondaryDisplay(this, displayId));
    }

    void assertKeyguardGoneOnSecondaryDisplay(int displayId) {
        assertFalse("KeyguardDialog must be gone on display " + displayId,
                isKeyguardOnSecondaryDisplay(this, displayId));
    }

    public void assertAodShowing() {
        assertTrue("AOD is showing",
                getKeyguardControllerState().aodShowing);
    }

    public void assertAodNotShowing() {
        assertFalse("AOD is not showing",
                getKeyguardControllerState().aodShowing);
    }

    public void assertNoneEmptyTasks() {
        computeState();
        final List<ActivityTask> tasks = getRootTasks();
        for (ActivityTask task : tasks) {
            task.forAllTasks((t) -> assertWithMessage("Empty task was found, id = " + t.mTaskId)
                    .that(t.mTasks.size() + t.mActivities.size()).isGreaterThan(0));
        }
    }

    public void assumePendingActivityContain(ComponentName activity) {
        assumeTrue(pendingActivityContain(activity));
    }

    public void assertActivityDisplayed(final ComponentName activityName) {
        assertWindowDisplayed(getWindowName(activityName));
    }

    public void assertWindowDisplayed(final String windowName) {
        waitForValidState(WaitForValidActivityState.forWindow(windowName));
        assertTrue(windowName + "is visible", isWindowSurfaceShown(windowName));
    }

    void waitAndAssertImeWindowShownOnDisplay(int displayId) {
        final WindowState imeWinState = Condition.waitForResult("IME window",
                condition -> condition
                        .setResultSupplier(this::getImeWindowState)
                        .setResultValidator(
                                w -> w != null && w.isSurfaceShown()
                                        && w.getDisplayId() == displayId));

        assertNotNull("IME window must exist", imeWinState);
        assertTrue("IME window must be shown", imeWinState.isSurfaceShown());
        assertEquals("IME window must be on the given display", displayId,
                imeWinState.getDisplayId());
    }

    WindowState getImeWindowState() {
        computeState();
        return getInputMethodWindowState();
    }

    boolean isScreenPortrait() {
        final int displayId = getStandardStackByWindowingMode(
            WINDOWING_MODE_SPLIT_SCREEN_PRIMARY).mDisplayId;
        return isScreenPortrait(displayId);
    }

    boolean isScreenPortrait(int displayId) {
        final Rect displayRect = getDisplay(displayId).getDisplayRect();
        return displayRect.height() > displayRect.width();
    }

    private static boolean isKeyguardOnSecondaryDisplay(
            WindowManagerState windowManagerState, int displayId) {
        final List<WindowManagerState.WindowState> states =
                windowManagerState.getMatchingWindowType(TYPE_KEYGUARD_DIALOG);
        for (WindowManagerState.WindowState ws : states) {
            if (ws.getDisplayId() == displayId) return true;
        }
        return false;
    }
}
