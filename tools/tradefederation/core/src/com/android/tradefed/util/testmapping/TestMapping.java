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

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil2;

import com.google.common.annotations.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileVisitOption;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** A class for loading a TEST_MAPPING file. */
public class TestMapping {

    // Key for test sources information stored in meta data of ConfigurationDescription.
    public static final String TEST_SOURCES = "Test Sources";

    private static final String PRESUBMIT = "presubmit";
    private static final String IMPORTS = "imports";
    private static final String KEY_HOST = "host";
    private static final String KEY_KEYWORDS = "keywords";
    private static final String KEY_NAME = "name";
    private static final String KEY_OPTIONS = "options";
    private static final String TEST_MAPPING = "TEST_MAPPING";
    private static final String TEST_MAPPINGS_ZIP = "test_mappings.zip";
    // A file containing module names that are disabled in presubmit test runs.
    private static final String DISABLED_PRESUBMIT_TESTS_FILE = "disabled-presubmit-tests";

    private Map<String, Set<TestInfo>> mTestCollection = null;
    // Pattern used to identify comments start with "//" or "#" in TEST_MAPPING.
    private static final Pattern COMMENTS_REGEX = Pattern.compile(
            "(?m)[\\s\\t]*(//|#).*|(\".*?\")");
    private static final Set<String> COMMENTS = new HashSet<>(Arrays.asList("#", "//"));

    private static List<String> mTestMappingRelativePaths = new ArrayList<>();

    /**
     * Set the TEST_MAPPING paths inside of TEST_MAPPINGS_ZIP to limit loading the TEST_MAPPING.
     *
     * @param relativePaths A {@code List<String>} of TEST_MAPPING paths relative to
     *     TEST_MAPPINGS_ZIP.
     */
    public static void setTestMappingPaths(List<String> relativePaths) {
        mTestMappingRelativePaths.clear();
        mTestMappingRelativePaths.addAll(relativePaths);
    }


    /**
     * Constructor to create a {@link TestMapping} object from a path to TEST_MAPPING file.
     *
     * @param path The {@link Path} to a TEST_MAPPING file.
     * @param testMappingsDir The {@link Path} to the folder of all TEST_MAPPING files for a build.
     */
    public TestMapping(Path path, Path testMappingsDir) {
        mTestCollection = new LinkedHashMap<>();
        String relativePath = testMappingsDir.relativize(path.getParent()).toString();
        String errorMessage = null;
        try {
            String content = removeComments(
                    String.join("\n", Files.readAllLines(path, StandardCharsets.UTF_8)));
            if (content != null) {
                JSONTokener tokener = new JSONTokener(content);
                JSONObject root = new JSONObject(tokener);
                Iterator<String> testGroups = (Iterator<String>) root.keys();
                while (testGroups.hasNext()) {
                    String group = testGroups.next();
                    if (group.equals(IMPORTS)) {
                        // TF runs tests in all TEST_MAPPING files in a build, so imports do not
                        // need to be considered.
                        continue;
                    }
                    Set<TestInfo> testsForGroup = new HashSet<>();
                    mTestCollection.put(group, testsForGroup);
                    JSONArray arr = root.getJSONArray(group);
                    for (int i = 0; i < arr.length(); i++) {
                        JSONObject testObject = arr.getJSONObject(i);
                        boolean hostOnly =
                                testObject.has(KEY_HOST) && testObject.getBoolean(KEY_HOST);
                        Set<String> keywords = new HashSet<>();
                        if (testObject.has(KEY_KEYWORDS)) {
                            JSONArray keywordArray = testObject.getJSONArray(KEY_KEYWORDS);
                            for (int j = 0; j < keywordArray.length(); j++) {
                                keywords.add(keywordArray.getString(j));
                            }
                        }
                        TestInfo test =
                                new TestInfo(
                                        testObject.getString(KEY_NAME),
                                        relativePath,
                                        hostOnly,
                                        keywords);
                        if (testObject.has(KEY_OPTIONS)) {
                            JSONArray optionObjects = testObject.getJSONArray(KEY_OPTIONS);
                            for (int j = 0; j < optionObjects.length(); j++) {
                                JSONObject optionObject = optionObjects.getJSONObject(j);
                                for (int k = 0; k < optionObject.names().length(); k++) {
                                    String name = optionObject.names().getString(k);
                                    String value = optionObject.getString(name);
                                    TestOption option = new TestOption(name, value);
                                    test.addOption(option);
                                }
                            }
                        }
                        testsForGroup.add(test);
                    }
                }
            }
        } catch (IOException e) {
            errorMessage = String.format("TEST_MAPPING file does not exist: %s.", path.toString());
            CLog.e(errorMessage);
        } catch (JSONException e) {
            errorMessage =
                    String.format(
                            "Error parsing TEST_MAPPING file: %s. Error: %s", path.toString(), e);
        }

        if (errorMessage != null) {
            CLog.e(errorMessage);
            throw new RuntimeException(errorMessage);
        }
    }

