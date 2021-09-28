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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil;

import com.google.common.collect.Sets;

import org.easymock.EasyMock;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link TestMapping}. */
@RunWith(JUnit4.class)
public class TestMappingTest {

    private static final String TEST_DATA_DIR = "testdata";
    private static final String TEST_MAPPING = "TEST_MAPPING";
    private static final String TEST_MAPPINGS_ZIP = "test_mappings.zip";
    private static final String DISABLED_PRESUBMIT_TESTS = "disabled-presubmit-tests";

    /** Test for {@link TestMapping#getTests()} implementation. */
    @Test
    public void testparseTestMapping() throws Exception {
        File tempDir = null;
        File testMappingFile = null;

        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            File testMappingRootDir = FileUtil.createTempDir("subdir", tempDir);
            String rootDirName = testMappingRootDir.getName();
            testMappingFile =
                    FileUtil.saveResourceFile(resourceStream, testMappingRootDir, TEST_MAPPING);
            Set<TestInfo> tests =
                    new TestMapping(testMappingFile.toPath(), Paths.get(tempDir.getAbsolutePath()))
                            .getTests("presubmit", null, true, null);
            assertEquals(1, tests.size());
            Set<String> names = new HashSet<String>();
            for (TestInfo test : tests) {
                names.add(test.getName());
            }
            assertTrue(names.contains("test1"));
            tests =
                    new TestMapping(testMappingFile.toPath(), Paths.get(tempDir.getAbsolutePath()))
                            .getTests("presubmit", null, false, null);
            assertEquals(1, tests.size());
            names = new HashSet<String>();
            for (TestInfo test : tests) {
                names.add(test.getName());
            }
            assertTrue(names.contains("suite/stub1"));
            tests =
                    new TestMapping(testMappingFile.toPath(), Paths.get(tempDir.getAbsolutePath()))
                            .getTests("postsubmit", null, false, null);
            assertEquals(2, tests.size());
            TestOption testOption =
                    new TestOption(
                            "instrumentation-arg",
                            "annotation=android.platform.test.annotations.Presubmit");
            names = new HashSet<String>();
            Set<TestOption> testOptions = new HashSet<TestOption>();
            for (TestInfo test : tests) {
                names.add(test.getName());
                testOptions.addAll(test.getOptions());
            }
            assertTrue(names.contains("test2"));
            assertTrue(names.contains("instrument"));
            assertTrue(testOptions.contains(testOption));
            tests =
                    new TestMapping(testMappingFile.toPath(), Paths.get(tempDir.getAbsolutePath()))
                            .getTests("othertype", null, false, null);
            assertEquals(1, tests.size());
            names = new HashSet<String>();
            testOptions = new HashSet<TestOption>();
            Set<String> sources = new HashSet<String>();
            for (TestInfo test : tests) {
                names.add(test.getName());
                testOptions.addAll(test.getOptions());
                sources.addAll(test.getSources());
            }
            assertTrue(names.contains("test3"));
            assertEquals(1, testOptions.size());
            assertTrue(sources.contains(rootDirName));
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Test for {@link TestMapping#getTests()} throw exception for malformated json file. */
    @Test(expected = RuntimeException.class)
    public void testparseTestMapping_BadJson() throws Exception {
        File tempDir = null;

        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            File testMappingFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPING).toFile();
            FileUtil.writeToFile("bad format json file", testMappingFile);
            Set<TestInfo> tests =
                    new TestMapping(testMappingFile.toPath(), Paths.get(tempDir.getAbsolutePath()))
                            .getTests("presubmit", null, false, null);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Test for {@link TestMapping#getTests()} for loading tests from test_mappings.zip. */
    @Test
    public void testGetTests() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);
            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);
            IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile).times(2);

            EasyMock.replay(mockBuildInfo);

            // Ensure the static variable doesn't have any relative path configured.
            TestMapping.setTestMappingPaths(new ArrayList<String>());
            Set<TestInfo> tests = TestMapping.getTests(mockBuildInfo, "presubmit", false, null);
            assertEquals(0, tests.size());

