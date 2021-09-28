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

package android.permission.cts;

import org.junit.rules.MethodRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;

/**
 * A {@link org.junit.Rule} that will cause all tests in the class
 * to be ignored if the argument to the constructor is true.
 */
public class IgnoreAllTestsRule implements MethodRule {

    private boolean mIgnore;

    /**
     * Creates a new IgnoreAllTestsRule
     * @param ignore If true, all tests in the class will be ignored. If false, this rule will
     *               do nothing.
     */
    public IgnoreAllTestsRule(boolean ignore) {
        mIgnore = ignore;
    }

    @Override
    public Statement apply(Statement base, FrameworkMethod method, Object target) {
        if (mIgnore) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                }
            };
        } else {
            return base;
        }
    }
}