    /**
     * Helper to remove comments in a TEST_MAPPING file to valid format. Only "//" and "#" are
     * regarded as comments.
     *
     * @param jsonContent A {@link String} of json which content is from a TEST_MAPPING file.
     * @return A {@link String} of valid json without comments.
     */
    @VisibleForTesting
    static String removeComments(String jsonContent) {
        StringBuffer out = new StringBuffer();
        Matcher matcher = COMMENTS_REGEX.matcher(jsonContent);
        while (matcher.find()) {
            if (COMMENTS.contains(matcher.group(1))) {
                matcher.appendReplacement(out, "");
            }
        }
        matcher.appendTail(out);
        return out.toString();
    }

    /**
     * Helper to get all tests set in a TEST_MAPPING file for a given group.
     *
     * @param testGroup A {@link String} of the test group.
     * @param disabledTests A set of {@link String} for the name of the disabled tests.
     * @param hostOnly true if only tests running on host and don't require device should be
     *     returned. false to return tests that require device to run.
     * @param keywords A set of {@link String} to be matched when filtering tests to run in a Test
     *     Mapping suite.
     * @return A {@code Set<TestInfo>} of the test infos.
     */
    public Set<TestInfo> getTests(
            String testGroup, Set<String> disabledTests, boolean hostOnly, Set<String> keywords) {
        Set<TestInfo> tests = new HashSet<TestInfo>();

        for (TestInfo test : mTestCollection.getOrDefault(testGroup, new HashSet<>())) {
            if (disabledTests != null && disabledTests.contains(test.getName())) {
                continue;
            }
            if (test.getHostOnly() != hostOnly) {
                continue;
            }
            // Skip the test if no keyword is specified but the test requires certain keywords.
            if ((keywords == null || keywords.isEmpty()) && !test.getKeywords().isEmpty()) {
                continue;
            }
            // Skip the test if any of the required keywords is not specified by the test.
            if (keywords != null) {
                Boolean allKeywordsFound = true;
                for (String keyword : keywords) {
                    if (!test.getKeywords().contains(keyword)) {
                        allKeywordsFound = false;
                        break;
                    }
                }
                // The test should be skipped if any keyword is missing in the test configuration.
                if (!allKeywordsFound) {
                    continue;
                }
            }
            tests.add(test);
        }

        return tests;
    }

    /**
     * Merge multiple tests if there are any for the same test module, but with different test
     * options.
     *
     * @param tests A {@code Set<TestInfo>} of the test infos to be processed.
     * @return A {@code Set<TestInfo>} of tests that each is for a unique test module.
     */
    private static Set<TestInfo> mergeTests(Set<TestInfo> tests) {
        Map<String, List<TestInfo>> testsGroupedbyNameAndHost =
                tests.stream()
                        .collect(
                                Collectors.groupingBy(
                                        TestInfo::getNameAndHostOnly, Collectors.toList()));

        Set<TestInfo> mergedTests = new HashSet<>();
        for (List<TestInfo> multiTests : testsGroupedbyNameAndHost.values()) {
            TestInfo mergedTest = multiTests.get(0);
            if (multiTests.size() > 1) {
                for (TestInfo test : multiTests.subList(1, multiTests.size())) {
                    mergedTest.merge(test);
                }
            }
            mergedTests.add(mergedTest);
        }

        return mergedTests;
    }

