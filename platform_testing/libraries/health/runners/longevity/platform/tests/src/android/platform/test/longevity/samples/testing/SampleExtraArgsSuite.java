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

package android.platform.test.longevity.samples.testing;

import android.platform.test.longevity.ProfileSuite;
import android.platform.test.scenario.annotation.Scenario;
import androidx.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.Suite.SuiteClasses;
import org.junit.runners.model.Statement;

@RunWith(ProfileSuite.class)
@SuiteClasses({
    SampleExtraArgsSuite.ClassLevelExtraArgsScenario.class,
    SampleExtraArgsSuite.TestLevelExtraArgsScenario.class,
    SampleExtraArgsSuite.InTestExtraArgsScenario.class,
    SampleExtraArgsSuite.NotInInstrumentationArgsTest.class
})
/** A sample test suite to verify that extra args are injected before the test class starts. */
public class SampleExtraArgsSuite {

    // A string to filter out expected assertion failures in the test.
    public static final String ASSERTION_FAILURE_MESSAGE = "Test assertion failed";

    /** This test simulates a test option that is used as a {@link ClassRule}. */
    @Scenario
    @RunWith(JUnit4.class)
    public static class ClassLevelExtraArgsScenario {
        public static final String CLASS_LEVEL_OPTION = "class-level-option";
        public static final String CLASS_LEVEL_DEFAULT = "class-level-default";
        public static final String CLASS_LEVEL_OVERRIDE = "class-level-override";

        @ClassRule
        public static TestRule classLevelOption =
                new TestRule() {
                    @Override
                    public Statement apply(Statement base, Description description) {
                        return new Statement() {
                            @Override
                            public void evaluate() throws Throwable {
                                Assert.assertEquals(
                                        ASSERTION_FAILURE_MESSAGE,
                                        CLASS_LEVEL_OVERRIDE,
                                        InstrumentationRegistry.getArguments()
                                                .getString(
                                                        CLASS_LEVEL_OPTION, CLASS_LEVEL_DEFAULT));
                                base.evaluate();
                            }
                        };
                    }
                };

        @Test
        public void test() {}
    }

    /** This test simulates a test option that is used as a {@link Rule}. */
    @Scenario
    @RunWith(JUnit4.class)
    public static class TestLevelExtraArgsScenario {
        public static final String TEST_LEVEL_OPTION = "test-level-option";
        public static final String TEST_LEVEL_DEFAULT = "test-level-default";
        public static final String TEST_LEVEL_OVERRIDE = "test-level-override";

        @Rule
        public TestRule classLevelOption =
                new TestRule() {
                    @Override
                    public Statement apply(Statement base, Description description) {
                        return new Statement() {
                            @Override
                            public void evaluate() throws Throwable {
                                Assert.assertEquals(
                                        ASSERTION_FAILURE_MESSAGE,
                                        TEST_LEVEL_OVERRIDE,
                                        InstrumentationRegistry.getArguments()
                                                .getString(TEST_LEVEL_OPTION, TEST_LEVEL_DEFAULT));
                                base.evaluate();
                            }
                        };
                    }
                };

        @Test
        public void test() {}
    }

    /** This test simulates an argument that is read within a test. */
    @Scenario
    @RunWith(JUnit4.class)
    public static class InTestExtraArgsScenario {
        public static final String IN_TEST_ARG = "in-test-option";
        public static final String IN_TEST_DEFAULT = "in-test-default";
        public static final String IN_TEST_OVERRIDE = "in-test-override";

        @Test
        public void testArgOverride() {
            Assert.assertEquals(
                    ASSERTION_FAILURE_MESSAGE,
                    IN_TEST_OVERRIDE,
                    InstrumentationRegistry.getArguments().getString(IN_TEST_ARG, IN_TEST_DEFAULT));
        }
    }

    /** Tests that a particular argument is not in instrumentation args. */
    @Scenario
    @RunWith(JUnit4.class)
    public static class NotInInstrumentationArgsTest {
        public static final String ARG_TO_CHECK_OPTION = "arg-to-check";

        @Test
        public void testArgIsAbsent() {
            String argToCheck =
                    InstrumentationRegistry.getArguments().getString(ARG_TO_CHECK_OPTION);
            Assert.assertNotNull(ASSERTION_FAILURE_MESSAGE, argToCheck);
            Assert.assertNull(
                    ASSERTION_FAILURE_MESSAGE,
                    InstrumentationRegistry.getArguments().getString(argToCheck));
        }
    }
}
