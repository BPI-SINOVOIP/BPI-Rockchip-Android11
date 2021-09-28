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

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.content.ComponentName;
import android.server.wm.intent.Persistence.LaunchFromIntent;
import android.server.wm.intent.Persistence.LaunchIntent;

import com.google.common.collect.Lists;

import java.util.List;
import java.util.Optional;

/**
 * <pre>
 * The main API entry point for specifying intent tests.
 * A {@code Launch} object is an immutable command object to specify sequences of intents to
 * launch.
 *
 * They can be run by a {@link LaunchRunner}
 * Most tests using this api are defined in {@link Cases}.
 * The two test classes actually using the tests specified there are:
 *
 * 1. {@link IntentGenerationTests}, which runs the cases, records the results and writes them
 * out to device storage
 * 2. {@link IntentTests}, which reads the recorded test files and verifies them again.
 *
 *
 * Features supported by this API are:
 * 1. Starting activities normally or for result.
 * {@code
 *  LaunchSequence.start(intentForResult(RegularActivity.class)) // starting an intent for result.
 *                .append(intent(RegularActivity.class));         // starting an intent normally.
 * }
 * 2. Specifying Intent Flags
 * {@code
 *   LaunchSequence.start(intent(RegularActivity.class).withFlags(Cases.NEW_TASK));
 * }
 *
 * 3. Launching an intent from any point earlier in the launch sequence.
 * {@code
 *   LaunchIntent multipleTask = intent(RegularActivity.class)
 *                                     .withFlags(Cases.NEW_TASK,Cases.MULTIPLE_TASK);
 *   LaunchSequence firstTask = LaunchSequence.start(multipleTask);
 *   firstTask.append(multipleTask).append(intent(SingleTopActivity.class), firstTask);
 * }
 *
 * The above will result in: the first task having two activities in it and the second task having
 * one activity, instead of the normal behaviour adding only one task.
 *
 * It is completely immutable and can therefore be reused without hesitation.
 * Note that making a launch object doesn't start anything yet until it is ran by a
 * {@link LaunchRunner}
 * </pre>
 */
public interface LaunchSequence {
    /**
     * @return the amount of intents that will launch before this {@link LaunchSequence} object.
     */
    int depth();

    /**
     * Extract all the information that has been built up in this {@link LaunchSequence} object, so
     * that {@link LaunchRunner} can run the described sequence of intents.
     */
    LaunchSequenceExecutionInfo fold();

    /**
     * @return the {@link LaunchIntent} inside of this {@link LaunchSequence}.
     */
    LaunchIntent getIntent();

    /**
     * Create a {@link LaunchSequence} object this always has
     * {@link android.content.Intent#FLAG_ACTIVITY_NEW_TASK} set because without it we can not
     * launch from the application context.
     *
     * @param intent the first intent to be Launched.
     * @return an {@link LaunchSequence} object launching just this intent.
     */
    static LaunchSequence create(LaunchIntent intent) {
        return new RootLaunch(intent.withFlags(Cases.NEW_TASK));
    }

    /**
     * @param intent the next intent that should be launched
     * @return a {@link LaunchSequence} that will launch all the intents in this and then
     * {@code intent}
     */
    default LaunchSequence append(LaunchIntent intent) {
        return new ConsecutiveLaunch(this, intent, false, Optional.empty());
    }

    /**
     * @param intent     the next intent that should be launched
     * @param launchFrom a {@link LaunchSequence} to start the intent from, {@code launchFrom}
     *                   should be a sub sequence of this.
     * @return a {@link LaunchSequence} that will launch all the intents in this and then
     * {@code intent}
     */
    default LaunchSequence append(LaunchIntent intent, LaunchSequence launchFrom) {
        return new ConsecutiveLaunch(this, intent, false, Optional.of(launchFrom));
    }

    /**
     * @param intent the intent to Launch
     * @return a launch with the {@code intent} added to the Act stage.
     */
    default LaunchSequence act(LaunchIntent intent) {
        return new ConsecutiveLaunch(this, intent, true, Optional.empty());
    }