    /**
     * Helper to get all TEST_MAPPING paths relative to TEST_MAPPINGS_ZIP.
     *
     * @param testMappingsRootPath The {@link Path} to a test mappings zip path.
     * @return A {@code Set<Path>} of all the TEST_MAPPING paths relative to TEST_MAPPINGS_ZIP.
     */
    @VisibleForTesting
    static Set<Path> getAllTestMappingPaths(Path testMappingsRootPath) {
        Set<Path> allTestMappingPaths = new HashSet<>();
        for (String path : mTestMappingRelativePaths) {
            boolean hasAdded = false;
            Path testMappingPath = testMappingsRootPath.resolve(path);
            // Recursively find the TEST_MAPPING file until reaching to testMappingsRootPath.
            while (!testMappingPath.equals(testMappingsRootPath)) {
                if (testMappingPath.resolve(TEST_MAPPING).toFile().exists()) {
                    hasAdded = true;
                    CLog.d("Adding TEST_MAPPING path: %s", testMappingPath);
                    allTestMappingPaths.add(testMappingPath.resolve(TEST_MAPPING));
                }
                testMappingPath = testMappingPath.getParent();
            }
            if (!hasAdded) {
                CLog.w("Couldn't find TEST_MAPPING files from %s", path);
            }
        }
        if (allTestMappingPaths.isEmpty()) {
            throw new RuntimeException(
                    String.format(
                            "Couldn't find TEST_MAPPING files from %s", mTestMappingRelativePaths));
        }
        return allTestMappingPaths;
    }

    /**
     * Helper to find all tests in all TEST_MAPPING files. This is needed when a suite run requires
     * to run all tests in TEST_MAPPING files for a given group, e.g., presubmit.
     *
     * @param buildInfo the {@link IBuildInfo} describing the build.
     * @param testGroup a {@link String} of the test group.
     * @param hostOnly true if only tests running on host and don't require device should be
     *     returned. false to return tests that require device to run.
     * @return A {@code Set<TestInfo>} of tests set in the build artifact, test_mappings.zip.
     */
    public static Set<TestInfo> getTests(
            IBuildInfo buildInfo, String testGroup, boolean hostOnly, Set<String> keywords) {
        Set<TestInfo> tests = new HashSet<TestInfo>();
        File testMappingsDir = extractTestMappingsZip(buildInfo.getFile(TEST_MAPPINGS_ZIP));
        Stream<Path> stream = null;
        try {
            Path testMappingsRootPath = Paths.get(testMappingsDir.getAbsolutePath());
            Set<String> disabledTests = getDisabledTests(testMappingsRootPath, testGroup);
            if (mTestMappingRelativePaths.isEmpty()) {
                stream = Files.walk(testMappingsRootPath, FileVisitOption.FOLLOW_LINKS);
            }
            else {
                stream = getAllTestMappingPaths(testMappingsRootPath).stream();
            }
            stream.filter(path -> path.getFileName().toString().equals(TEST_MAPPING))
                    .forEach(
                            path ->
                                    tests.addAll(
                                            new TestMapping(path, testMappingsRootPath)
                                                    .getTests(
                                                            testGroup,
                                                            disabledTests,
                                                            hostOnly,
                                                            keywords)));

        } catch (IOException e) {
            throw new RuntimeException(
                    String.format(
                            "IO exception (%s) when reading tests from TEST_MAPPING files (%s)",
                            e.getMessage(), testMappingsDir.getAbsolutePath()), e);
        } finally {
            if (stream != null) {
                stream.close();
            }
            FileUtil.recursiveDelete(testMappingsDir);
        }

        return tests;
    }

