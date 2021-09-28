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

package android.platform.test.longevity;

import static java.lang.Math.max;
import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Process;
import android.platform.test.longevity.proto.Configuration.Scenario;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.util.List;
import java.util.concurrent.CountDownLatch;

import org.junit.rules.TestRule;
import org.junit.rules.Timeout;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

/**
 * A {@link BlockJUnit4ClassRunner} that runs a test class with a specified timeout and optionally
 * performs an idle before teardown (staying inside the app for Android CUJs).
 *
 * <p>TODO(b/146215435): Refactor to extends the index-based {@link ScenarioRunner}.
 */
public class ScheduledScenarioRunner extends LongevityClassRunner {
    // A leeway to ensure that the teardown steps in @After and @AfterClass has time to finish.
    // Regardless of test passing (in which case teardown is considered in test timeout) or failing
    // (in which case teardown happens outside the scope of the timeout).
    // Please note that in most cases (when the CUJ does not time out) the actual cushion for
    // teardown is double the value below, as a cushion needs to be created inside the timeout
    // rule and also outside of it.
    // This parameter is configurable via the command line as the teardown time varies across CUJs.
    @VisibleForTesting static final String TEARDOWN_LEEWAY_OPTION = "teardown-window_ms";
    @VisibleForTesting static final long TEARDOWN_LEEWAY_DEFAULT = 3000L;
    private long mTeardownLeewayMs = TEARDOWN_LEEWAY_DEFAULT;

    private static final String LOG_TAG = ScheduledScenarioRunner.class.getSimpleName();

    private final Scenario mScenario;
    private final long mTotalTimeoutMs;
    // Timeout after the teardown leeway is taken into account.
    private final long mEnforcedTimeoutMs;
    private final boolean mShouldIdle;
    private final Bundle mArguments;

    private long mStartTimeMs;

    public ScheduledScenarioRunner(
            Class<?> klass, Scenario scenario, long timeout, boolean shouldIdle)
            throws InitializationError {
        this(klass, scenario, timeout, shouldIdle, InstrumentationRegistry.getArguments());
    }

    @VisibleForTesting
    ScheduledScenarioRunner(
            Class<?> klass, Scenario scenario, long timeout, boolean shouldIdle, Bundle arguments)
            throws InitializationError {
        super(klass, arguments);
        mScenario = scenario;
        // Ensure that the timeout is non-negative.
        mTotalTimeoutMs = max(timeout, 0);
        // Ensure that the enforced timeout is non-negative. This cushion is built in so that the
        // CUJ still has time for teardown steps when the test portion times out.
        mEnforcedTimeoutMs = max(mTotalTimeoutMs - mTeardownLeewayMs, 0);
        mShouldIdle = shouldIdle;
        mArguments = arguments;
        mTeardownLeewayMs =
                Long.parseLong(
                        arguments.getString(
                                TEARDOWN_LEEWAY_OPTION, String.valueOf(mTeardownLeewayMs)));
    }

    @Override
    protected List<TestRule> getTestRules(Object target) {
        List<TestRule> rules = super.getTestRules(target);
        // Ensure that the timeout rule has a non-negative timeout.
        rules.add(0, Timeout.millis(mEnforcedTimeoutMs));
        return rules;
    }

