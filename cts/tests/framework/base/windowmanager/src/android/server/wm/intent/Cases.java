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

import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TASK;
import static android.content.Intent.FLAG_ACTIVITY_CLEAR_TOP;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP;
import static android.content.Intent.FLAG_ACTIVITY_REORDER_TO_FRONT;
import static android.content.Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED;
import static android.content.Intent.FLAG_ACTIVITY_SINGLE_TOP;
import static android.server.wm.intent.LaunchSequence.intent;
import static android.server.wm.intent.LaunchSequence.intentForResult;
import static android.server.wm.intent.Persistence.flag;

import android.content.Intent;
import android.server.wm.intent.Activities.RegularActivity;
import android.server.wm.intent.Persistence.IntentFlag;
import android.server.wm.intent.Persistence.LaunchIntent;

import com.google.common.collect.Lists;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Contains all information to create and reuse intent test launches.
 * It enumerates all the flags so they can easily be used.
 *
 * It also stores commonly used {@link LaunchSequence} objects for reuse.
 */
public class Cases {

    public static final IntentFlag CLEAR_TASK = flag(FLAG_ACTIVITY_CLEAR_TASK,
            "FLAG_ACTIVITY_CLEAR_TASK");
    public static final IntentFlag CLEAR_TOP = flag(FLAG_ACTIVITY_CLEAR_TOP,
            "FLAG_ACTIVITY_CLEAR_TOP");
    private static final IntentFlag SINGLE_TOP = flag(FLAG_ACTIVITY_SINGLE_TOP,
            "FLAG_ACTIVITY_SINGLE_TOP");
    public static final IntentFlag NEW_TASK = flag(FLAG_ACTIVITY_NEW_TASK,
            "FLAG_ACTIVITY_NEW_TASK");
    public static final IntentFlag NEW_DOCUMENT = flag(FLAG_ACTIVITY_NEW_DOCUMENT,
            "FLAG_ACTIVITY_NEW_DOCUMENT");
    private static final IntentFlag MULTIPLE_TASK = flag(FLAG_ACTIVITY_MULTIPLE_TASK,
            "FLAG_ACTIVITY_MULTIPLE_TASK");
    public static final IntentFlag RESET_TASK_IF_NEEDED = flag(
            FLAG_ACTIVITY_RESET_TASK_IF_NEEDED,
            "FLAG_ACTIVITY_RESET_TASK_IF_NEEDED");
    public static final IntentFlag PREVIOUS_IS_TOP = flag(FLAG_ACTIVITY_PREVIOUS_IS_TOP,
            "FLAG_ACTIVITY_PREVIOUS_IS_TOP");
    public static final IntentFlag REORDER_TO_FRONT = flag(FLAG_ACTIVITY_REORDER_TO_FRONT,
            "FLAG_ACTIVITY_REORDER_TO_FRONT");

    // Flag only used for parsing intents that contain no flags.
    private static final IntentFlag NONE = flag(0, "");

    public final List<IntentFlag> flags = Lists.newArrayList(
            CLEAR_TASK,
            CLEAR_TOP,
            SINGLE_TOP,
            NEW_TASK,
            NEW_DOCUMENT,
            MULTIPLE_TASK,
            RESET_TASK_IF_NEEDED,
            PREVIOUS_IS_TOP,
            REORDER_TO_FRONT
    );

    // Definition of intents used across multiple test cases.
    private final LaunchIntent mRegularIntent = intent(RegularActivity.class);
    private final LaunchIntent mSingleTopIntent = intent(Activities.SingleTopActivity.class);
    private final LaunchIntent mAff1Intent = intent(Activities.TaskAffinity1Activity.class);
    private final LaunchIntent mSecondAff1Intent = intent(Activities.TaskAffinity1Activity2.class);
    private final LaunchIntent mSingleInstanceIntent = intent(
            Activities.SingleInstanceActivity.class);
    private final LaunchIntent mSingleTaskIntent = intent(Activities.SingleTaskActivity.class);
    private final LaunchIntent mRegularForResultIntent = intentForResult(RegularActivity.class);

    // LaunchSequences used across multiple test cases.
    private final LaunchSequence mRegularSequence = LaunchSequence.create(mRegularIntent);

    // To show that the most recent task get's resumed by new task
    private final LaunchSequence mTwoAffinitiesSequence =
            LaunchSequence.create(mAff1Intent)
                    .append(mRegularIntent)
                    .append(mAff1Intent.withFlags(NEW_TASK, MULTIPLE_TASK));

    // Used to show that the task affinity is determined by the activity that started it,
    // Not the affinity of the  current root activity.
    private final LaunchSequence mRearrangedRootSequence = LaunchSequence.create(mRegularIntent)
            .append(mAff1Intent.withFlags(NEW_TASK))
            .append(mRegularIntent)
            .append(mAff1Intent.withFlags(REORDER_TO_FRONT));


    private final LaunchSequence mSingleInstanceActivitySequence =
            LaunchSequence.create(mSingleInstanceIntent);

    private final LaunchSequence mSingleTaskActivitySequence = LaunchSequence.create(
            mSingleTaskIntent);