    /**
     * @param intent     the intent to Launch
     * @param launchFrom a {@link LaunchSequence} to start the intent from, {@code launchFrom}
     *                   should be a sub sequence of this.
     * @return a launch with the {@code intent} added to the Act stage.
     */
    default LaunchSequence act(LaunchIntent intent, LaunchSequence launchFrom) {
        return new ConsecutiveLaunch(this, intent, true, Optional.of(launchFrom));
    }

    /**
     * @param activity the activity to create an intent for.
     * @return Creates an {@link LaunchIntent} that will target the {@link android.app.activity}
     * class passed in. see {@link LaunchIntent#withFlags} to add intent flags to the returned
     * intent.
     */
    static LaunchIntent intent(Class<? extends android.app.Activity> activity) {
        return new LaunchIntent(Lists.newArrayList(), createComponent(activity), false);
    }


    /**
     * Creates an {@link LaunchIntent} that will be started with {@link
     * android.app.Activity#startActivityForResult}
     *
     * @param activity the activity to create an intent for.
     */
    static LaunchIntent intentForResult(Class<? extends android.app.Activity> activity) {
        return new LaunchIntent(Lists.newArrayList(), createComponent(activity), true);
    }

    String packageName = getInstrumentation().getTargetContext().getPackageName();

    static ComponentName createComponent(Class<? extends android.app.Activity> activity) {
        return new ComponentName(packageName, activity.getName());
    }

    /**
     * Allows {@link LaunchSequence} objects to form a linkedlist of intents to launch.
     */
    class ConsecutiveLaunch implements LaunchSequence {
        private final LaunchSequence mPrevious;
        private final LaunchIntent mLaunchIntent;
        private final boolean mAct;

        public ConsecutiveLaunch(LaunchSequence previous, LaunchIntent launchIntent,
                boolean act, Optional<LaunchSequence> launchFrom) {
            mPrevious = previous;
            mLaunchIntent = launchIntent;
            mAct = act;
            mLaunchFrom = launchFrom;
        }

        /**
         * This should always be a {@link LaunchSequence} that is in mPrevious.
         */
        @SuppressWarnings("OptionalUsedAsFieldOrParameterType")
        private Optional<LaunchSequence> mLaunchFrom;

        @Override
        public int depth() {
            return mPrevious.depth() + 1;
        }

        @Override
        public LaunchSequenceExecutionInfo fold() {
            LaunchSequenceExecutionInfo launchSequenceExecutionInfo = mPrevious.fold();
            int launchSite = mLaunchFrom.map(LaunchSequence::depth).orElse(this.depth() - 1);

            LaunchFromIntent intent = new LaunchFromIntent(mLaunchIntent, launchSite);
            if (mAct) {
                launchSequenceExecutionInfo.acts.add(intent);
            } else {
                launchSequenceExecutionInfo.setup.add(intent);
            }
            return launchSequenceExecutionInfo;
        }

        @Override
        public LaunchIntent getIntent() {
            return mLaunchIntent;
        }
    }

    /**
     * The first intent to launch in a {@link LaunchSequence} Object.
     */
    class RootLaunch implements LaunchSequence {
        /**
         * The intent that should be launched.
         */
        private final LaunchIntent mLaunchIntent;

        public RootLaunch(LaunchIntent launchIntent) {
            mLaunchIntent = launchIntent;
        }

        @Override
        public int depth() {
            return 0;
        }

        @Override
        public LaunchSequenceExecutionInfo fold() {
            return new LaunchSequenceExecutionInfo(
                    Lists.newArrayList(new LaunchFromIntent(mLaunchIntent, -1)),
                    Lists.newArrayList());
        }

        @Override
        public LaunchIntent getIntent() {
            return mLaunchIntent;
        }
    }

    /**
     * Information for the {@link LaunchRunner} to run the launch represented by a {@link
     * LaunchSequence} object. The {@link LaunchSequence} object is "folded" as a list
     * into the summary of all intents with the corresponding indexes from what context
     * to launch them.
     */
    class LaunchSequenceExecutionInfo {
        List<LaunchFromIntent> setup;
        List<LaunchFromIntent> acts;

        public LaunchSequenceExecutionInfo(List<LaunchFromIntent> setup,
                List<LaunchFromIntent> acts) {
            this.setup = setup;
            this.acts = acts;
        }
    }
}

