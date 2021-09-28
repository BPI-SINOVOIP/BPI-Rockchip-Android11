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
 * limitations under the License
 */

package android.server.wm.lifecycle;

import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.server.wm.StateLogger.log;
import static android.server.wm.app.Components.PipActivity.EXTRA_ENTER_PIP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_ACTIVITY_RESULT;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_DESTROY;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_MULTI_WINDOW_MODE_CHANGED;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_NEW_INTENT;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_PAUSE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_POST_CREATE;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESTART;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_RESUME;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_START;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_STOP;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_TOP_POSITION_GAINED;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.ON_TOP_POSITION_LOST;
import static android.server.wm.lifecycle.LifecycleLog.ActivityCallback.PRE_ON_CREATE;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.ActivityOptions;
import android.app.PictureInPictureParams;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Looper;
import android.server.wm.MultiDisplayTestBase;
import android.server.wm.ObjectTracker;
import android.server.wm.lifecycle.LifecycleLog.ActivityCallback;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.util.Pair;

import android.server.wm.cts.R;

import androidx.annotation.NonNull;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Assert;
import org.junit.Before;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** Base class for device-side tests that verify correct activity lifecycle transitions. */
public class ActivityLifecycleClientTestBase extends MultiDisplayTestBase {

    /**
     * Activity launch time is evaluated. It is expected to be less than 5 seconds. Otherwise, it's
     * likely there is a timeout.
     */
    private static final long ACTIVITY_LAUNCH_TIMEOUT = 5 * 1000;

    static final String EXTRA_RECREATE = "recreate";
    static final String EXTRA_FINISH_IN_ON_CREATE = "finish_in_on_create";
    static final String EXTRA_FINISH_IN_ON_START = "finish_in_on_start";
    static final String EXTRA_FINISH_IN_ON_RESUME = "finish_in_on_resume";
    static final String EXTRA_FINISH_IN_ON_PAUSE = "finish_in_on_pause";
    static final String EXTRA_FINISH_IN_ON_STOP = "finish_in_on_stop";
    static final String EXTRA_START_ACTIVITY_IN_ON_CREATE = "start_activity_in_on_create";
    static final String EXTRA_START_ACTIVITY_WHEN_IDLE = "start_activity_when_idle";

    static final ComponentName CALLBACK_TRACKING_ACTIVITY =
            getComponentName(CallbackTrackingActivity.class);

    static final ComponentName CONFIG_CHANGE_HANDLING_ACTIVITY =
            getComponentName(ConfigChangeHandlingActivity.class);

    final ActivityTestRule mSlowActivityTestRule = new ActivityTestRule<>(
            SlowActivity.class, true /* initialTouchMode */, false /* launchActivity */);

    private static LifecycleLog mLifecycleLog;

    protected Context mTargetContext;
    private LifecycleTracker mLifecycleTracker;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mTargetContext = getInstrumentation().getTargetContext();
        // Log transitions for all activities that belong to this app.
        mLifecycleLog = new LifecycleLog();
        mLifecycleLog.clear();

