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
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/** Unit tests for {@link StringOption}. */
public class StringOptionTest {
    private static final String OPTION_NAME = "option";

    private static class TestableStringOption extends StringOption {
        private Bundle mArguments = new Bundle();
        private String mName;

        public TestableStringOption(String name) {
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

    /** Test that the option is retrieved correctly. */
    @Test
    public void testRetrieval() throws Throwable {
        TestableStringOption option = new TestableStringOption(OPTION_NAME);
        String value = "val";
        option.stubValue(value);
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
}
