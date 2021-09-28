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

package android.server.wm.intent;

import static android.server.wm.intent.Persistence.LaunchFromIntent.prepareSerialisation;
import static android.server.wm.intent.StateComparisonException.assertEndStatesEqual;
import static android.server.wm.intent.StateComparisonException.assertInitialStateEqual;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.common.collect.Iterables.getLast;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.SystemClock;
import android.server.wm.WindowManagerStateHelper;
import android.server.wm.WindowManagerState;
import android.server.wm.intent.LaunchSequence.LaunchSequenceExecutionInfo;
import android.server.wm.intent.Persistence.GenerationIntent;
import android.server.wm.intent.Persistence.LaunchFromIntent;
import android.server.wm.intent.Persistence.StateDump;
import android.view.Display;

import com.google.common.collect.Lists;

import java.util.List;

/**
 * Launch runner is an interpreter for a {@link LaunchSequence} command object.
 * It supports three main modes of operation.
 *
 * 1. The {@link LaunchRunner#runAndWrite} method to run a launch object and write out the
 * resulting {@link Persistence.TestCase} to device storage
 *
 * 2. The {@link LaunchRunner#verify} method to rerun a previously recorded
 * {@link Persistence.TestCase} and verify that the recorded states match the states resulting from
 * the rerun.
 *
 * 3. The {@link LaunchRunner#run} method to run a launch object and return an {@link LaunchRecord}
 * that can be used to do assertions directly in the same test.
 */
public class LaunchRunner {
    private static final int ACTIVITY_LAUNCH_TIMEOUT = 10000;
    private static final int BEFORE_DUMP_TIMEOUT = 3000;

    /**
     * Used for the waiting utilities.
     */
    private IntentTestBase mTestBase;

    /**
     * The activities that were already present in the system when the test started.
     * So they can be removed form the outputs, otherwise our tests would be system dependent.
     */
    private List<WindowManagerState.ActivityTask> mBaseTasks;

    public LaunchRunner(IntentTestBase testBase) {
        mTestBase = testBase;
        mBaseTasks = getBaseTasks();
    }

    /**
     * Re-run a previously recorded {@link Persistence.TestCase} and verify that the recorded
     * states match the states resulting from the rerun.
     *
     * @param initialContext the context to launch the first Activity from.
     * @param testCase       the {@link Persistence.TestCase} we are verifying.
     */
    void verify(Context initialContext, Persistence.TestCase testCase) {
        List<GenerationIntent> initialState = testCase.getSetup().getInitialIntents();
        List<GenerationIntent> act = testCase.getSetup().getAct();

        List<Activity> activityLog = Lists.newArrayList();

        // Launch the first activity from the start context
        GenerationIntent firstIntent = initialState.get(0);
        activityLog.add(launchFromContext(initialContext, firstIntent.getActualIntent()));

        // launch the rest from the initial intents
        for (int i = 1; i < initialState.size(); i++) {
            GenerationIntent generationIntent = initialState.get(i);
            Activity activityToLaunchFrom = activityLog.get(generationIntent.getLaunchFromIndex(i));
            Activity result = launch(activityToLaunchFrom, generationIntent.getActualIntent(),
                    generationIntent.startForResult());
            activityLog.add(result);
        }

        // assert that the state after setup is the same this time as the recorded state.
        StateDump setupStateDump = waitDumpAndTrimForVerification(getLast(activityLog),
                testCase.getInitialState());
        assertInitialStateEqual(testCase.getInitialState(), setupStateDump);

        // apply all the intents in the act stage
        for (int i = 0; i < act.size(); i++) {
            GenerationIntent generationIntent = act.get(i);
            Activity activityToLaunchFrom = activityLog.get(
                    generationIntent.getLaunchFromIndex(initialState.size() + i));
            Activity result = launch(activityToLaunchFrom, generationIntent.getActualIntent(),
                    generationIntent.startForResult());
            activityLog.add(result);
        }

        // assert that the endStates are the same.
        StateDump endStateDump = waitDumpAndTrimForVerification(getLast(activityLog),
                testCase.getEndState());
        assertEndStatesEqual(testCase.getEndState(), endStateDump);
    }

