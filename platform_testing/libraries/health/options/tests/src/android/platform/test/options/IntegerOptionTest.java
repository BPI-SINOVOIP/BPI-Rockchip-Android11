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

/** Unit tests for {@link IntegerOption}. */
public class IntegerOptionTest {
    @Rule public ExpectedException mThrown = ExpectedException.none();

    private static final String OPTION_NAME = "option";

    private static class TestableIntegerOption extends IntegerOption {
        private Bundle mArguments = new Bundle();
        private String mName;

        public TestableIntegerOption(String name) {
            super(name);
            mName = name;
        }

        @Override
        Bundle getArguments() {
            return mArguments;
        }

        public void stubValue(String value) {
            mArguments.putString(mName, value);
        }
    }

    /** Test that the option is parsed correctly for a valid integer value. */
    @Test
    public void testParsing_valid() throws Throwable {
        TestableIntegerOption option = new TestableIntegerOption(OPTION_NAME);
        // Using boxed value here to avoid ambiguity when calling assertEquals.
        Integer value = 7;
        option.stubValue(String.valueOf(value));
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(value, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that the option throws when using an invalid number format. */
    @Test
    public void testParsing_invalid() throws Throwable {
        mThrown.expectMessage("Error parsing");

        TestableIntegerOption option = new TestableIntegerOption(OPTION_NAME);
        option.stubValue("not an integer");
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
}
