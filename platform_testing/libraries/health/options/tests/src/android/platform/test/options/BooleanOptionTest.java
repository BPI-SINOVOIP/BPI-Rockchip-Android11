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

/** Unit tests for {@link BooleanOption}. */
public class BooleanOptionTest {
    private static final String OPTION_NAME = "option";

    private static class TestableBooleanOption extends BooleanOption {
        private Bundle mArguments = new Bundle();
        private String mName;

        public TestableBooleanOption(String name) {
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

    /** Test that the option is parsed correctly for a "true" value. */
    @Test
    public void testParsing_true() throws Throwable {
        TestableBooleanOption option = new TestableBooleanOption(OPTION_NAME);
        option.stubValue(String.valueOf(true));
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(true, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }

    /** Test that the option is parsed correctly for a "false" value equivalent. */
    @Test
    public void testParsing_false() throws Throwable {
        TestableBooleanOption option = new TestableBooleanOption(OPTION_NAME);
        option.stubValue("not true");
        Statement testStatement =
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        Assert.assertEquals(false, option.get());
                    }
                };
        Statement withOption = option.apply(testStatement, Description.EMPTY);
        withOption.evaluate();
    }
}