    /**
     * Runs a launch object and writes out the resulting {@link Persistence.TestCase} to
     * device storage
     *
     * @param startContext the context to launch the first Activity from.
     * @param name         the name of the directory to store the json files in.
     * @param launches     a list of launches to run and record.
     */
    public void runAndWrite(Context startContext, String name, List<LaunchSequence> launches)
            throws Exception {
        for (int i = 0; i < launches.size(); i++) {
            Persistence.TestCase testCase = this.runAndSerialize(launches.get(i), startContext,
                    Integer.toString(i));
            IntentTests.writeToDocumentsStorage(testCase, i + 1, name);
            // Cleanup all the activities of this testCase before going to the next
            // to preserve isolation across test cases.
            mTestBase.cleanUp(testCase.getSetup().componentsInCase());
        }
    }

    private Persistence.TestCase runAndSerialize(LaunchSequence launchSequence,
            Context startContext, String name) {
        LaunchRecord launchRecord = run(launchSequence, startContext);

        LaunchSequenceExecutionInfo executionInfo = launchSequence.fold();
        List<GenerationIntent> setupIntents = prepareSerialisation(executionInfo.setup);
        List<GenerationIntent> actIntents = prepareSerialisation(executionInfo.acts,
                setupIntents.size());

        Persistence.Setup setup = new Persistence.Setup(setupIntents, actIntents);

        return new Persistence.TestCase(setup, launchRecord.initialDump, launchRecord.endDump,
                name);
    }

    /**
     * Runs a launch object and returns a {@link LaunchRecord} that can be used to do assertions
     * directly in the same test.
     *
     * @param launch       the {@link LaunchSequence}we want to run
     * @param startContext the {@link android.content.Context} to launch the first Activity from.
     * @return {@link LaunchRecord} that can be used to do assertions.
     */
    LaunchRecord run(LaunchSequence launch, Context startContext) {
        LaunchSequence.LaunchSequenceExecutionInfo work = launch.fold();
        List<Activity> activityLog = Lists.newArrayList();

        if (work.setup.isEmpty() || work.acts.isEmpty()) {
            throw new IllegalArgumentException("no intents to start");
        }

        // Launch the first activity from the start context.
        LaunchFromIntent firstIntent = work.setup.get(0);
        Activity firstActivity = this.launchFromContext(startContext,
                firstIntent.getActualIntent());

        activityLog.add(firstActivity);

        // launch the rest from the initial intents.
        for (int i = 1; i < work.setup.size(); i++) {
            LaunchFromIntent launchFromIntent = work.setup.get(i);
            Intent actualIntent = launchFromIntent.getActualIntent();
            Activity activity = launch(activityLog.get(launchFromIntent.getLaunchFrom()),
                    actualIntent, launchFromIntent.startForResult());
            activityLog.add(activity);
        }

        // record the state after the initial intents.
        StateDump initialDump = waitDumpAndTrim(getLast(activityLog));

        // apply all the intents in the act stage
        for (LaunchFromIntent launchFromIntent : work.acts) {
            Intent actualIntent = launchFromIntent.getActualIntent();
            Activity activity = launch(activityLog.get(launchFromIntent.getLaunchFrom()),
                    actualIntent, launchFromIntent.startForResult());

            activityLog.add(activity);
        }

        //record the end state after all intents are launched.
        StateDump endDump = waitDumpAndTrim(getLast(activityLog));

        return new LaunchRecord(initialDump, endDump, activityLog);
    }

    /**
     * Results from the running of an {@link LaunchSequence} so the user can assert on the results
     * directly.
     */
    class LaunchRecord {

        /**
         * The end state after the setup intents.
         */
        public final StateDump initialDump;

        /**
         * The end state after the setup and act intents.
         */
        public final StateDump endDump;

        /**
         * The activities that were started by every intent in the {@link LaunchSequence}.
         */
        public final List<Activity> mActivitiesLog;

