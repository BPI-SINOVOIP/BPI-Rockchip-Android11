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

import static com.google.common.base.Preconditions.checkState;

import com.android.tradefed.log.LogUtil.CLog;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Stores the test information set in a TEST_MAPPING file. */
public class TestInfo {
    private static final String OPTION_INCLUDE_ANNOTATION = "include-annotation";
    private static final String OPTION_EXCLUDE_ANNOTATION = "exclude-annotation";

    private String mName = null;
    private List<TestOption> mOptions = new ArrayList<TestOption>();
    // A Set of locations with TEST_MAPPING files that containing the test.
    private Set<String> mSources = new HashSet<String>();
    // True if the test should run on host and require no device.
    private boolean mHostOnly = false;
    // A Set of keywords to be matched when filtering tests to run in a Test Mapping suite.
    private Set<String> mKeywords = null;

    public TestInfo(String name, String source, boolean hostOnly) {
        this(name, source, hostOnly, new HashSet<String>());
    }

    public TestInfo(String name, String source, boolean hostOnly, Set<String> keywords) {
        mName = name;
        mSources.add(source);
        mHostOnly = hostOnly;
        mKeywords = keywords;
    }

    public String getName() {
        return mName;
    }

    public void addOption(TestOption option) {
        mOptions.add(option);
        Collections.sort(mOptions);
    }

    public List<TestOption> getOptions() {
        return mOptions;
    }

    public void addSources(Set<String> sources) {
        mSources.addAll(sources);
    }

    public Set<String> getSources() {
        return mSources;
    }

    public boolean getHostOnly() {
        return mHostOnly;
    }

    /**
     * Get a {@link String} represent the test name and its host setting. This allows TestInfos to
     * be grouped by name the requirement on device.
     */
    public String getNameAndHostOnly() {
        return String.format("%s - %s", mName, mHostOnly);
    }

    /** Get a {@link Set} of the keywords supported by the test. */
    public Set<String> getKeywords() {
        return new HashSet<>(mKeywords);
    }