    @Override
    protected Statement withAfters(FrameworkMethod method, Object target, Statement statement) {
        Statement withIdle =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        try {
                            // Run the underlying test and report exceptions.
                            statement.evaluate();
                        } finally {
                            // If there is time left for idling (i.e. more than mTeardownLeewayMs),
                            // and the scenario is set to stay in app, idle for the remainder of
                            // its timeout window until mTeardownLeewayMs before the start time of
                            // the next scenario, before executing the scenario's @After methods.
                            // The above does not apply if current scenario is the last one, in
                            // which case the idle is never performed regardless of its after_test
                            // policy.
                            if (mShouldIdle
                                    && mScenario
                                            .getAfterTest()
                                            .equals(Scenario.AfterTest.STAY_IN_APP)) {
                                // Subtract the teardown leeway so that teardown methods can finish
                                // within the scope of the timeout rule.
                                performIdleBeforeTeardown(
                                        max(
                                                getTimeRemainingForTimeoutRule()
                                                        - mTeardownLeewayMs,
                                                0));
                            }
                        }
                    }
                };
        return super.withAfters(method, target, withIdle);
    }

    @Override
    protected void runChild(final FrameworkMethod method, RunNotifier notifier) {
        mStartTimeMs = System.currentTimeMillis();
        super.runChild(method, notifier);
        // If there are remaining scenarios, idle until the next one starts.
        if (mShouldIdle) {
            performIdleBeforeNextScenario(getTimeRemainingForScenario());
        }
    }

    /** Get the remaining time within the current scenario. */
    private long getTimeRemainingForScenario() {
        // The idle time is total time minus time elapsed since the current scenario started.
        return max(mTotalTimeoutMs - (System.currentTimeMillis() - mStartTimeMs), 0);
    }

    /** Get the remaining time within the current timeout rule. */
    private long getTimeRemainingForTimeoutRule() {
        // The idle time is total time minus time elapsed since the current scenario started.
        return max(mEnforcedTimeoutMs - (System.currentTimeMillis() - mStartTimeMs), 0);
    }

    @VisibleForTesting
    protected void performIdleBeforeTeardown(long durationMs) {
        suspensionAwareSleep(durationMs);
    }

    @VisibleForTesting
    protected void performIdleBeforeNextScenario(long durationMs) {
        // TODO (b/119386011): Change this idle method to using a sleep test; for now, using the
        // same idling logic as {@link performIdleBeforeTeardown}.
        suspensionAwareSleep(durationMs);
    }

    /**
     * Idle with a sleep that will be accurate despite the device entering power-saving modes (e.g.
     * suspend, Doze).
     */
    @VisibleForTesting
    static void suspensionAwareSleep(long durationMs) {
        // Call the testable version of this method with arguments for the intended sleep behavior.
        suspensionAwareSleep(durationMs, durationMs);
    }

    /**
     * A testable version of suspension-aware sleep.
     *
     * <p>This method sets up a {@link CountDownLatch} that waits for a wake-up event, which is
     * triggered by an {@link AlarmManager} alarm set to fire after the sleep duration. When the
     * device enters suspend mode, the {@link CountDownLatch} await no longer works as intended and
     * in effect waits for much longer than expected, in which case the alarm fires and ends the
     * sleep behavior, ensuring that the device still sleeps for the expected amount of time. If the
     * device does not enter suspend mode, this method only waits for the {@link CountDownLatch} and
     * functions similarly to {@code Thread.sleep()}.
     *
     * <p>This testable method enables tests to set a longer await timeout on the {@link
     * CountDownLatch}, enabling that the alarm fires before the {@code CountDownLatch.await()}
     * timeout is reached, thus simulating the case where the device goes into suspend mode.
     */
    @VisibleForTesting
    static void suspensionAwareSleep(long durationMs, long countDownLatchTimeoutMs) {
        Log.i(LOG_TAG, String.format("Starting suspension-aware sleep for %d ms", durationMs));

        Context context = InstrumentationRegistry.getContext();
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);

        String wakeUpAction =
                String.format(
                        "%s.%d.%d.ScheduledScenarioRunnerSleepWakeUp",
                        context.getPackageName(), Process.myPid(), Thread.currentThread().getId());

        final CountDownLatch countDownLatch = new CountDownLatch(1);
        IntentFilter wakeUpActionFilter = new IntentFilter(wakeUpAction);
        BroadcastReceiver receiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        Log.i(
                                LOG_TAG,
                                "Suspension-aware sleep ended by receiving the wake-up intent.");
                        countDownLatch.countDown();
                    }
                };
        context.registerReceiver(receiver, wakeUpActionFilter);
        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(
                        context, 0, new Intent(wakeUpAction), PendingIntent.FLAG_UPDATE_CURRENT);

        alarmManager.setExactAndAllowWhileIdle(
                AlarmManager.RTC_WAKEUP, System.currentTimeMillis() + durationMs, pendingIntent);

        try {
            countDownLatch.await(countDownLatchTimeoutMs, MILLISECONDS);
            Log.i(LOG_TAG, "Suspension-aware sleep ended.");
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        } finally {
            context.unregisterReceiver(receiver);
        }
    }

    /** Expose the teardown leeway since tests rely on it for verifying timing. */
    @VisibleForTesting
    long getTeardownLeeway() {
        return mTeardownLeewayMs;
    }
}
