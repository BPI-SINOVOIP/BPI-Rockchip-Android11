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
 * limitations under the License.
 */
package android.platform.test.rule;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;

import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test the logic for {@link IorapCompilationRule}
 */
@RunWith(JUnit4.class)
public class IorapCompilationRuleTest {
    /**
     * Tests that this rule will fail to register if no apps are supplied.
     */
    @Test
    public void testNoAppToKillFails() {
        try {
            IorapCompilationRule rule = new IorapCompilationRule();
            fail("An initialization error should have been thrown, but wasn't.");
        } catch (InitializationError e) {
            return;
        }
    }

    /**
     * Tests that this rule does nothing when 'iorapd-enabled' is unset.
     */
    @Test
    public void testDoingNothingWhenParamsUnset() throws Throwable {
        TestableIorapCompilationRule rule =
                new TestableIorapCompilationRule(new Bundle(), "example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                "test")
                .inOrder();
    }

    /**
     * Tests that this rule will disable iorapd when 'iorapd-enabled' is false.
     */
    @Test
    public void testDisablingIorapdWhenParamsAreSet() throws Throwable {
        Bundle bundle = new Bundle();
        bundle.putString(IorapCompilationRule.ARGUMENT_IORAPD_ENABLED, "false");
        TestableIorapCompilationRule rule =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                "setprop iorapd.perfetto.enable false",
                "setprop iorapd.readahead.enable false",
                "setprop iorapd.maintenance.min_traces 1",
                "dumpsys iorapd --refresh-properties",
                "test")
                .inOrder();
    }

    /**
     * Tests that this rule will enable iorapd when 'iorapd-enabled' is true.
     */
    @Test
    public void testEnablingIorapdWhenParamsAreSet() throws Throwable {
        Bundle bundle = new Bundle();
        bundle.putString(IorapCompilationRule.ARGUMENT_IORAPD_ENABLED, "true");
        TestableIorapCompilationRule rule =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                "setprop iorapd.perfetto.enable true",
                "setprop iorapd.readahead.enable true",
                "setprop iorapd.maintenance.min_traces 1",
                "dumpsys iorapd --refresh-properties",
                String.format(IorapCompilationRule.IORAP_MAINTENANCE_CMD, "example.package.name"),
                "test")
                .inOrder();
    }

    /**
     * Tests that this rule will enable iorapd when 'iorapd-enabled' is true.
     */
    @Test
    public void testCompilingIorapdWhenParamsAreSet() throws Throwable {
        Bundle bundle = new Bundle();
        bundle.putString(IorapCompilationRule.ARGUMENT_IORAPD_ENABLED, "true");
        TestableIorapCompilationRule rule =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
                .evaluate();
        // The first iteration turns on iorapd and will trace the app.
        assertThat(rule.getOperations()).containsExactly(
                "setprop iorapd.perfetto.enable true",
                "setprop iorapd.readahead.enable true",
                "setprop iorapd.maintenance.min_traces 1",
                "dumpsys iorapd --refresh-properties",
                String.format(IorapCompilationRule.IORAP_MAINTENANCE_CMD, "example.package.name"),
                "test")
                .inOrder();

        // We do nothing special for the second iteration, iorapd will be tracing.
        TestableIorapCompilationRule rule2 =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule2.apply(rule2.getTestStatement(), Description.createTestDescription("clzz", "mthd2"))
                .evaluate();
        assertThat(rule2.getOperations()).containsExactly(
                "test")
                .inOrder();

        // On the 3rd iteration, we iorap compile the package after the test method finishes.
        TestableIorapCompilationRule rule3 =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule3.apply(rule3.getTestStatement(), Description.createTestDescription("clzz", "mthd3"))
                .evaluate();
        assertThat(rule3.getOperations()).containsExactly(
                "test",
                String.format(IorapCompilationRule.IORAP_COMPILE_CMD, "example.package.name"),
                IorapCompilationRule.IORAP_DUMPSYS_CMD)
                .inOrder();

        // On the 4th and later iteration, we do nothing.
        TestableIorapCompilationRule rule4 =
                new TestableIorapCompilationRule(bundle, "example.package.name");
        rule4.apply(rule4.getTestStatement(), Description.createTestDescription("clzz", "mthd4"))
                .evaluate();
        assertThat(rule4.getOperations()).containsExactly(
                "test")
                .inOrder();
    }

    @Before
    public void resetRuleState() {
        TestableIorapCompilationRule.resetState();
    }

    private static class TestableIorapCompilationRule extends IorapCompilationRule {
        private List<String> mOperations = new ArrayList<>();
        private Bundle mBundle;

        public TestableIorapCompilationRule(Bundle bundle, String app) {
            super(app);
            mBundle = bundle;
        }

        @Override
        protected String executeShellCommand(String cmd) {
            mOperations.add(cmd);

            if (cmd.equals(IorapCompilationRule.IORAP_DUMPSYS_CMD)) {
                return "  example.package.name/com.android.example.Activity@62000712" +
                    "\n    Compiled Status: Usable compiled trace";
            }

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

        public static void resetState() {
            IorapCompilationRule.resetState();
        }

        @Override
        protected boolean checkIfRoot() {
            return true;
        }

        @Override
        protected void sleep(int ms) {
            // Intentionally left empty. The tests don't need to sleep.
        }
    }
}

