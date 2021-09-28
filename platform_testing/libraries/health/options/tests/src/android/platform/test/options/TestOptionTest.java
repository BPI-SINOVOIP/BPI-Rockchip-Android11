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
package android.platform.test.option;

import android.os.Bundle;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link TestOption}. */
public class TestOptionTest {
    @Rule public ExpectedException mThrown = ExpectedException.none();

    private static final String OPTION_NAME = "option";
    private static final String VALUE_VALID = "valid";
    private static final String VALUE_INVALID = "invalid";

    // A testable TestOption class that enables stubbing of the option value.
    private static class TestableOption extends TestOption<String> {
        // If stubValue() is not called, the Bundle is empty and thus does not contain the option
        // value.
        private Bundle mArguments = new Bundle();
        private String mName;

        public TestableOption(String name) {
            super(name);
            mName = name;
        }

        @Override
        public String parseValueFromString(String value) {
            // Mimick invalid argument behavior.
            if (value == VALUE_INVALID) {
                throw new RuntimeException("Invalid argument");
            }

            return value;
        }

        @Override
        Bundle getArguments() {
            return mArguments;
        }

        public void stubValue(String value) {
            mArguments.putString(mName, value);
        }
    }

    /** Test that a valid argument is properly accessed. */
    @Test
    public void testSuppliedValue_valid() throws Throwable {
        TestableOption option = new TestableOption(OPTION_NAME);
        option.stubValue(VALUE_VALID);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(VALUE_VALID, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that an invalid argument is rejected. */
    @Test
    public void testSuppliedValue_invalid() throws Throwable {
        mThrown.expectMessage("Error parsing");

        TestableOption option = new TestableOption(OPTION_NAME);
        option.stubValue(VALUE_INVALID);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(VALUE_VALID, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that the option throws for a missing argument explicitly set to be required. */
    @Test
    public void testRequiredValueAbsent_explicit() throws Throwable {
        mThrown.expectMessage("Missing argument");

        TestableOption option = new TestableOption(OPTION_NAME).setRequired(true);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        option.get();
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that an option is required by default and throws if absent in instrumentation args. */
    @Test
    public void testRequiredValueAbsent_implicit() throws Throwable {
        mThrown.expectMessage("Missing argument");

        TestableOption option = new TestableOption(OPTION_NAME);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        option.get();
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that an option with a default value will not throw, even if it is required. */
    @Test
    public void testRequiredValueAbsent_withDefault() throws Throwable {
        TestableOption option =
                new TestableOption(OPTION_NAME).setRequired(true).setDefault(VALUE_VALID);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(VALUE_VALID, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that a non-required option can return null if not provided when accessed in test. */
    @Test
    public void testNonRequiredValueAbsent() throws Throwable {
        TestableOption option = new TestableOption(OPTION_NAME).setRequired(false);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertNull(option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that an invalid option value will cause the test to be skipped. */
    @Test
    public void testOptionErrorSkipsTest_invalidValue() throws Throwable {
        TestableOption option = new TestableOption(OPTION_NAME);
        option.stubValue(VALUE_INVALID);
        // AtomicBoolean is used to allow for modification in the test statement.
        final AtomicBoolean testReached = new AtomicBoolean(false);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        testReached.set(true);
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        try {
            withOption.evaluate();
        } catch (Throwable e) {
            // Expected; do nothing.
        } finally {
            Assert.assertFalse(testReached.get());
        }
    }

    /** Test that a missing required option will cause the test to be skipped. */
    @Test
    public void testOptionErrorSkipsTest_requiredMissing() throws Throwable {
        TestableOption option = new TestableOption(OPTION_NAME).setRequired(true);
        // AtomicBoolean is used to allow for modification in the test statement.
        final AtomicBoolean testReached = new AtomicBoolean(false);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        testReached.set(true);
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        try {
            withOption.evaluate();
        } catch (Throwable e) {
            // Expected; do nothing.
        } finally {
            Assert.assertFalse(testReached.get());
        }
    }

    /**
     * Test that if an option had not been initialized before the test, the test will fail when
     * accessing the option.
     *
     * <p>This should never happen in practice, but tested just in case that custom runners do
     * something unexpected.
     */
    @Test
    public void testUninitializedOptionThrows() throws Throwable {
        mThrown.expectMessage("had not been initialized");

        TestableOption option = new TestableOption(OPTION_NAME).setRequired(true);
        option.stubValue(VALUE_VALID);
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        option.get();
                    }
                };
        // Run the "test statement" directly without running the option rule, which should throw.
        testStatement.evaluate();
    }
}
