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
 * limitations under the License.
 */
package android.platform.test.rule;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test the logic for {@link KillAppsRule}
 */
@RunWith(JUnit4.class)
public class KillAppsRuleTest {
    /**
     * Tests that this rule will fail to register if no apps are supplied.
     */
    @Test
    public void testNoAppToKillFails() {
        try {
            KillAppsRule rule = new KillAppsRule();
            fail("An initialization error should have been thrown, but wasn't.");
        } catch (InitializationError e) {
            return;
        }
    }

    /**
     * Tests that this rule will kill one app before the test, if supplied.
     */
    @Test
    public void testOneAppToKill() throws Throwable {
        TestableKillAppsRule rule = new TestableKillAppsRule(new Bundle(), "example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                "am force-stop example.package.name", "test")
                .inOrder();
    }

    /**
     * Tests that this rule will kill multiple apps before the test, if supplied.
     */
    @Test
    public void testMultipleAppsToKill() throws Throwable {
        TestableKillAppsRule rule = new TestableKillAppsRule(new Bundle(),
                "package.name1",
                "package.name2",
                "package.name3");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                "am force-stop package.name1",
                "am force-stop package.name2",
                "am force-stop package.name3",
                "test")
                .inOrder();
    }

    /**
     * Tests apps are not killed if kill-app flag is set to false.
     */
    @Test
    public void testDisableKillsAppsRuleOption() throws Throwable {
        Bundle noKillAppsBundle = new Bundle();
        noKillAppsBundle.putString(KillAppsRule.KILL_APP, "false");
        TestableKillAppsRule rule = new TestableKillAppsRule(noKillAppsBundle,
                "example.package.name");

        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly("test")
                .inOrder();
    }

    private static class TestableKillAppsRule extends KillAppsRule {
        private List<String> mOperations = new ArrayList<>();
        private Bundle mBundle;

        public TestableKillAppsRule(Bundle bundle, String app) {
            super(app);
            mBundle = bundle;
        }

        public TestableKillAppsRule(Bundle bundle, String... apps) {
            super(apps);
            mBundle = bundle;
        }

        @Override
        protected String executeShellCommand(String cmd) {
            mOperations.add(cmd);
            return "";
        }

        @Override
        protected Bundle getArguments() {
            return mBundle;
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