    public List<LaunchSequence> newTaskCases() {
        LaunchIntent aff1NewTask = mAff1Intent.withFlags(NEW_TASK);

        return Lists.newArrayList(
                // 1. Single instance will start a new task even without new task set
                mRegularSequence.act(mSingleInstanceIntent),
                // 2. With new task it will still end up in a new task
                mRegularSequence.act(mSingleInstanceIntent.withFlags(NEW_TASK)),
                // 3. Starting an activity with a different affinity without new task will put it in
                // the existing task
                mRegularSequence.act(mAff1Intent),
                // 4. Starting a task with a different affinity and new task will start a new task
                mRegularSequence.act(aff1NewTask),
                // 5. Starting the intent with new task will not add a new activity to the task
                mRegularSequence.act(mRegularIntent.withFlags(NEW_TASK)),
                // 6. A different activity with the same affinity will start a new activity in
                // the sam task
                mRegularSequence.act(mSingleTopIntent.withFlags(NEW_TASK)),
                // 7. To show that the most recent task get's resumed by new task, this can't
                // be observed without differences in the same activity / task
                mTwoAffinitiesSequence.act(aff1NewTask),
                // 8. To show that new task respects the root as a single even
                // if it is not at the bottom
                mRearrangedRootSequence.act(mAff1Intent.withFlags(NEW_TASK)),
                // 9. To show that new task with non root does start a new activity.
                mRearrangedRootSequence.act(mSecondAff1Intent.withFlags(NEW_TASK)),
                // 10. Multiple task will allow starting activities of the same affinity in
                // different tasks
                mRegularSequence.act(mRegularIntent.withFlags(NEW_TASK, MULTIPLE_TASK)),
                // 11. Single instance will not start a new task even with multiple task on
                mSingleInstanceActivitySequence.act(
                        mSingleInstanceIntent.withFlags(NEW_TASK, MULTIPLE_TASK)),
                // 12. The same should happen for single task.
                mSingleTaskActivitySequence.act(
                        mSingleTaskIntent.withFlags(NEW_TASK, MULTIPLE_TASK)),
                // 13. This starts a regular in a new task
                mSingleInstanceActivitySequence.act(mRegularIntent),
                // 14. This adds regular in the same task
                mSingleTaskActivitySequence.act(mRegularIntent),
                // 15. Starting the activity for result keeps it in the same task
                mSingleInstanceActivitySequence.act(mRegularForResultIntent),
                // 16. Restarts the previous task with regular activity.
                mRegularSequence.append(mSingleInstanceIntent).act(mRegularIntent)
        );
    }

    /**
     * {@link android.content.Intent#FLAG_ACTIVITY_NEW_DOCUMENT }test cases are the same as
     * {@link Cases#newTaskCases()}, we should check them for differences.
     */
    public List<LaunchSequence> newDocumentCases() {
        LaunchIntent aff1NewDocument = mAff1Intent.withFlags(NEW_DOCUMENT);

        return Lists.newArrayList(
                // 1. Single instance will start a new task even without new task set
                mRegularSequence.act(mSingleInstanceIntent),
                // 2. With new task it will still end up in a new task
                mRegularSequence.act(mSingleInstanceIntent.withFlags(NEW_DOCUMENT)),
                // 3. Starting an activity with a different affinity without new task will put it
                // in the existing task
                mRegularSequence.act(mAff1Intent),
                // 4. With new document it will start it's own task
                mRegularSequence.act(aff1NewDocument),
                // 5. Starting the intent with new task will not add a new activity to the task
                mRegularSequence.act(mRegularIntent.withFlags(NEW_DOCUMENT)),
                // 6. Unlike the case with NEW_TASK, with new Document
                mRegularSequence.act(mSingleTopIntent.withFlags(NEW_DOCUMENT)),
                // 7. To show that the most recent task get's resumed by new task
                mTwoAffinitiesSequence.act(aff1NewDocument),
                // 8. To show that new task respects the root as a single
                mRearrangedRootSequence.act(mAff1Intent.withFlags(NEW_DOCUMENT)),
                // 9. New document starts a third task here, because there was no task for the
                // document yet
                mRearrangedRootSequence.act(mSecondAff1Intent.withFlags(NEW_DOCUMENT)),
                // 10. Multiple task wil allow starting activities of the same affinity in different
                // tasks
                mRegularSequence.act(mRegularIntent.withFlags(NEW_DOCUMENT, MULTIPLE_TASK)),
                // 11. Single instance will not start a new task even with multiple task on
                mSingleInstanceActivitySequence
                        .act(mSingleInstanceIntent.withFlags(NEW_DOCUMENT, MULTIPLE_TASK)),
                // 12. The same should happen for single task.
                mSingleTaskActivitySequence.act(
                        mSingleTaskIntent.withFlags(NEW_DOCUMENT, MULTIPLE_TASK))
        );
    }

