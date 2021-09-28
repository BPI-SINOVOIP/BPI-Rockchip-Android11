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
package com.android.compatibility;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.testtype.InstrumentationTest;
import com.android.tradefed.util.PublicApkUtil.ApkInfo;


import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

@RunWith(JUnit4.class)
public final class AppCompatibilityTestTest {

    private ConcreteAppCompatibilityTest mSut;

    private class ConcreteAppCompatibilityTest extends AppCompatibilityTest {

        ConcreteAppCompatibilityTest() {
            super(null, null, null);
        }

        @Override
        protected InstrumentationTest createInstrumentationTest(String packageBeingTested) {
            return null;
        }
    }

    @Before
    public void setUp() {
        mSut = new ConcreteAppCompatibilityTest();
    }

    @Test(expected = IllegalArgumentException.class)
    public void addIncludeFilter_nullIncludeFilter_throwsException() {
        mSut.addIncludeFilter(null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void addIncludeFilter_emptyIncludeFilter_throwsException() {
        mSut.addIncludeFilter("");
    }

    @Test
    public void addIncludeFilter_validIncludeFilter() {
        mSut.addIncludeFilter("test_filter");

        assertTrue(mSut.mIncludeFilters.contains("test_filter"));
    }

    @Test(expected = NullPointerException.class)
    public void addAllIncludeFilters_nullIncludeFilter_throwsException() {
        mSut.addAllIncludeFilters(null);
    }

    @Test
    public void addAllIncludeFilters_validIncludeFilters() {
        Set<String> test_filters = new TreeSet<>();
        test_filters.add("filter_one");
        test_filters.add("filter_two");

        mSut.addAllIncludeFilters(test_filters);

        assertTrue(mSut.mIncludeFilters.contains("filter_one"));
        assertTrue(mSut.mIncludeFilters.contains("filter_two"));
    }

    @Test
    public void clearIncludeFilters() {
        mSut.addIncludeFilter("filter_test");

        mSut.clearIncludeFilters();

        assertTrue(mSut.mIncludeFilters.isEmpty());
    }

    @Test
    public void getIncludeFilters() {
        mSut.addIncludeFilter("filter_test");

        assertEquals(mSut.mIncludeFilters, mSut.getIncludeFilters());
    }

    @Test(expected = IllegalArgumentException.class)
    public void addExcludeFilter_nullExcludeFilter_throwsException() {
        mSut.addExcludeFilter(null);
    }

    @Test(expected = IllegalArgumentException.class)
    public void addExcludeFilter_emptyExcludeFilter_throwsException() {
        mSut.addExcludeFilter("");
    }

    @Test
    public void addExcludeFilter_validExcludeFilter() {
        mSut.addExcludeFilter("test_filter");

        assertTrue(mSut.mExcludeFilters.contains("test_filter"));
    }

    @Test(expected = NullPointerException.class)
    public void addAllExcludeFilters_nullExcludeFilters_throwsException() {
        mSut.addAllExcludeFilters(null);
    }

    @Test
    public void addAllExcludeFilters_validExcludeFilters() {
        Set<String> test_filters = new TreeSet<>();
        test_filters.add("filter_one");
        test_filters.add("filter_two");

        mSut.addAllExcludeFilters(test_filters);

        assertTrue(mSut.mExcludeFilters.contains("filter_one"));
        assertTrue(mSut.mExcludeFilters.contains("filter_two"));
    }

    @Test
    public void clearExcludeFilters() {
        mSut.addExcludeFilter("filter_test");

        mSut.clearExcludeFilters();

        assertTrue(mSut.mExcludeFilters.isEmpty());
    }

    @Test
    public void getExcludeFilters() {
        mSut.addExcludeFilter("filter_test");

        assertEquals(mSut.mExcludeFilters, mSut.getExcludeFilters());
    }

    @Test
    public void filterApk_withNoFilter() {
        List<ApkInfo> testList = createApkList();

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(filteredList, testList);
    }

    @Test
    public void filterApk_withRelatedIncludeFilters() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_one");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(convertList(filteredList), Arrays.asList("filter_one"));
    }

    @Test
    public void filterApk_withUnrelatedIncludeFilters() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_three");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertTrue(filteredList.isEmpty());
    }

    @Test
    public void filterApk_withRelatedExcludeFilters() {
        List<ApkInfo> testList = createApkList();
        mSut.addExcludeFilter("filter_one");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(convertList(filteredList), Arrays.asList("filter_two"));
    }

    @Test
    public void filterApk_withUnrelatedExcludeFilters() {
        List<ApkInfo> testList = createApkList();
        mSut.addExcludeFilter("filter_three");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(filteredList, testList);
    }

    @Test
    public void filterApk_withSameIncludeAndExcludeFilters() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_one");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertTrue(filteredList.isEmpty());
    }

    @Test
    public void filterApk_withDifferentIncludeAndExcludeFilter() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_one");
        mSut.addIncludeFilter("filter_two");
        mSut.addExcludeFilter("filter_two");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(convertList(filteredList), Arrays.asList("filter_one"));
    }

    @Test
    public void filterApk_withUnrelatedIncludeFilterAndRelatedExcludeFilter() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_three");
        mSut.addExcludeFilter("filter_two");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertTrue(filteredList.isEmpty());
    }

    @Test
    public void filterApk_withRelatedIncludeFilterAndUnrelatedExcludeFilter() {
        List<ApkInfo> testList = createApkList();
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_three");

        List<ApkInfo> filteredList = mSut.filterApk(testList);

        assertEquals(convertList(filteredList), Arrays.asList("filter_one"));
    }

    private List<ApkInfo> createApkList() {
        List<ApkInfo> testList = new ArrayList<>();
        ApkInfo apk_info_one = new ApkInfo(0, "filter_one", "", "", "");
        ApkInfo apk_info_two = new ApkInfo(0, "filter_two", "", "", "");
        testList.add(apk_info_one);
        testList.add(apk_info_two);
        return testList;
    }

    private List<String> convertList(List<ApkInfo> apkList) {
        List<String> convertedList = new ArrayList<>();
        for (ApkInfo apkInfo : apkList) {
            convertedList.add(apkInfo.packageName);
        }
        return convertedList;
    }

    @Test
    public void filterTest_withEmptyFilter() {
        assertTrue(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withRelatedIncludeFilter() {
        mSut.addIncludeFilter("filter_one");

        assertTrue(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withUnrelatedIncludeFilter() {
        mSut.addIncludeFilter("filter_two");

        assertFalse(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withRelatedExcludeFilter() {
        mSut.addExcludeFilter("filter_one");

        assertFalse(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withUnrelatedExcludeFilter() {
        mSut.addExcludeFilter("filter_two");

        assertTrue(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withSameIncludeAndExcludeFilters() {
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_one");

        assertFalse(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withUnrelatedIncludeFilterAndRelatedExcludeFilter() {
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_two");

        assertFalse(mSut.filterTest("filter_two"));
    }

    @Test
    public void filterTest_withRelatedIncludeFilterAndUnrelatedExcludeFilter() {
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_two");

        assertTrue(mSut.filterTest("filter_one"));
    }

    @Test
    public void filterTest_withUnRelatedIncludeFilterAndUnrelatedExcludeFilter() {
        mSut.addIncludeFilter("filter_one");
        mSut.addExcludeFilter("filter_two");

        assertFalse(mSut.filterTest("filter_three"));
    }
}
