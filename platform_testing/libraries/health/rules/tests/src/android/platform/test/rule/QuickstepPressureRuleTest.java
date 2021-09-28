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
package android.platform.test.rule;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/** Unit test the logic for {@link QuickstepPressureRule} */
@RunWith(JUnit4.class)
public class QuickstepPressureRuleTest {
    /** Tests that this rule will fail to register if no apps are supplied. */
    @Test
    public void testNoAppToOpenFails() {
        try {
            QuickstepPressureRule rule = new QuickstepPressureRule();
            fail("An illegal argument error should have been thrown, but wasn't.");
        } catch (IllegalArgumentException e) {
            return;
        }
    }

    /** Tests that this rule will open one app before the test, if supplied. */
    @Test
    public void testOneAppToOpen() throws Throwable {
        TestableQuickstepPressureRule rule =
                new TestableQuickstepPressureRule("example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations())
                .containsExactly("start example.package.name", "test")
                .inOrder();
    }

    /** Tests that this rule will open multiple apps before the test, if supplied. */
    @Test
    public void testMultipleAppsToOpen() throws Throwable {
        TestableQuickstepPressureRule rule =
                new TestableQuickstepPressureRule(
                        "package.name1", "package.name2", "package.name3");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations())
                .containsExactly(
                        "start package.name1", "start package.name2", "start package.name3", "test")
                .inOrder();
    }

    private static class TestableQuickstepPressureRule extends QuickstepPressureRule {
        private List<String> mOperations = new ArrayList<>();

        public TestableQuickstepPressureRule(String app) {
            super(app);
        }

        public TestableQuickstepPressureRule(String... apps) {
            super(apps);
        }

        @Override
        void startActivity(String pkg) {
            mOperations.add(String.format("start %s", pkg));
        }

        public List<String> getOperations() {
            return mOperations;
        }

        public Statement getTestStatement() {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    mOperations.add("test");
                }
            };
        }
    }
}