    public List<LaunchSequence> clearCases() {
        LaunchSequence doubleRegularActivity = mRegularSequence.append(mRegularIntent);

        return Lists.newArrayList(
                // 1. This will clear the bottom and end up with just one activity
                mRegularSequence.act(mRegularIntent.withFlags(CLEAR_TOP)),
                // 2. This will result in still two regulars
                doubleRegularActivity.act(mRegularIntent.withFlags(CLEAR_TOP)),
                // 3. This will result in a single regular it clears the top regular
                // activity and then fails to start a new regular activity because it is already
                // at the root of the task
                doubleRegularActivity.act(mRegularIntent.withFlags(CLEAR_TOP, NEW_TASK)),
                // 3. This will also result in two regulars, showing the first difference between
                // new document and new task.
                doubleRegularActivity.act(mRegularIntent.withFlags(CLEAR_TOP, NEW_DOCUMENT)),
                // 4. This is here to show that previous is top has no effect on clear top or single
                // top
                doubleRegularActivity.act(mRegularIntent.withFlags(CLEAR_TOP, PREVIOUS_IS_TOP)),
                // 5. Clear top finds the same activity in the task and clears from there
                mRegularSequence.append(mAff1Intent).append(mRegularIntent).act(
                        mAff1Intent.withFlags(CLEAR_TOP))
        );
    }

    /**
     * Tests for {@link android.app.Activity#startActivityForResult(Intent, int)}
     */
    //TODO: If b/122968776 is fixed these tests need to be updated
    public List<LaunchSequence> forResultCases() {
        LaunchIntent singleTopForResult = intentForResult(Activities.SingleTopActivity.class);

        return Lists.newArrayList(
                // 1. Start just a single regular activity
                mRegularSequence.act(mRegularIntent.withFlags(SINGLE_TOP)),
                // 2. For result will start a second activity
                mRegularSequence.act(mRegularForResultIntent.withFlags(SINGLE_TOP)),
                // 3. The same but for SINGLE_TOP as a launch mode
                LaunchSequence.create(mSingleTopIntent).act(mSingleTopIntent),
                // 4. Launch mode SINGLE_TOP when launched for result also starts a second activity.
                LaunchSequence.create(mSingleTopIntent).act(singleTopForResult),
                // 5. CLEAR_TOP results in a single regular activity
                mRegularSequence.act(mRegularIntent.withFlags(CLEAR_TOP)),
                // 6. Clear will still kill the for result
                mRegularSequence.act(mRegularForResultIntent.withFlags(CLEAR_TOP)),
                // 7. An activity started for result can go to a different task
                mRegularSequence.act(mRegularForResultIntent.withFlags(NEW_TASK, MULTIPLE_TASK)),
                // 8. Reorder to front with for result
                mRegularSequence.append(mAff1Intent).act(
                        mRegularForResultIntent.withFlags(REORDER_TO_FRONT)),
                // 9. Reorder can move an activity above one that it started for result
                mRegularSequence.append(intentForResult(Activities.TaskAffinity1Activity.class))
                        .act(mRegularIntent.withFlags(REORDER_TO_FRONT))
        );
    }

    /**
     * Reset task if needed will trigger when it is delivered with new task set
     * and there are activities in the task that have a different affinity.
     *
     * @return the test cases
     */
    //TODO: If b/122324373 is fixed these test need to be updated.
    public List<LaunchSequence> resetTaskIfNeeded() {
        // If a task with a different affinity like this get's reset
        // it will create another task in the same stack with the now orphaned activity.
        LaunchIntent resetRegularTask = mRegularIntent.withFlags(NEW_TASK,
                RESET_TASK_IF_NEEDED);
        LaunchSequence resetIfNeeded = mRegularSequence.append(mAff1Intent)
                .act(resetRegularTask);

        // If you try to reset a task with an activity that was started for result
        // it will not move task.
        LaunchSequence resetWontMoveResult = mRegularSequence.append(
                intentForResult(Activities.TaskAffinity1Activity.class))
                .act(resetRegularTask);

        // Reset will not move activities with to a task with that affinity,
        // instead it will always create a new task in that stack.
        LaunchSequence resetToExistingTask2 = mRegularSequence
                .append(mAff1Intent)
                .append(mAff1Intent.withFlags(NEW_TASK))
                .act(resetRegularTask);

        // If a reset occurs the activities that move retain the same order
        // in the new task as they had in the old task.
        LaunchSequence resetOrdering = mRegularSequence
                .append(mAff1Intent)
                .append(mSecondAff1Intent)
                .act(resetRegularTask);

        return Lists.newArrayList(resetIfNeeded, resetWontMoveResult, resetToExistingTask2,
                resetOrdering);
    }

    /**
     * The human readable flags in the JSON files need to be converted back to the corresponding
     * IntentFlag object when reading the file. This creates a map from the flags to their
     * corresponding object.
     *
     * @return lookup table for the parsing of intent flags in the json files.
     */
    public Map<String, IntentFlag> createFlagParsingTable() {
        HashMap<String, IntentFlag> flags = new HashMap<>();
        for (IntentFlag flag : this.flags) {
            flags.put(flag.name, flag);
        }

        flags.put(NONE.getName(), NONE);
        return flags;
    }
}