    /**
     * Helper to find all tests in the TEST_MAPPING files from a given directory.
     *
     * @param testMappingsDir the {@link File} the directory containing all Test Mapping files.
     * @return A {@code Map<String, Set<TestInfo>>} of tests in the given directory and its child
     *     directories.
     */
    public static Map<String, Set<TestInfo>> getAllTests(File testMappingsDir) {
        Map<String, Set<TestInfo>> allTests = new HashMap<String, Set<TestInfo>>();
        Stream<Path> stream = null;
        try {
            Path testMappingsRootPath = Paths.get(testMappingsDir.getAbsolutePath());
            stream = Files.walk(testMappingsRootPath, FileVisitOption.FOLLOW_LINKS);
            stream.filter(path -> path.getFileName().toString().equals(TEST_MAPPING))
                    .forEach(
                            path ->
                                    getAllTests(allTests, path, testMappingsRootPath));

        } catch (IOException e) {
            throw new RuntimeException(
                    String.format(
                            "IO exception (%s) when reading tests from TEST_MAPPING files (%s)",
                            e.getMessage(), testMappingsDir.getAbsolutePath()), e);
        } finally {
            if (stream != null) {
                stream.close();
            }
        }
        return allTests;
    }

    /**
     * Extract a zip file and return the directory that contains the content of unzipped files.
     *
     * @param testMappingsZip A {@link File} of the test mappings zip to extract.
     * @return a {@link File} pointing to the temp directory for test mappings zip.
     */
    public static File extractTestMappingsZip(File testMappingsZip) {
        File testMappingsDir = null;
        try {
            testMappingsDir = ZipUtil2.extractZipToTemp(testMappingsZip, TEST_MAPPINGS_ZIP);
        } catch (IOException e) {
            throw new RuntimeException(
                    String.format(
                            "IO exception (%s) when extracting test mappings zip (%s)",
                            e.getMessage(), testMappingsZip.getAbsolutePath()), e);
        }
        return testMappingsDir;
    }

    /**
     * Get disabled tests from test mapping artifact.
     *
     * @param testMappingsRootPath The {@link Path} to a test mappings zip path.
     * @param testGroup a {@link String} of the test group.
     * @return a {@link Set<String>} containing all the disabled presubmit tests. No test is
     *     returned if the testGroup is not PRESUBMIT.
     */
    @VisibleForTesting
    static Set<String> getDisabledTests(Path testMappingsRootPath, String testGroup) {
        Set<String> disabledTests = new HashSet<>();
        File disabledPresubmitTestsFile =
                new File(testMappingsRootPath.toString(), DISABLED_PRESUBMIT_TESTS_FILE);
        if (!(testGroup.equals(PRESUBMIT) && disabledPresubmitTestsFile.exists())) {
            return disabledTests;
        }
        try {
            disabledTests.addAll(
                    Arrays.asList(
                            FileUtil.readStringFromFile(disabledPresubmitTestsFile)
                                    .split("\\r?\\n")));
        } catch (IOException e) {
            throw new RuntimeException(
                    String.format(
                            "IO exception (%s) when reading disabled tests from file (%s)",
                            e.getMessage(), disabledPresubmitTestsFile.getAbsolutePath()), e);
        }
        return disabledTests;
    }

    /**
     * Helper to find all tests in the TEST_MAPPING files from a given directory.
     *
     * @param allTests the {@code HashMap<String, Set<TestInfo>>} containing the tests of each
     * test group.
     * @param path the {@link Path} to a TEST_MAPPING file.
     * @param testMappingsRootPath the {@link Path} to a test mappings zip path.
     */
    private static void getAllTests(Map<String, Set<TestInfo>> allTests,
        Path path, Path testMappingsRootPath) {
        Map<String, Set<TestInfo>> testCollection =
            new TestMapping(path, testMappingsRootPath).getTestCollection();
        for (String group : testCollection.keySet()) {
            allTests.computeIfAbsent(group, k -> new HashSet<>()).addAll(testCollection.get(group));
        }
    }

    /**
     * Helper to get the test collection in a TEST_MAPPING file.
     *
     * @return A {@code Map<String, Set<TestInfo>>} containing the test collection in a
     *     TEST_MAPPING file.
     */
    private Map<String, Set<TestInfo>> getTestCollection() {
        return mTestCollection;
    }
}
