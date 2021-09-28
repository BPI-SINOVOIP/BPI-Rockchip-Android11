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
package com.android.tradefed.cluster;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.util.FileUtil;

import com.android.tradefed.cluster.ClusterLogSaver.FilePickingStrategy;

import org.json.JSONException;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/** Unit tests for {@link ClusterLogSaverTest}. */
@RunWith(JUnit4.class)
public class ClusterLogSaverTest {

    private static final String REQUEST_ID = "request_id";
    private static final String COMMAND_ID = "command_id";

    private File mWorkDir;
    private TestOutputUploader mMockTestOutputUploader;
    private IClusterClient mMockClusterClient;
    private ClusterLogSaver mClusterLogSaver;
    private OptionSetter mOptionSetter;

    @Before
    public void setUp() throws Exception {
        mWorkDir = FileUtil.createTempDir(this.getClass().getName());
        mMockTestOutputUploader = Mockito.mock(TestOutputUploader.class);
        mMockClusterClient = Mockito.mock(IClusterClient.class);
        mClusterLogSaver = Mockito.spy(new ClusterLogSaver());
        Mockito.doReturn(mMockTestOutputUploader).when(mClusterLogSaver).getTestOutputUploader();
        Mockito.doReturn(mMockClusterClient).when(mClusterLogSaver).getClusterClient();
        mOptionSetter = new OptionSetter(mClusterLogSaver);
        mOptionSetter.setOptionValue("cluster:root-dir", mWorkDir.getAbsolutePath());
        mOptionSetter.setOptionValue("cluster:request-id", REQUEST_ID);
        mOptionSetter.setOptionValue("cluster:command-id", COMMAND_ID);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mWorkDir);
    }

    /** Create an empty file in the test's temporary directory. */
    private File createMockFile(String path, String name) throws IOException {
        final File dir = new File(mWorkDir, path);
        dir.mkdirs();
        final File file = new File(dir, name);
        file.createNewFile();
        return file;
    }

    /** Create an empty zip file in the test's temporary directory. */
    private File createMockZipFile(String path, String name) throws IOException {
        File file = createMockFile(path, name);
        new ZipOutputStream(new FileOutputStream(file)).close();
        return file;
    }

    /** Get the names of all entries in a zip file. */
    private Set<String> getZipEntries(File file) throws IOException {
        try (ZipInputStream zip = new ZipInputStream(new FileInputStream(file))) {
            Set<String> names = new LinkedHashSet<>();
            for (ZipEntry entry = zip.getNextEntry(); entry != null; entry = zip.getNextEntry()) {
                names.add(entry.getName());
            }
            return names;
        }
    }

    @Test
    public void testFindTestContextFile() throws IOException {
        final String pattern = "android-cts/results/(?<SESSION>[^/]+)/test_result\\.xml";
        final String sessionId = "1970.01.01_00.05.10";
        final File file =
                createMockFile(
                        String.format("android-cts/results/%s", sessionId), "test_result.xml");
        Map<String, String> envVars = new TreeMap<>();

        File contextFile =
                mClusterLogSaver.findTestContextFile(
                        mWorkDir, pattern, FilePickingStrategy.PICK_LAST, envVars);

        assertNotNull(contextFile);
        assertEquals(file.getAbsolutePath(), contextFile.getAbsolutePath());
        assertEquals(1, envVars.size());
        assertEquals(sessionId, envVars.get("SESSION"));
    }

    @Test
    public void testFindTestContextFile_multipleMatches_pickFirst() throws IOException {
        final String pattern = "android-cts/results/(?<SESSION>[^/]+)/test_result\\.xml";
        final String[] sessionIds = new String[] {"1970.01.01_00.05.10", "2018.01.01_00.05.10"};
        final File[] files =
                new File[] {
                    createMockFile(
                            String.format("android-cts/results/%s", sessionIds[0]),
                            "test_result.xml"),
                    createMockFile(
                            String.format("android-cts/results/%s", sessionIds[1]),
                            "test_result.xml")
                };
        Map<String, String> envVars = new TreeMap<>();

        File contextFile =
                mClusterLogSaver.findTestContextFile(
                        mWorkDir, pattern, FilePickingStrategy.PICK_FIRST, envVars);

        assertNotNull(contextFile);
        assertEquals(files[0].getAbsolutePath(), contextFile.getAbsolutePath());
        assertEquals(1, envVars.size());
        assertEquals(sessionIds[0], envVars.get("SESSION"));
    }

    @Test
    public void testFindTestContextFile_multipleMatches_pickLast() throws IOException {
        final String pattern = "android-cts/results/(?<SESSION>[^/]+)/test_result\\.xml";
        final String[] sessionIds = new String[] {"1970.01.01_00.05.10", "2018.01.01_00.05.10"};
        final File[] files =
                new File[] {
                    createMockFile(
                            String.format("android-cts/results/%s", sessionIds[0]),
                            "test_result.xml"),
                    createMockFile(
                            String.format("android-cts/results/%s", sessionIds[1]),
                            "test_result.xml")
                };
        Map<String, String> envVars = new TreeMap<>();

        File contextFile =
                mClusterLogSaver.findTestContextFile(
                        mWorkDir, pattern, FilePickingStrategy.PICK_LAST, envVars);

        assertNotNull(contextFile);
        assertEquals(files[1].getAbsolutePath(), contextFile.getAbsolutePath());
        assertEquals(1, envVars.size());
        assertEquals(sessionIds[1], envVars.get("SESSION"));
    }

    @Test
    public void testFindTestContextFile_noMatch() throws IOException {
        final String pattern = "android-cts/results/(?<SESSION>[^/]+)/test_result\\.xml";
        final String sessionId = "1970.01.01_00.05.10";
        createMockFile(String.format("android-cts/results/%s", sessionId), "test_result2.xml");
        Map<String, String> envVars = new TreeMap<>();

        File contextFile =
                mClusterLogSaver.findTestContextFile(
                        mWorkDir, pattern, FilePickingStrategy.PICK_LAST, envVars);

        assertNull(contextFile);
        assertEquals(0, envVars.size());
    }

    @Test
    public void testFindTestContextFile_existingEnvVar() throws IOException {
        final String pattern = "android-cts/results/(?<SESSION>[^/]+)/test_result\\.xml";
        final String sessionId = "1970.01.01_00.05.10";
        final File file =
                createMockFile(
                        String.format("android-cts/results/%s", sessionId), "test_result.xml");
        Map<String, String> envVars = new TreeMap<>();
        envVars.put("SESSION", "foo");

        File contextFile =
                mClusterLogSaver.findTestContextFile(
                        mWorkDir, pattern, FilePickingStrategy.PICK_LAST, envVars);

        assertNotNull(contextFile);
        assertEquals(file.getAbsolutePath(), contextFile.getAbsolutePath());
        assertEquals(1, envVars.size());
        assertEquals(sessionId, envVars.get("SESSION"));
    }

    @Test
    public void testInvocationEnded() throws IOException, ConfigurationException, JSONException {
        final InvocationContext invocationContext = new InvocationContext();
        final String outputFileUploadUrl = "output_file_upload_url";
        final String retryCommandLine = "retry_command_line";
        // create output files (host_log_\d+ will be skipped)
        File fooTxtOutputFile = createMockFile("logs", "foo.txt");
        File fooHtmlOutputFile = createMockFile("results", "foo.html");
        File fooCtxOutputFile = createMockFile("context", "foo.ctx");
        createMockFile("logs", "host_log_123456.txt");
        mOptionSetter.setOptionValue("cluster:output-file-upload-url", outputFileUploadUrl);
        mOptionSetter.setOptionValue("cluster:output-file-pattern", ".*\\.html");
        mOptionSetter.setOptionValue("cluster:context-file-pattern", ".*\\.ctx");
        mOptionSetter.setOptionValue("cluster:retry-command-line", retryCommandLine);
        final String testOutputUrl = "test_output_url";
        Mockito.doReturn(testOutputUrl)
                .when(mMockTestOutputUploader)
                .uploadFile(Mockito.any(), Mockito.any());

        mClusterLogSaver.invocationStarted(invocationContext);
        mClusterLogSaver.invocationEnded(0);

        // verify that file names are properly stored including any path prefix
        final File fileNamesFile = new File(mWorkDir, ClusterLogSaver.FILE_NAMES_FILE_NAME);
        String expectedFileNames =
                Stream.of("tool-logs/foo.txt", "foo.html", "foo.ctx")
                        .sorted()
                        .collect(Collectors.joining("\n"));
        assertEquals(expectedFileNames, FileUtil.readStringFromFile(fileNamesFile));

        Mockito.verify(mMockTestOutputUploader).setUploadUrl(outputFileUploadUrl);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fileNamesFile, null);
        Mockito.verify(mMockTestOutputUploader)
                .uploadFile(fooTxtOutputFile, ClusterLogSaver.TOOL_LOG_PATH);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fooHtmlOutputFile, null);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fooCtxOutputFile, null);
        TestContext expTextContext = new TestContext();
        expTextContext.addTestResource(new TestResource("context/foo.ctx", testOutputUrl));
        expTextContext.setCommandLine(retryCommandLine);
        Mockito.verify(mMockClusterClient)
                .updateTestContext(REQUEST_ID, COMMAND_ID, expTextContext);
    }

    @Test
    public void testInvocationEnded_uploadError() throws IOException, ConfigurationException {
        final InvocationContext invocationContext = new InvocationContext();
        final String outputFileUploadUrl = "output_file_upload_url";
        createMockFile("", ClusterLogSaver.FILE_NAMES_FILE_NAME);
        File fooTxtOutputFile = createMockFile("logs", "foo.txt");
        File barTxtOutputFile = createMockFile("logs", "bar.txt");
        File fooCtxOutputFile = createMockFile("context", "foo.ctx");
        mOptionSetter.setOptionValue("cluster:context-file-pattern", ".*\\.ctx");
        mOptionSetter.setOptionValue("cluster:output-file-upload-url", outputFileUploadUrl);
        final String testOutputUrl = "test_output_url";
        Mockito.doReturn(testOutputUrl)
                .doThrow(RuntimeException.class)
                .doReturn(testOutputUrl)
                .when(mMockTestOutputUploader)
                .uploadFile(Mockito.any(), Mockito.any());

        mClusterLogSaver.invocationStarted(invocationContext);
        mClusterLogSaver.invocationEnded(0);

        // verify that file names are properly stored including any path prefix
        final File fileNamesFile = new File(mWorkDir, ClusterLogSaver.FILE_NAMES_FILE_NAME);
        String expectedFileNames =
                Stream.of("tool-logs/foo.txt", "tool-logs/bar.txt", "foo.ctx")
                        .sorted()
                        .collect(Collectors.joining("\n"));
        assertEquals(expectedFileNames, FileUtil.readStringFromFile(fileNamesFile));

        Mockito.verify(mMockTestOutputUploader).setUploadUrl(outputFileUploadUrl);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fileNamesFile, null);
        Mockito.verify(mMockTestOutputUploader)
                .uploadFile(fooTxtOutputFile, ClusterLogSaver.TOOL_LOG_PATH);
        Mockito.verify(mMockTestOutputUploader)
                .uploadFile(barTxtOutputFile, ClusterLogSaver.TOOL_LOG_PATH);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fooCtxOutputFile, null);
    }

    @Test
    public void testInvocationEnded_duplicateUpload() throws IOException, ConfigurationException {
        String outputFileUploadUrl = "output_file_upload_url";
        mOptionSetter.setOptionValue("cluster:output-file-upload-url", outputFileUploadUrl);

        // create two files with same destination, only the second should get picked
        mOptionSetter.setOptionValue("cluster:output-file-pattern", ".*\\.txt");
        mOptionSetter.setOptionValue("cluster:file-picking-strategy", "PICK_LAST");
        File first = createMockFile("first", "foo.txt");
        File second = createMockFile("second", "foo.txt");

        InvocationContext invocationContext = new InvocationContext();
        mClusterLogSaver.invocationStarted(invocationContext);
        mClusterLogSaver.invocationEnded(0);

        // verify that only one file is recorded in the filename file
        File fileNamesFile = new File(mWorkDir, ClusterLogSaver.FILE_NAMES_FILE_NAME);
        assertEquals("foo.txt", FileUtil.readStringFromFile(fileNamesFile));

        // verify that only the second file is uploaded (and filename file)
        Mockito.verify(mMockTestOutputUploader).setUploadUrl(outputFileUploadUrl);
        Mockito.verify(mMockTestOutputUploader).uploadFile(fileNamesFile, null);
        Mockito.verify(mMockTestOutputUploader).uploadFile(second, null);
        Mockito.verifyNoMoreInteractions(mMockTestOutputUploader);
    }

    @Test
    public void testAppendFilesToContext() throws IOException {
        // create context file
        File contextFile = createMockZipFile("context", "context.zip");

        // create files to append
        File first = createMockFile("extra", "1.txt");
        File second = createMockFile("extra", "2.txt");

        // append files using absolute, relative, and unknown paths
        mClusterLogSaver.appendFilesToContext(
                contextFile, Arrays.asList(first.getAbsolutePath(), "extra/2.txt", "unknown.txt"));

        // verify that context file contains the additional files
        Set<String> entries = getZipEntries(contextFile);
        assertTrue(entries.contains("1.txt"));
        assertTrue(entries.contains("2.txt"));
        assertFalse(entries.contains("unknown.txt")); // unknown file ignored
    }
}
