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

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test the logic for {@link DropCachesRule}
 */
@RunWith(JUnit4.class)
public class DropCachesRuleTest {
    /**
     * Tests that the drop caches command is run before the test method.
     */
    @Test
    public void testDropCachesCommand() throws Throwable {
        TestableDropCachesRule rule = new TestableDropCachesRule(new Bundle());
        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).containsExactly(
                rule.getDropCacheScriptPath(), "test")
            .inOrder();
    }

    /**
     * Tests no drop cache if the drop-cache flag is set to false.
     */
    @Test
    public void testNoDropCacheFlag() throws Throwable {
        Bundle noDropCacheBundle = new Bundle();
        noDropCacheBundle.putString(DropCachesRule.KEY_DROP_CACHE, "false");
        TestableDropCachesRule rule = new TestableDropCachesRule(noDropCacheBundle);

        rule.apply(rule.getTestStatement(), Description.createTestDescription("clzz", "mthd"))
            .evaluate();
        assertThat(rule.getOperations()).containsExactly("test")
            .inOrder();
    }

    private static class TestableDropCachesRule extends DropCachesRule {
        private List<String> mOperations = new ArrayList<>();
        private Bundle mBundle;

        public TestableDropCachesRule(Bundle bundle) {
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

        @Override
        public String getDropCacheScriptPath() {
            return super.getDropCacheScriptPath();
        }
    }
}
