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

package com.android.tradefed.result;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * A utility class for marshalling and unmarshalling a list of TestDescriptions to a test file.
 *
 * <p>Intended to cache and minimize file I/O by lazy loading.
 *
 * <p>NOT thread safe.
 */
public class TestDescriptionsFile {

    private static final int BUFFER_SIZE = 32 * 1024;

    // the cached versions of test file and tests.
    private File mTestFile = null;
    private Set<TestDescription> mTests = null;

    /** Create an empty test description list. */
    public TestDescriptionsFile() {}

    /** Create a test description list from the file contents. */
    public TestDescriptionsFile(File file) {
        mTestFile = file;
    }

    public static TestDescriptionsFile fromTests(List<TestDescription> shardTests) {
        TestDescriptionsFile testDescriptionsFile = new TestDescriptionsFile();
        testDescriptionsFile.addAll(shardTests);
        return testDescriptionsFile;
    }

    /**
     * Return the test file representation of the tests - building it if necessary.
     *
     * <p>Tests will be unique and sorted.
     */
    public File getFile() {
        if (mTestFile == null) {
            try {
                File testFile = File.createTempFile("testfile", ".txt");
                populateTestFile(testFile);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return mTestFile;
    }

    /** Populates the given file with the current tests. */
    public void populateTestFile(File testfile) {
        populateTestFile(getTests(), testfile);
        mTestFile = testfile;
    }

    /** Return a copy of the tests stored - building from test file if necessary. */
    public List<TestDescription> getTests() {
        return new ArrayList<>(getOrBuildTests());
    }

    public void add(TestDescription test) {
        getOrBuildTests().add(test);
        // invalidate test file
        mTestFile = null;
    }

    public void addAll(List<TestDescription> test) {
        getOrBuildTests().addAll(test);
        // invalidate test file
        mTestFile = null;
    }

    public int size() {
        return getOrBuildTests().size();
    }

    public void remove(TestDescription test) {
        getOrBuildTests().remove(test);
        mTestFile = null;
    }

    private Collection<TestDescription> getOrBuildTests() {
        if (mTests == null) {
            mTests = new LinkedHashSet<>();
            if (mTestFile != null) {
                readTestsFromFile(mTests, mTestFile);
            }
        }
        return mTests;
    }

    private static void readTestsFromFile(Collection<TestDescription> tests, File testFile) {
        try (BufferedReader reader = new BufferedReader(new FileReader(testFile), BUFFER_SIZE)) {
            String line;
            while ((line = reader.readLine()) != null) {
                TestDescription test = TestDescription.fromString(line);
                if (test != null) {
                    tests.add(test);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private static void populateTestFile(List<TestDescription> tests, File testFile) {
        Collections.sort(tests);

        try (BufferedWriter writer = new BufferedWriter(new FileWriter(testFile), BUFFER_SIZE)) {
            for (TestDescription test : tests) {
                writer.write(test.toString());
                writer.newLine();
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
