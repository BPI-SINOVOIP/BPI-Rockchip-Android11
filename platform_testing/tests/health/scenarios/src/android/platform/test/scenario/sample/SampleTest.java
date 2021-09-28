/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.platform.test.scenario.sample;

import android.util.Log;
import android.platform.test.option.BooleanOption;
import android.platform.test.rule.TestWatcher;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * A test showcasing the order of execution for different components of standard JUnit 4 test.
 *
 * <p>Run this test with the listener alongside, {@link PrintListener}, to see how they interact.
 */
@Scenario
@RunWith(JUnit4.class)
public class SampleTest {

    public static final String LOG_TAG = SampleTest.class.getSimpleName();

    // Class-level rules
    @ClassRule
    public static RuleChain atClassRules =
            RuleChain.outerRule(new PrintRule("@ClassRule #1"))
                    .around(new PrintRule("@ClassRule #2"))
                    .around(new PrintRule("@ClassRule #3"));

    // Method-level rules
    @Rule
    public RuleChain atRules =
            RuleChain.outerRule(new PrintRule("@Rule #1"))
                    .around(new PrintRule("@Rule #2"))
                    .around(new PrintRule("@Rule #3"));

    @ClassRule
    public static BooleanOption failBeforeClass =
            new BooleanOption("fail-before-class").setRequired(false).setDefault(false);

    @Rule
    public BooleanOption failBefore =
            new BooleanOption("fail-before").setRequired(false).setDefault(false);

    @Rule
    public BooleanOption failTest =
            new BooleanOption("fail-test").setRequired(false).setDefault(false);

    @Rule
    public BooleanOption failAfter =
            new BooleanOption("fail-after").setRequired(false).setDefault(false);

    @ClassRule
    public static BooleanOption failAfterClass =
            new BooleanOption("fail-after-class").setRequired(false).setDefault(false);

    @BeforeClass
    public static void beforeClassMethod() {
        failIfRequested(failBeforeClass, "@BeforeClass");
        Log.d(LOG_TAG, "@BeforeClass");
    }

    @Before
    public void beforeMethod() {
        failIfRequested(failBefore, "@Before");
        Log.d(LOG_TAG, "@Before");
    }

    @Test
    public void testMethod() {
        failIfRequested(failTest, "@Test");
        Log.d(LOG_TAG, "@Test");
    }

    @After
    public void afterMethod() {
        failIfRequested(failAfter, "@After");
        Log.d(LOG_TAG, "@After");
    }

    @AfterClass
    public static void afterClassMethod() {
        failIfRequested(failAfterClass, "@AfterClass");
        Log.d(LOG_TAG, "@AfterClass");
    }

    /** Log and throw a failure if the provided {@code option} is set. */
    public static void failIfRequested(BooleanOption option, String location) {
        if (option.get()) {
            String message = String.format("Failed %s", location);
            Log.d(LOG_TAG, message);
            throw new RuntimeException(message);
        }
    }

    /** A {@link TestWatcher} that prints the methods it executes. */
    private static class PrintRule extends TestWatcher {

        private String mTag;

        public PrintRule(String tag) {
            mTag = tag;
        }

        @Override
        protected void starting(Description description) {
            Log.d(LOG_TAG, String.format("%s#starting", mTag));
        }

        @Override
        protected void finished(Description description) {
            Log.d(LOG_TAG, String.format("%s#finished.", mTag));
        }
    }
}
