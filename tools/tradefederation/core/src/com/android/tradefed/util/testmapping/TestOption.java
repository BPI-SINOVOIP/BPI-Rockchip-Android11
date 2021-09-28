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
package com.android.tradefed.util.testmapping;

/** Stores the test option details set in a TEST_MAPPING file. */
public class TestOption implements Comparable<TestOption> {
    // Name of the option
    private String mName = null;
    // Value of the option, can be empty.
    private String mValue = null;

    public TestOption(String name, String value) {
        mName = name;
        mValue = value;
    }

    public String getName() {
        return mName;
    }

    public String getValue() {
        return mValue;
    }

    /** {@inheritDoc} */
    @Override
    public int compareTo(TestOption option) {
        return (mName.compareTo(option.getName()));
    }

    /**
     * Check if the option is used to only include certain tests.
     *
     * <p>Some sample inclusive options are:
     *
     * <p>include-filter
     *
     * <p>positive-testname-filter (GTest)
     *
     * <p>test-file-include-filter (AndroidJUnitTest)
     *
     * <p>include-annotation (AndroidJUnitTest)
     *
     * @return true if the option is used to only include certain tests.
     */
    public boolean isInclusive() {
        return mName.contains("include") || mName.contains("positive");
    }

    /**
     * Check if the option is used to only exclude certain tests.
     *
     * <p>Some sample exclusive options are:
     *
     * <p>exclude-filter
     *
     * <p>negative-testname-filter (GTest)
     *
     * <p>test-file-exclude-filter (AndroidJUnitTest)
     *
     * <p>exclude-annotation (AndroidJUnitTest)
     *
     * @return true if the option is used to only exclude certain tests.
     */
    public boolean isExclusive() {
        return mName.contains("exclude") || mName.contains("negative");
    }

    @Override
    public int hashCode() {
        return this.toString().hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        return mName.equals(((TestOption) obj).getName())
                && mValue.equals(((TestOption) obj).getValue());
    }

    @Override
    public String toString() {
        return String.format("%s:%s", mName, mValue);
    }
}
