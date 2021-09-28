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

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test the logic for {@link TracePointRule}
 */
@RunWith(JUnit4.class)
public class TracePointRuleTest {
    /**
     * Tests that this section will use the test name if none is supplied.
     */
    @Test
    public void testNoNameGiven() throws Throwable {
        TestableTracePointRule rule = new TestableTracePointRule();
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).contains("begin: mthd(clzz)");
    }

    /**
     * Tests that this section will use the test name if an empty string is supplied.
     */
    @Test
    public void testEmptyStringGiven() throws Throwable {
        TestableTracePointRule rule = new TestableTracePointRule("");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).contains("begin: mthd(clzz)");
    }

    /**
     * Tests that this section will use the supplied name if given.
     */
    @Test
    public void testTagGiven() throws Throwable {
        TestableTracePointRule rule = new TestableTracePointRule("FakeName");
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).contains("begin: FakeName");
    }

    /**
     * Tests that the necessary calls are made in the proper order.
     */
    @Test
    public void testTraceCallOrder() throws Throwable {
        TestableTracePointRule rule = new TestableTracePointRule();
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).containsExactly("begin: mthd(clzz)", "test", "end")
            .inOrder();
    }

    private static class TestableTracePointRule extends TracePointRule {
        private List<String> mOperations = new ArrayList<>();

        public TestableTracePointRule() {
            super();
        }

        public TestableTracePointRule(String sectionTag) {
            super(sectionTag);
        }

        @Override
        protected void beginSection(String sectionTag) {
            mOperations.add(String.format("begin: %s", sectionTag));
        }

        @Override
        protected void endSection() {
            mOperations.add("end");
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