    /**
     * Merge with another test.
     *
     * <p>Update test options so the test has the best possible coverage of both tests.
     *
     * <p>TODO(b/113616538): Implement a more robust option merging mechanism.
     *
     * @param test {@link TestInfo} object to be merged with.
     */
    public void merge(TestInfo test) {
        CLog.d("Merging test %s and %s.", this, test);
        // Merge can only happen for tests for the same module.
        checkState(
                mName.equals(test.getName()), "Only TestInfo for the same module can be merged.");
        // Merge can only happen for tests for the same device requirement.
        checkState(
                mHostOnly == test.getHostOnly(),
                "Only TestInfo for the same device requirement (running on device or host) can"
                        + " be merged.");

        List<TestOption> mergedOptions = new ArrayList<>();

        // If any test only has exclusive options or no option, only keep the common exclusive
        // option in the merged test. For example:
        // this.mOptions: include-filter=value1, exclude-annotation=flaky
        // test.mOptions: exclude-annotation=flaky, exclude-filter=value2
        // merged options: exclude-annotation=flaky
        // Note that:
        // * The exclude-annotation of flaky is common between the two tests, so it's kept.
        // * The include-filter of value1 is dropped as `test` doesn't have any include-filter,
        //   thus it has larger test coverage and the include-filter is ignored.
        // * The exclude-filter of value2 is dropped as it's only for `test`. To achieve maximum
        //   test coverage for both `this` and `test`, we shall only keep the common exclusive
        //   filters.
        // * In the extreme case that one of the test has no option at all, the merged test will
        //   also have no option.
        if (test.exclusiveOptionsOnly() || this.exclusiveOptionsOnly()) {
            Set<TestOption> commonOptions = new HashSet<TestOption>(test.getOptions());
            commonOptions.retainAll(new HashSet<TestOption>(mOptions));
            mOptions = new ArrayList<TestOption>(commonOptions);
            this.addSources(test.getSources());
            CLog.d("Options are merged, updated test: %s.", this);
            return;
        }

        // When neither test has no option or with only exclusive options, we try the best to
        // merge the test options so the merged test will cover both tests.
        // 1. Keep all non-exclusive options, except include-annotation
        // 2. Keep common exclusive options
        // 3. Keep common include-annotation options
        // 4. Keep any exclude-annotation options
        // Condition 3 and 4 are added to make sure we have the best test coverage if possible.
        // In most cases, one add include-annotation to include only presubmit test, but some other
        // test config that doesn't use presubmit annotation doesn't have such option. Therefore,
        // uncommon include-annotation option has to be dropped to prevent losing test coverage.
        // On the other hand, exclude-annotation is often used to exclude flaky tests. Therefore,
        // it's better to keep any exclude-annotation option to prevent flaky tests from being
        // included.
        // For example:
        // this.mOptions: include-filter=value1, exclude-filter=ex-value1, exclude-filter=ex-value2,
        //                exclude-annotation=flaky, include-annotation=presubmit
        // test.mOptions: exclude-filter=ex-value1, include-filter=value3
        // merged options: exclude-annotation=flaky, include-filter=value1, include-filter=value3
        // Note that:
        // * The "exclude-filter=value3" option is kept as it's common in both tests.
        // * The "exclude-annotation=flaky" option is kept even though it's only in one test.
        // * The "include-annotation=presubmit" option is dropped as it only exists for `this`.
        // * The include-filter of value1 and value3 are both kept so the merged test will cover
        //   both tests.
        // * The "exclude-filter=ex-value1" option is kept as it's common in both tests.
        // * The "exclude-filter=ex-value2" option is dropped as it's only for `this`. To achieve
        //     maximum test coverage for both `this` and `test`, we shall only keep the common
        //     exclusive filters.

        // Options from this test:
        Set<TestOption> nonExclusiveOptions =
                mOptions.stream()
                        .filter(
                                option ->
                                        !option.isExclusive()
                                                && !OPTION_INCLUDE_ANNOTATION.equals(
                                                        option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> includeAnnotationOptions =
                mOptions.stream()
                        .filter(option -> OPTION_INCLUDE_ANNOTATION.equals(option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> exclusiveOptions =
                mOptions.stream()
                        .filter(
                                option ->
                                        option.isExclusive()
                                                && !OPTION_EXCLUDE_ANNOTATION.equals(
                                                        option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> excludeAnnotationOptions =
                mOptions.stream()
                        .filter(option -> OPTION_EXCLUDE_ANNOTATION.equals(option.getName()))
                        .collect(Collectors.toSet());
        // Options from TestInfo to be merged:
        Set<TestOption> nonExclusiveOptionsToMerge =
                test.getOptions()
                        .stream()
                        .filter(
                                option ->
                                        !option.isExclusive()
                                                && !OPTION_INCLUDE_ANNOTATION.equals(
                                                        option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> includeAnnotationOptionsToMerge =
                test.getOptions()
                        .stream()
                        .filter(option -> OPTION_INCLUDE_ANNOTATION.equals(option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> exclusiveOptionsToMerge =
                test.getOptions()
                        .stream()
                        .filter(
                                option ->
                                        option.isExclusive()
                                                && !OPTION_EXCLUDE_ANNOTATION.equals(
                                                        option.getName()))
                        .collect(Collectors.toSet());
        Set<TestOption> excludeAnnotationOptionsToMerge =
                test.getOptions()
                        .stream()
                        .filter(option -> OPTION_EXCLUDE_ANNOTATION.equals(option.getName()))
                        .collect(Collectors.toSet());

        // 1. Keep all non-exclusive options, except include-annotation
        nonExclusiveOptions.addAll(nonExclusiveOptionsToMerge);
        for (TestOption option : nonExclusiveOptions) {
            mergedOptions.add(option);
        }
        // 2. Keep common exclusive options, except exclude-annotation
        exclusiveOptions.retainAll(exclusiveOptionsToMerge);
        for (TestOption option : exclusiveOptions) {
            mergedOptions.add(option);
        }
        // 3. Keep common include-annotation options
        includeAnnotationOptions.retainAll(includeAnnotationOptionsToMerge);
        for (TestOption option : includeAnnotationOptions) {
            mergedOptions.add(option);
        }
        // 4. Keep any exclude-annotation options
        excludeAnnotationOptions.addAll(excludeAnnotationOptionsToMerge);
        for (TestOption option : excludeAnnotationOptions) {
            mergedOptions.add(option);
        }
        this.mOptions = mergedOptions;
        this.addSources(test.getSources());
        CLog.d("Options are merged, updated test: %s.", this);
    }

    /* Check if the TestInfo only has exclusive options.
     *
     * @return true if the TestInfo only has exclusive options.
     */
    private boolean exclusiveOptionsOnly() {
        for (TestOption option : mOptions) {
            if (option.isInclusive()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean equals(Object o) {
        return this.toString().equals(o.toString());
    }

    @Override
    public int hashCode() {
        return this.toString().hashCode();
    }

    @Override
    public String toString() {
        StringBuilder string = new StringBuilder();
        string.append(mName);
        if (!mOptions.isEmpty()) {
            String options =
                    String.format(
                            "Options: %s",
                            String.join(
                                    ",",
                                    mOptions.stream()
                                            .sorted()
                                            .map(TestOption::toString)
                                            .collect(Collectors.toList())));
            string.append("\n\t").append(options);
        }
        if (!mKeywords.isEmpty()) {
            String keywords =
                    String.format(
                            "Keywords: %s",
                            String.join(
                                    ",", mKeywords.stream().sorted().collect(Collectors.toList())));
            string.append("\n\t").append(keywords);
        }
        if (!mSources.isEmpty()) {
            String sources =
                    String.format(
                            "Sources: %s",
                            String.join(
                                    ",", mSources.stream().sorted().collect(Collectors.toList())));
            string.append("\n\t").append(sources);
        }
        string.append("\n\tHost: ").append(mHostOnly);
        return string.toString();
    }
}