            tests = TestMapping.getTests(mockBuildInfo, "presubmit", true, null);
            assertEquals(2, tests.size());
            Set<String> names = new HashSet<String>();
            for (TestInfo test : tests) {
                names.add(test.getName());
                if (test.getName().equals("test1")) {
                    assertTrue(test.getHostOnly());
                } else {
                    assertFalse(test.getHostOnly());
                }
            }
            assertTrue(!names.contains("suite/stub1"));
            assertTrue(names.contains("test1"));
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMapping#getTests()} for loading tests from test_mappings.zip for matching
     * keywords.
     */
    @Test
    public void testGetTests_matchKeywords() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);
            List<File> filesToZip =
                    Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);
            IBuildInfo mockBuildInfo = EasyMock.createMock(IBuildInfo.class);
            EasyMock.expect(mockBuildInfo.getFile(TEST_MAPPINGS_ZIP)).andReturn(zipFile).times(2);

            EasyMock.replay(mockBuildInfo);

            Set<TestInfo> tests =
                    TestMapping.getTests(
                            mockBuildInfo, "presubmit", false, Sets.newHashSet("key_1"));
            assertEquals(1, tests.size());
            assertEquals("suite/stub2", tests.iterator().next().getName());
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMapping#getAllTestMappingPaths(Path)} to get TEST_MAPPING files from
     * child directory.
     */
    @Test
    public void testGetAllTestMappingPaths_FromChildDirectory() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            Path testMappingsRootPath = Paths.get(tempDir.getAbsolutePath());
            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<String> testMappingRelativePaths = new ArrayList<>();
            Path relPath = testMappingsRootPath.relativize(Paths.get(subDir.getAbsolutePath()));
            testMappingRelativePaths.add(relPath.toString());
            TestMapping.setTestMappingPaths(testMappingRelativePaths);
            Set<Path> paths = TestMapping.getAllTestMappingPaths(testMappingsRootPath);
            assertEquals(2, paths.size());
        } finally {
            TestMapping.setTestMappingPaths(new ArrayList<>());
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMapping#getAllTestMappingPaths(Path)} to get TEST_MAPPING files from
     * parent directory.
     */
    @Test
    public void testGetAllTestMappingPaths_FromParentDirectory() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            Path testMappingsRootPath = Paths.get(tempDir.getAbsolutePath());
            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);

            List<String> testMappingRelativePaths = new ArrayList<>();
            Path relPath = testMappingsRootPath.relativize(Paths.get(srcDir.getAbsolutePath()));
            testMappingRelativePaths.add(relPath.toString());
            TestMapping.setTestMappingPaths(testMappingRelativePaths);
            Set<Path> paths = TestMapping.getAllTestMappingPaths(testMappingsRootPath);
            assertEquals(1, paths.size());
        } finally {
            TestMapping.setTestMappingPaths(new ArrayList<>());
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestMapping#getAllTestMappingPaths(Path)} to fail when no TEST_MAPPING files
     * found.
     */
    @Test(expected = RuntimeException.class)
    public void testGetAllTestMappingPaths_NoFilesFound() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            Path testMappingsRootPath = Paths.get(tempDir.getAbsolutePath());
            File srcDir = FileUtil.createTempDir("src", tempDir);

            List<String> testMappingRelativePaths = new ArrayList<>();
            Path relPath = testMappingsRootPath.relativize(Paths.get(srcDir.getAbsolutePath()));
            testMappingRelativePaths.add(relPath.toString());
            TestMapping.setTestMappingPaths(testMappingRelativePaths);
            // No TEST_MAPPING files should be found according to the srcDir, getAllTestMappingPaths
            // method shall raise RuntimeException.
            TestMapping.getAllTestMappingPaths(testMappingsRootPath);
        } finally {
            TestMapping.setTestMappingPaths(new ArrayList<>());
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects to fail when module names
     * are different.
     */
    @Test(expected = RuntimeException.class)
    public void testMergeFailByName() throws Exception {
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test2", "folder1", false);
        test1.merge(test2);
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects to fail when device
     * requirements are different.
     */
    @Test(expected = RuntimeException.class)
    public void testMergeFailByHostOnly() throws Exception {
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test2", "folder1", true);
        test1.merge(test2);
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects, one of which has no
     * option.
     */
    @Test
    public void testMergeSuccess() throws Exception {
        // Check that the test without any option should be the merge result.
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test1", "folder1", false);
        test2.addOption(new TestOption("include-filter", "value"));
        test1.merge(test2);
        assertTrue(test1.getOptions().isEmpty());
        assertFalse(test1.getHostOnly());

        test1 = new TestInfo("test1", "folder1", false);
        test2 = new TestInfo("test1", "folder1", false);
        test1.addOption(new TestOption("include-filter", "value"));
        test1.merge(test2);
        assertTrue(test1.getOptions().isEmpty());
        assertFalse(test1.getHostOnly());

        test1 = new TestInfo("test1", "folder1", true);
        test2 = new TestInfo("test1", "folder1", true);
        test1.addOption(new TestOption("include-filter", "value"));
        test1.merge(test2);
        assertTrue(test1.getOptions().isEmpty());
        assertTrue(test1.getHostOnly());
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects, each has a different
     * include-filter.
     */
    @Test
    public void testMergeSuccess_2Filters() throws Exception {
        // Check that the test without any option should be the merge result.
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test1", "folder2", false);
        TestOption option1 = new TestOption("include-filter", "value1");
        test1.addOption(option1);
        TestOption option2 = new TestOption("include-filter", "value2");
        test2.addOption(option2);
        test1.merge(test2);
        assertEquals(2, test1.getOptions().size());
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option1));
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option2));
        assertEquals(2, test1.getSources().size());
        assertTrue(test1.getSources().contains("folder1"));
        assertTrue(test1.getSources().contains("folder2"));
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects, each has mixed
     * include-filter and exclude-filter.
     */
    @Test
    public void testMergeSuccess_multiFilters() throws Exception {
        // Check that the test without any option should be the merge result.
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test1", "folder2", false);
        TestOption inclusiveOption1 = new TestOption("include-filter", "value1");
        test1.addOption(inclusiveOption1);
        TestOption exclusiveOption1 = new TestOption("exclude-filter", "exclude-value1");
        test1.addOption(exclusiveOption1);
        TestOption exclusiveOption2 = new TestOption("exclude-filter", "exclude-value2");
        test1.addOption(exclusiveOption2);
        TestOption otherOption1 = new TestOption("somefilter", "");
        test1.addOption(otherOption1);

        TestOption inclusiveOption2 = new TestOption("include-filter", "value2");
        test2.addOption(inclusiveOption2);
        // Same exclusive option as in test1.
        test2.addOption(exclusiveOption1);
        TestOption exclusiveOption3 = new TestOption("exclude-filter", "exclude-value1");
        test2.addOption(exclusiveOption3);
        TestOption otherOption2 = new TestOption("somefilter2", "value2");
        test2.addOption(otherOption2);

        test1.merge(test2);
        assertEquals(5, test1.getOptions().size());
        Set<TestOption> mergedOptions = new HashSet<TestOption>(test1.getOptions());
        // Options from test1.
        assertTrue(mergedOptions.contains(inclusiveOption1));
        assertTrue(mergedOptions.contains(otherOption1));
        // Shared exclusive option between test1 and test2.
        assertTrue(mergedOptions.contains(exclusiveOption1));
        // Options from test2.
        assertTrue(mergedOptions.contains(inclusiveOption2));
        assertTrue(mergedOptions.contains(otherOption2));
        // Both folders are in sources
        assertEquals(2, test1.getSources().size());
        assertTrue(test1.getSources().contains("folder1"));
        assertTrue(test1.getSources().contains("folder2"));
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects, each has a different
     * include-filter and include-annotation option.
     */
    @Test
    public void testMergeSuccess_MultiFilters_dropIncludeAnnotation() throws Exception {
        // Check that the test without all options except include-annotation option should be the
        // merge result.
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test1", "folder1", false);
        TestOption option1 = new TestOption("include-filter", "value1");
        test1.addOption(option1);
        TestOption optionIncludeAnnotation =
                new TestOption("include-annotation", "androidx.test.filters.FlakyTest");
        test1.addOption(optionIncludeAnnotation);
        TestOption option2 = new TestOption("include-filter", "value2");
        test2.addOption(option2);
        test1.merge(test2);
        assertEquals(2, test1.getOptions().size());
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option1));
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option2));
    }

    /**
     * Test for {@link TestInfo#merge()} for merging two TestInfo objects, each has a different
     * include-filter and exclude-annotation option.
     */
    @Test
    public void testMergeSuccess_MultiFilters_keepExcludeAnnotation() throws Exception {
        // Check that the test without all options including exclude-annotation option should be the
        // merge result.
        TestInfo test1 = new TestInfo("test1", "folder1", false);
        TestInfo test2 = new TestInfo("test1", "folder1", false);
        TestOption option1 = new TestOption("include-filter", "value1");
        test1.addOption(option1);
        TestOption optionExcludeAnnotation1 =
                new TestOption("exclude-annotation", "androidx.test.filters.FlakyTest");
        test1.addOption(optionExcludeAnnotation1);
        TestOption optionExcludeAnnotation2 =
                new TestOption("exclude-annotation", "another-annotation");
        test1.addOption(optionExcludeAnnotation2);
        TestOption option2 = new TestOption("include-filter", "value2");
        test2.addOption(option2);
        TestOption optionExcludeAnnotation3 =
                new TestOption("exclude-annotation", "androidx.test.filters.FlakyTest");
        test1.addOption(optionExcludeAnnotation3);
        test1.merge(test2);
        assertEquals(4, test1.getOptions().size());
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option1));
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(optionExcludeAnnotation1));
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(optionExcludeAnnotation2));
        assertTrue(new HashSet<TestOption>(test1.getOptions()).contains(option2));
    }

    /** Test for {@link TestMapping#getAllTests()} for loading tests from test_mappings directory. */
    @Test
    public void testGetAllTests() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);

            Map<String, Set<TestInfo>> allTests = TestMapping.getAllTests(tempDir);
            Set<TestInfo> tests = allTests.get("presubmit");
            assertEquals(5, tests.size());

            tests = allTests.get("postsubmit");
            assertEquals(4, tests.size());

            tests = allTests.get("othertype");
            assertEquals(1, tests.size());
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Test for {@link TestMapping#extractTestMappingsZip()} for extracting test mappings zip. */
    @Test
    public void testExtractTestMappingsZip() throws Exception {
        File tempDir = null;
        File extractedFile = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);
            List<File> filesToZip =
                Arrays.asList(srcDir, new File(tempDir, DISABLED_PRESUBMIT_TESTS));

            File zipFile = Paths.get(tempDir.getAbsolutePath(), TEST_MAPPINGS_ZIP).toFile();
            ZipUtil.createZip(filesToZip, zipFile);

            extractedFile = TestMapping.extractTestMappingsZip(zipFile);
            Map<String, Set<TestInfo>> allTests = TestMapping.getAllTests(tempDir);
            Set<TestInfo> tests = allTests.get("presubmit");
            assertEquals(5, tests.size());

            tests = allTests.get("postsubmit");
            assertEquals(4, tests.size());

            tests = allTests.get("othertype");
            assertEquals(1, tests.size());
        } finally {
            FileUtil.recursiveDelete(tempDir);
            FileUtil.recursiveDelete(extractedFile);
        }
    }

    /** Test for {@link TestMapping#extractTestMappingsZip()} for extracting test mappings zip. */
    @Test
    public void testGetDisabledTests() throws Exception {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");

            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_1";
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            File subDir = FileUtil.createTempDir("sub_dir", srcDir);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + "test_mapping_2";
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, subDir, TEST_MAPPING);
            srcFile = File.separator + TEST_DATA_DIR + File.separator + DISABLED_PRESUBMIT_TESTS;
            resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, tempDir, DISABLED_PRESUBMIT_TESTS);
            Path tempDirPath = Paths.get(tempDir.getAbsolutePath());
            Set<String> disabledTests = TestMapping.getDisabledTests(tempDirPath, "presubmit");
            assertEquals(2, disabledTests.size());

            disabledTests = TestMapping.getDisabledTests(tempDirPath, "postsubmit");
            assertEquals(0, disabledTests.size());

            disabledTests = TestMapping.getDisabledTests(tempDirPath, "othertype");
            assertEquals(0, disabledTests.size());

        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    /** Test for {@link TestMapping#removeComments()} for removing comments in TEST_MAPPING file. */
    @Test
    public void testRemoveComments() throws Exception {
        String jsonString = getJsonStringByName("test_mapping_with_comments1");
        String goldenString = getJsonStringByName("test_mapping_golden1");
        assertEquals(TestMapping.removeComments(jsonString), goldenString);
    }

    /** Test for {@link TestMapping#removeComments()} for removing comments in TEST_MAPPING file. */
    @Test
    public void testRemoveComments2() throws Exception {
        String jsonString = getJsonStringByName("test_mapping_with_comments2");
        String goldenString = getJsonStringByName("test_mapping_golden2");
        assertEquals(TestMapping.removeComments(jsonString), goldenString);
    }

    private String getJsonStringByName(String fileName) throws Exception  {
        File tempDir = null;
        try {
            tempDir = FileUtil.createTempDir("test_mapping");
            File srcDir = FileUtil.createTempDir("src", tempDir);
            String srcFile = File.separator + TEST_DATA_DIR + File.separator + fileName;
            InputStream resourceStream = this.getClass().getResourceAsStream(srcFile);
            FileUtil.saveResourceFile(resourceStream, srcDir, TEST_MAPPING);
            Path file = Paths.get(srcDir.getAbsolutePath(), TEST_MAPPING);
            return String.join("\n", Files.readAllLines(file, StandardCharsets.UTF_8));
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }
}