        public LaunchRecord(StateDump initialDump, StateDump endDump,
                List<Activity> activitiesLog) {
            this.initialDump = initialDump;
            this.endDump = endDump;
            mActivitiesLog = activitiesLog;
        }
    }


    public Activity launchFromContext(Context context, Intent intent) {
        Instrumentation.ActivityMonitor monitor = getInstrumentation()
                .addMonitor((String) null, null, false);

        context.startActivity(intent);
        Activity activity = monitor.waitForActivityWithTimeout(ACTIVITY_LAUNCH_TIMEOUT);
        waitAndAssertActivityLaunched(activity, intent);

        return activity;
    }

    public Activity launch(Activity activityContext, Intent intent, boolean startForResult) {
        Instrumentation.ActivityMonitor monitor = getInstrumentation()
                .addMonitor((String) null, null, false);

        if (startForResult) {
            activityContext.startActivityForResult(intent, 1);
        } else {
            activityContext.startActivity(intent);
        }
        Activity activity = monitor.waitForActivityWithTimeout(ACTIVITY_LAUNCH_TIMEOUT);

        if (activity == null) {
            return activityContext;
        } else {
            waitAndAssertActivityLaunched(activity, intent);
        }

        return activity;
    }

    private void waitAndAssertActivityLaunched(Activity activity, Intent intent) {
        assertNotNull("Intent: " + intent.toString(), activity);

        final ComponentName testActivityName = activity.getComponentName();
        mTestBase.waitAndAssertTopResumedActivity(testActivityName,
                Display.DEFAULT_DISPLAY, "Activity must be resumed");
    }

    /**
     * After the last activity has been launched we wait for a valid state + an extra three seconds
     * so have a stable state of the system. Also all previously known tasks in
     * {@link LaunchRunner#mBaseTasks} is excluded from the output.
     *
     * @param activity The last activity to be launched before dumping the state.
     * @return A stable {@link StateDump}, meaning no more {@link android.app.Activity} is in a
     * life cycle transition.
     */
    public StateDump waitDumpAndTrim(Activity activity) {
        mTestBase.getWmState().waitForValidState(activity.getComponentName());
        // The last activity that was launched before the dump could still be in an intermediate
        // lifecycle state. wait an extra 3 seconds for it to settle
        SystemClock.sleep(BEFORE_DUMP_TIMEOUT);
        mTestBase.getWmState().computeState(activity.getComponentName());
        List<WindowManagerState.ActivityTask> endStateTasks =
                mTestBase.getWmState().getRootTasks();
        return StateDump.fromTasks(endStateTasks, mBaseTasks);
    }

    /**
     * Like {@link LaunchRunner#waitDumpAndTrim(Activity)} but also waits until the state becomes
     * equal to the state we expect. It is therefore only used when verifying a recorded testcase.
     *
     * If we take a dump of an unstable state we allow it to settle into the expected state.
     *
     * @param activity The last activity to be launched before dumping the state.
     * @param expected The state that was previously recorded for this testCase.
     * @return A stable {@link StateDump}, meaning no more {@link android.app.Activity} is in a
     * life cycle transition.
     */
    public StateDump waitDumpAndTrimForVerification(Activity activity, StateDump expected) {
        mTestBase.getWmState().waitForValidState(activity.getComponentName());
        mTestBase.getWmState().waitForWithAmState(
                am -> StateDump.fromTasks(am.getRootTasks(), mBaseTasks).equals(expected),
                "the activity states match up with what we recorded");
        mTestBase.getWmState().computeState(activity.getComponentName());

        List<WindowManagerState.ActivityTask> endStateTasks =
                mTestBase.getWmState().getRootTasks();

        return StateDump.fromTasks(endStateTasks, mBaseTasks);
    }

    private List<WindowManagerState.ActivityTask> getBaseTasks() {
        WindowManagerStateHelper amWmState = mTestBase.getWmState();
        amWmState.computeState(new ComponentName[]{});
        return amWmState.getRootTasks();
    }
}