        // Track transitions and allow waiting for pending activity states.
        mLifecycleTracker = new LifecycleTracker(mLifecycleLog);
    }

    /** Activity launch builder for lifecycle tests. */
    class Launcher implements ObjectTracker.Consumable {
        private int mFlags;
        private ActivityCallback mExpectedState;
        private List<String> mExtraFlags = new ArrayList<>();
        private Consumer<Intent> mPostIntentSetup;
        private ActivityOptions mOptions;
        private boolean mNoInstance;
        private final Class<? extends Activity> mActivityClass;
        private boolean mSkipLaunchTimeCheck;

        private boolean mLaunchCalled = false;

        /**
         * @param activityClass Class of the activity to launch.
         */
        Launcher(@NonNull Class<? extends Activity> activityClass) {
            mActivityClass = activityClass;
            mObjectTracker.track(this);
        }

        /**
         * Perform the activity launch. Will wait for an instance of the activity if needed and will
         * verify the launch time.
         */
        Activity launch() throws Exception {
            mLaunchCalled = true;

            // Prepare the intent
            final Intent intent = new Intent(mTargetContext, mActivityClass);
            if (mFlags != 0) {
                intent.setFlags(mFlags);
            } else {
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            for (String flag : mExtraFlags) {
                intent.putExtra(flag, true);
            }
            if (mPostIntentSetup != null) {
                mPostIntentSetup.accept(intent);
            }
            final Bundle optionsBundle = mOptions != null ? mOptions.toBundle() : null;

            // Start measuring time spent on starting the activity
            final long startTime = System.currentTimeMillis();
            final Activity activity = SystemUtil.callWithShellPermissionIdentity(() -> {
                if (mNoInstance) {
                    mTargetContext.startActivity(intent, optionsBundle);
                    return null;
                }
                return getInstrumentation().startActivitySync(
                        intent, optionsBundle);
            });
            if (!mNoInstance && activity == null) {
                fail("Must have returned an instance of Activity after launch.");
            }
            // Wait for activity to reach the desired state and verify launch time.
            if (mExpectedState == null) {
                mExpectedState = CallbackTrackingActivity.class.isAssignableFrom(mActivityClass)
                        ? ON_TOP_POSITION_GAINED : ON_RESUME;
            }
            waitAndAssertActivityStates(state(mActivityClass, mExpectedState));
            if (!mSkipLaunchTimeCheck) {
                Assert.assertThat(System.currentTimeMillis() - startTime,
                        lessThan(ACTIVITY_LAUNCH_TIMEOUT));
            }

            return activity;
        }

        /** Set intent flags for launch. */
        public Launcher setFlags(int flags) {
            mFlags = flags;
            return this;
        }

        /**
         * Set the expected lifecycle state to verify. Will be inferred automatically if not set.
         */
        public Launcher setExpectedState(ActivityCallback expectedState) {
            mExpectedState = expectedState;
            return this;
        }

        /** Allow the caller to customize the intent right before starting activity. */
        public Launcher customizeIntent(Consumer<Intent> intentSetup) {
            mPostIntentSetup = intentSetup;
            return this;
        }

        /** Set extra flags to pass as boolean values through the intent. */
        public Launcher setExtraFlags(String... extraFlags) {
            mExtraFlags.addAll(Arrays.asList(extraFlags));
            return this;
        }

        /** Set the activity options to use for the launch. */
        public Launcher setOptions(ActivityOptions options) {
            mOptions = options;
            return this;
        }

        /**
         * Indicate that no instance should be returned. Usually used for activity launches that are
         * expected to end up in not-active state and when the synchronous instrumentation launch
         * can timeout.
         */
        Launcher setNoInstance() {
            mNoInstance = true;
            return this;
        }

        /** Indicate that launch time verification should not be performed. */
        Launcher setSkipLaunchTimeCheck() {
            mSkipLaunchTimeCheck = true;
            return this;
        }

        @Override
        public boolean isConsumed() {
            return mLaunchCalled;
        }
    }

    /**
     * Launch an activity given a class. Will wait for the launch to finish and verify the launch
     * time.
     * @return The launched Activity instance.
     */
    Activity launchActivityAndWait(Class<? extends Activity> activityClass) throws Exception {
        return new Launcher(activityClass).launch();
    }

    /**
     * Blocking call that will wait for activities to reach expected states with timeout.
     */
    @SafeVarargs
    final void waitAndAssertActivityStates(
            Pair<Class<? extends Activity>, ActivityCallback>... activityCallbacks) {
        log("Start waitAndAssertActivityCallbacks");
        mLifecycleTracker.waitAndAssertActivityStates(activityCallbacks);
    }

    /**
     * Blocking call that will wait for activities to perform the expected sequence of transitions.
     * @see LifecycleTracker#waitForActivityTransitions(Class, List)
     */
    final void waitForActivityTransitions(Class<? extends Activity> activityClass,
            List<ActivityCallback> expectedTransitions) {
        log("Start waitForActivityTransitions");
        mLifecycleTracker.waitForActivityTransitions(activityClass, expectedTransitions);
    }

    /**
     * Blocking call that will wait for activities to perform the expected sequence of transitions.
     * After waiting it asserts that the sequence matches the expected.
     * @see LifecycleTracker#waitForActivityTransitions(Class, List)
     */
    final void waitAndAssertActivityTransitions(Class<? extends Activity> activityClass,
            List<ActivityCallback> expectedTransitions, String message) {
        log("Start waitAndAssertActivityTransition");
        mLifecycleTracker.waitForActivityTransitions(activityClass, expectedTransitions);

        LifecycleVerifier.assertSequence(activityClass, getLifecycleLog(), expectedTransitions,
                message);
    }

    LifecycleLog getLifecycleLog() {
        return mLifecycleLog;
    }

    static Pair<Class<? extends Activity>, ActivityCallback> state(Activity activity,
            ActivityCallback stage) {
        return state(activity.getClass(), stage);
    }

    static Pair<Class<? extends Activity>, ActivityCallback> state(
            Class<? extends Activity> activityClass, ActivityCallback stage) {
        return new Pair<>(activityClass, stage);
    }

    /**
     * Returns a pair of the activity and the state it should be in based on the configuration of
     * occludingActivity.
     */
    static Pair<Class<? extends Activity>, ActivityCallback> occludedActivityState(
            Activity activity, Activity occludingActivity) {
        return occludedActivityState(activity, isTranslucent(occludingActivity));
    }

    /**
     * Returns a pair of the activity and the state it should be in based on
     * occludingActivityIsTranslucent.
     */
    static Pair<Class<? extends Activity>, ActivityCallback> occludedActivityState(
            Activity activity, boolean occludingActivityIsTranslucent) {
        // Activities behind a translucent activity should be in the paused state since they are
        // still visible. Otherwise, they should be in the stopped state.
        return state(activity, occludedActivityState(occludingActivityIsTranslucent));
    }

    static ActivityCallback occludedActivityState(boolean occludingActivityIsTranslucent) {
        return occludingActivityIsTranslucent ? ON_PAUSE : ON_STOP;
    }

    /** Returns true if the input activity is translucent. */
    static boolean isTranslucent(Activity activity) {
        return ActivityInfo.isTranslucentOrFloating(activity.getWindow().getWindowStyle());
    }

    /** Base activity that only tracks fundamental activity lifecycle states. */
    public static class LifecycleTrackingActivity extends Activity {
        LifecycleLog.LifecycleLogClient mLifecycleLogClient;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            mLifecycleLogClient = LifecycleLog.LifecycleLogClient.create(this);
            mLifecycleLogClient.onActivityCallback(PRE_ON_CREATE);
            mLifecycleLogClient.onActivityCallback(ON_CREATE);

            final Intent intent = getIntent();
            final Intent startOnCreate =
                    intent.getParcelableExtra(EXTRA_START_ACTIVITY_IN_ON_CREATE);
            if (startOnCreate != null) {
                startActivity(startOnCreate);
            }

            final Intent startOnIdle = intent.getParcelableExtra(EXTRA_START_ACTIVITY_WHEN_IDLE);
            if (startOnIdle != null) {
                Looper.getMainLooper().getQueue().addIdleHandler(() -> {
                    startActivity(startOnIdle);
                    return false;
                });
            }

            if (intent.getBooleanExtra(EXTRA_FINISH_IN_ON_CREATE, false)) {
                finish();
            }
        }

        @Override
        protected void onStart() {
            super.onStart();
            mLifecycleLogClient.onActivityCallback(ON_START);

            if (getIntent().getBooleanExtra(EXTRA_FINISH_IN_ON_START, false)) {
                finish();
            }
        }

        @Override
        protected void onResume() {
            super.onResume();
            mLifecycleLogClient.onActivityCallback(ON_RESUME);

            final Intent intent = getIntent();
            if (intent.getBooleanExtra(EXTRA_FINISH_IN_ON_RESUME, false)) {
                finish();
            }
        }

        @Override
        protected void onPause() {
            super.onPause();
            mLifecycleLogClient.onActivityCallback(ON_PAUSE);

            if (getIntent().getBooleanExtra(EXTRA_FINISH_IN_ON_PAUSE, false)) {
                finish();
            }
        }

        @Override
        protected void onStop() {
            super.onStop();
            mLifecycleLogClient.onActivityCallback(ON_STOP);

            if (getIntent().getBooleanExtra(EXTRA_FINISH_IN_ON_STOP, false)) {
                finish();
            }
        }

        @Override
        protected void onDestroy() {
            super.onDestroy();
            mLifecycleLogClient.onActivityCallback(ON_DESTROY);
            mLifecycleLogClient.close();
        }

        @Override
        protected void onRestart() {
            super.onRestart();
            mLifecycleLogClient.onActivityCallback(ON_RESTART);
        }
    }

    // Test activity
    public static class FirstActivity extends LifecycleTrackingActivity {
    }

    // Test activity
    public static class SecondActivity extends LifecycleTrackingActivity {
    }

    // Test activity
    public static class ThirdActivity extends LifecycleTrackingActivity {
    }

    // Translucent test activity
    public static class TranslucentActivity extends LifecycleTrackingActivity {
    }

    // Translucent test activity
    public static class SecondTranslucentActivity extends LifecycleTrackingActivity {
    }

    /**
     * Base activity that records callbacks in addition to main lifecycle transitions.
     */
    public static class CallbackTrackingActivity extends LifecycleTrackingActivity {

        @Override
        protected void onActivityResult(int requestCode, int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            mLifecycleLogClient.onActivityCallback(ON_ACTIVITY_RESULT);
        }

        @Override
        protected void onPostCreate(Bundle savedInstanceState) {
            super.onPostCreate(savedInstanceState);
            mLifecycleLogClient.onActivityCallback(ON_POST_CREATE);
        }

        @Override
        protected void onNewIntent(Intent intent) {
            super.onNewIntent(intent);
            mLifecycleLogClient.onActivityCallback(ON_NEW_INTENT);
        }

        @Override
        public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
            mLifecycleLogClient.onActivityCallback(
                    isTopResumedActivity ? ON_TOP_POSITION_GAINED : ON_TOP_POSITION_LOST);
        }

        @Override
        public void onMultiWindowModeChanged(boolean isInMultiWindowMode, Configuration newConfig) {
            mLifecycleLogClient.onActivityCallback(ON_MULTI_WINDOW_MODE_CHANGED);
        }
    }

    // Just another callback tracking activity, nothing special.
    public static class SecondCallbackTrackingActivity extends CallbackTrackingActivity {
    }

    // Translucent callback tracking test activity
    public static class TranslucentCallbackTrackingActivity extends CallbackTrackingActivity {
    }

    // Callback tracking activity that supports being shown on top of lock screen
    public static class ShowWhenLockedCallbackTrackingActivity extends CallbackTrackingActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setShowWhenLocked(true);
        }
    }

    /**
     * Test activity that launches {@link ResultActivity} for result.
     */
    public static class LaunchForResultActivity extends CallbackTrackingActivity {
        private static final String EXTRA_FORWARD_EXTRAS = "FORWARD_EXTRAS";
        public static final String EXTRA_LAUNCH_ON_RESULT = "LAUNCH_ON_RESULT";
        public static final String EXTRA_LAUNCH_ON_RESUME_AFTER_RESULT =
                "LAUNCH_ON_RESUME_AFTER_RESULT";

        boolean mReceivedResultOk;

        /** Adds the flag to the extra of intent which will forward to {@link ResultActivity}. */
        static Consumer<Intent> forwardFlag(String flag) {
            return intent -> {
                final Bundle data = new Bundle();
                data.putBoolean(flag, true);
                intent.putExtra(EXTRA_FORWARD_EXTRAS, data);
            };
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            final Intent intent = new Intent(this, ResultActivity.class);
            final Bundle forwardExtras = getIntent().getBundleExtra(EXTRA_FORWARD_EXTRAS);
            if (forwardExtras != null) {
                intent.putExtras(forwardExtras);
            }
            startActivityForResult(intent, 1 /* requestCode */);
        }

        @Override
        protected void onResume() {
            super.onResume();
            if (mReceivedResultOk
                    && getIntent().getBooleanExtra(EXTRA_LAUNCH_ON_RESUME_AFTER_RESULT, false)) {
                startActivity(new Intent(this, CallbackTrackingActivity.class));
            }
        }

        @Override
        protected void onActivityResult(int requestCode, int resultCode, Intent data) {
            super.onActivityResult(requestCode, resultCode, data);
            mReceivedResultOk = resultCode == RESULT_OK;
            if (mReceivedResultOk && getIntent().getBooleanExtra(EXTRA_LAUNCH_ON_RESULT, false)) {
                startActivity(new Intent(this, CallbackTrackingActivity.class));
            }
        }
    }

    /** Test activity that is started for result. */
    public static class ResultActivity extends CallbackTrackingActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            setResult(RESULT_OK);
            super.onCreate(savedInstanceState);
        }
    }

    /** Test activity with NoDisplay theme that can finish itself. */
    public static class NoDisplayActivity extends ResultActivity {
        static final String EXTRA_LAUNCH_ACTIVITY = "extra_launch_activity";
        static final String EXTRA_NEW_TASK = "extra_new_task";

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            if (getIntent().getBooleanExtra(EXTRA_LAUNCH_ACTIVITY, false)) {
                final Intent intent = new Intent(this, CallbackTrackingActivity.class);
                if (getIntent().getBooleanExtra(EXTRA_NEW_TASK, false)) {
                    intent.setFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_MULTIPLE_TASK);
                }
                startActivity(intent);
            }
        }
    }

    /** Test activity that can call {@link Activity#recreate()} if requested in a new intent. */
    public static class SingleTopActivity extends CallbackTrackingActivity {

        @Override
        protected void onNewIntent(Intent intent) {
            super.onNewIntent(intent);
            if (intent != null && intent.getBooleanExtra(EXTRA_RECREATE, false)) {
                recreate();
            }
        }
    }

    // Config change handling activity
    public static class ConfigChangeHandlingActivity extends CallbackTrackingActivity {
    }

    // Callback tracking activity that runs in a separate process
    public static class SecondProcessCallbackTrackingActivity extends CallbackTrackingActivity {
    }

    // Pip-capable activity
    // TODO(b/123013403): Disabled onMultiWindowMode changed callbacks to make the tests pass, so
    // that they can verify other lifecycle transitions. This should be fixed and switched to
    // extend CallbackTrackingActivity.
    public static class PipActivity extends LifecycleTrackingActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            // Enter picture in picture with the given aspect ratio if provided
            if (getIntent().hasExtra(EXTRA_ENTER_PIP)) {
                enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
            }
        }
    }

    public static class AlwaysFocusablePipActivity extends CallbackTrackingActivity {
    }

    public static class SlowActivity extends CallbackTrackingActivity {

        static final String EXTRA_CONTROL_FLAGS = "extra_control_flags";
        static final int FLAG_SLOW_TOP_RESUME_RELEASE = 0x00000001;
        static final int FLAG_TIMEOUT_TOP_RESUME_RELEASE = 0x00000002;

        private int mFlags;

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            mFlags = getIntent().getIntExtra(EXTRA_CONTROL_FLAGS, 0);
        }

        @Override
        protected void onNewIntent(Intent intent) {
            super.onNewIntent(intent);
            mFlags = getIntent().getIntExtra(EXTRA_CONTROL_FLAGS, 0);
        }

        @Override
        public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
            if (!isTopResumedActivity && (mFlags & FLAG_SLOW_TOP_RESUME_RELEASE) != 0) {
                sleep(200);
            } else if (!isTopResumedActivity && (mFlags & FLAG_TIMEOUT_TOP_RESUME_RELEASE) != 0) {
                sleep(2000);
            }
            // Intentionally moving the logging of the state change to after sleep to facilitate
            // race condition with other activity getting top state before this releases its.
            super.onTopResumedActivityChanged(isTopResumedActivity);
        }

        private void sleep(long millis) {
            try {
                Thread.sleep(millis);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public static class DifferentAffinityActivity extends LifecycleTrackingActivity {
    }

    public static class TransitionSourceActivity extends LifecycleTrackingActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.transition_source_layout);
        }

        void launchActivityWithTransition() {
            final ActivityOptions options = ActivityOptions.makeSceneTransitionAnimation(this,
                    findViewById(R.id.transitionView), "sharedTransition");
            final Intent intent = new Intent(this, TransitionDestinationActivity.class);
            startActivity(intent, options.toBundle());
        }
    }

    public static class TransitionDestinationActivity extends LifecycleTrackingActivity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.transition_destination_layout);
            final Transition sharedElementEnterTransition =
                    getWindow().getSharedElementEnterTransition();
            sharedElementEnterTransition.addListener(new TransitionListenerAdapter() {
                @Override
                public void onTransitionEnd(Transition transition) {
                    super.onTransitionEnd(transition);
                    finishAfterTransition();
                }
            });
        }
    }

    static ComponentName getComponentName(Class<? extends Activity> activity) {
        return new ComponentName(getInstrumentation().getContext(), activity);
    }

    void moveTaskToPrimarySplitScreenAndVerify(Activity activity) {
        getLifecycleLog().clear();

        moveTaskToPrimarySplitScreen(activity.getTaskId());

        final Class<? extends Activity> activityClass = activity.getClass();
        waitAndAssertActivityTransitions(activityClass,
                LifecycleVerifier.getSplitScreenTransitionSequence(activityClass),
                "enterSplitScreen");
    }
}
