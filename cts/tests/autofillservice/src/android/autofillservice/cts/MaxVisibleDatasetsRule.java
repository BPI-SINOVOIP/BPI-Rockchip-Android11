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

package android.autofillservice.cts;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Custom JUnit4 rule that improves autofill-related environment by:
 *
 * <ol>
 *   <li>Setting max_visible_datasets before and after test.
 * </ol>
 */
public final class MaxVisibleDatasetsRule implements TestRule {

    private static final String TAG = MaxVisibleDatasetsRule.class.getSimpleName();

    private final int mMaxNumber;

    /**
     * Creates a MaxVisibleDatasetsRule with given datasets values.
     *
     * @param maxNumber The desired max_visible_datasets value for a test,
     * after the test it will be replaced by the original value
     */
    public MaxVisibleDatasetsRule(int maxNumber) {
        mMaxNumber = maxNumber;
    }


    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {

            @Override
            public void evaluate() throws Throwable {
                final int original = Helper.getMaxVisibleDatasets();
                Helper.setMaxVisibleDatasets(mMaxNumber);
                try {
                    base.evaluate();
                } finally {
                    Helper.setMaxVisibleDatasets(original);
                }
            }
        };
    }
}
