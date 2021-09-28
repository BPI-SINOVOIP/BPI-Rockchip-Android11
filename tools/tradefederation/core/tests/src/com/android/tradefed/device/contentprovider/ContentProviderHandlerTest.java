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
package com.android.tradefed.device.contentprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import java.io.File;
import java.io.OutputStream;
import java.util.HashMap;

/** Run unit tests for {@link ContentProviderHandler}. */
@RunWith(JUnit4.class)
public class ContentProviderHandlerTest {

    private ContentProviderHandler mProvider;
    private ITestDevice mMockDevice;

    @Before
    public void setUp() {
        mMockDevice = Mockito.mock(ITestDevice.class);
        mProvider = new ContentProviderHandler(mMockDevice);
    }

    @After
    public void tearDown() throws Exception {
        mProvider.tearDown();
    }

    /** Test the install flow. */
    @Test
    public void testSetUp_install() throws Exception {
        doReturn(1).when(mMockDevice).getCurrentUser();
        doReturn(null).when(mMockDevice).installPackage(any(), eq(true), eq(true));
        CommandResult resSet = new CommandResult(CommandStatus.SUCCESS);
        doReturn(resSet)
                .when(mMockDevice)
                .executeShellV2Command(
                        String.format(
                                "cmd appops set %s android:legacy_storage allow",
                                ContentProviderHandler.PACKAGE_NAME));
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        res.setStdout("LEGACY_STORAGE: allow");
        doReturn(res)
                .when(mMockDevice)
                .executeShellV2Command(
                        String.format("cmd appops get %s", ContentProviderHandler.PACKAGE_NAME));

        assertTrue(mProvider.setUp());
    }

    @Test
    public void testSetUp_alreadyInstalled() throws Exception {
        doReturn(0).when(mMockDevice).getCurrentUser();
        doReturn(true)
                .when(mMockDevice)
                .isPackageInstalled(ContentProviderHandler.PACKAGE_NAME, "0");

        assertTrue(mProvider.setUp());
    }

    @Test
    public void testSetUp_installFail() throws Exception {
        doReturn(1).when(mMockDevice).getCurrentUser();
        doReturn("fail").when(mMockDevice).installPackage(any(), eq(true), eq(true));

        assertFalse(mProvider.setUp());
    }

    /** Test {@link ContentProviderHandler#deleteFile(String)}. */
    @Test
    public void testDeleteFile() throws Exception {
        String devicePath = "path/somewhere/file.txt";
        doReturn(99).when(mMockDevice).getCurrentUser();
        doReturn(mockSuccess())
                .when(mMockDevice)
                .executeShellV2Command(
                        eq(
                                "content delete --user 99 --uri "
                                        + ContentProviderHandler.createEscapedContentUri(
                                                devicePath)));
        assertTrue(mProvider.deleteFile(devicePath));
    }

    /** Test {@link ContentProviderHandler#deleteFile(String)}. */
    @Test
    public void testDeleteFile_fail() throws Exception {
        String devicePath = "path/somewhere/file.txt";
        CommandResult result = new CommandResult(CommandStatus.FAILED);
        result.setStdout("");
        result.setStderr("couldn't find the file");
        doReturn(99).when(mMockDevice).getCurrentUser();
        doReturn(result)
                .when(mMockDevice)
                .executeShellV2Command(
                        eq(
                                "content delete --user 99 --uri "
                                        + ContentProviderHandler.createEscapedContentUri(
                                                devicePath)));
        assertFalse(mProvider.deleteFile(devicePath));
    }

    /** Test {@link ContentProviderHandler#deleteFile(String)}. */
    @Test
    public void testError() throws Exception {
        String devicePath = "path/somewhere/file.txt";
        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        result.setStdout("[ERROR] Unsupported operation: delete");
        doReturn(99).when(mMockDevice).getCurrentUser();
        doReturn(result)
                .when(mMockDevice)
                .executeShellV2Command(
                        eq(
                                "content delete --user 99 --uri "
                                        + ContentProviderHandler.createEscapedContentUri(
                                                devicePath)));
        assertFalse(mProvider.deleteFile(devicePath));
    }

    /** Test {@link ContentProviderHandler#pushFile(File, String)}. */
    @Test
    public void testPushFile() throws Exception {
        File toPush = FileUtil.createTempFile("content-provider-test", ".txt");
        try {
            String devicePath = "path/somewhere/file.txt";
            doReturn(99).when(mMockDevice).getCurrentUser();
            doReturn(mockSuccess())
                    .when(mMockDevice)
                    .executeShellV2Command(
                            eq(
                                    "content write --user 99 --uri "
                                            + ContentProviderHandler.createEscapedContentUri(
                                                    devicePath)),
                            eq(toPush));
            assertTrue(mProvider.pushFile(toPush, devicePath));
        } finally {
            FileUtil.deleteFile(toPush);
        }
    }

    /** Test {@link ContentProviderHandler#pushFile(File, String)} when the file doesn't exists */
    @Test
    public void testPushFile_notExists() throws Exception {
        File toPush = new File("content-provider-test.txt");
        try {
            String devicePath = "path/somewhere/file.txt";
            assertFalse(mProvider.pushFile(toPush, devicePath));
        } finally {
            FileUtil.deleteFile(toPush);
        }
    }

    /**
     * Test {@link ContentProviderHandler#pushFile(File, String)} when the file exists but is a
     * directory
     */
    @Test
    public void testPushFile_directory() throws Exception {
        File toPush = FileUtil.createTempDir("content-provider-test");
        try {
            String devicePath = "path/somewhere/file.txt";
            assertFalse(mProvider.pushFile(toPush, devicePath));
        } finally {
            FileUtil.recursiveDelete(toPush);
        }
    }

    /** Test {@link ContentProviderHandler#pullFile(String, File)}. */
    @Test
    public void testPullFile_verifyShellCommand() throws Exception {
        File pullTo = FileUtil.createTempFile("content-provider-test", ".txt");
        String devicePath = "path/somewhere/file.txt";
        doReturn(99).when(mMockDevice).getCurrentUser();
        mockPullFileSuccess();

        try {
            mProvider.pullFile(devicePath, pullTo);

            // Capture the shell command used by pullFile.
            ArgumentCaptor<String> shellCommandCaptor = ArgumentCaptor.forClass(String.class);
            verify(mMockDevice)
                    .executeShellV2Command(shellCommandCaptor.capture(), any(OutputStream.class));

            // Verify the command.
            assertEquals(
                    shellCommandCaptor.getValue(),
                    "content read --user 99 --uri "
                            + ContentProviderHandler.createEscapedContentUri(devicePath));
        } finally {
            FileUtil.deleteFile(pullTo);
        }
    }

    /** Test {@link ContentProviderHandler#pullFile(String, File)}. */
    @Test
    public void testPullFile_createLocalFileIfNotExist() throws Exception {
        File pullTo = new File("content-provider-test.txt");
        String devicePath = "path/somewhere/file.txt";
        mockPullFileSuccess();

        try {
            assertFalse(pullTo.exists());
            mProvider.pullFile(devicePath, pullTo);
            assertTrue(pullTo.exists());
        } finally {
            FileUtil.deleteFile(pullTo);
        }
    }

    /** Test {@link ContentProviderHandler#pullFile(String, File)}. */
    @Test
    public void testPullFile_success() throws Exception {
        File pullTo = new File("content-provider-test.txt");
        String devicePath = "path/somewhere/file.txt";

        try {
            mockPullFileSuccess();
            assertTrue(mProvider.pullFile(devicePath, pullTo));
        } finally {
            FileUtil.deleteFile(pullTo);
        }
    }

    /** Test {@link ContentProviderHandler#pullDir(String, File)}. */
    @Test
    public void testPullDir_EmptyDirectory() throws Exception {
        File pullTo = FileUtil.createTempDir("content-provider-test");

        doReturn("No result found.\n").when(mMockDevice).executeShellCommand(anyString());

        try {
            assertTrue(mProvider.pullDir("path/somewhere", pullTo));
        } finally {
            FileUtil.recursiveDelete(pullTo);
        }
    }

    @Test
    public void testPullDir_failedDevice() throws Exception {
        File pullTo = FileUtil.createTempDir("content-provider-test");

        doReturn("Something crashed").when(mMockDevice).executeShellCommand(anyString());

        try {
            assertFalse(mProvider.pullDir("path/somewhere", pullTo));
        } finally {
            FileUtil.recursiveDelete(pullTo);
        }
    }

    /**
     * Test {@link ContentProviderHandler#pullDir(String, File)} to pull a directory that contains
     * one text file.
     */
    @Test
    public void testPullDir_OneFile() throws Exception {
        File pullTo = FileUtil.createTempDir("content-provider-test");

        String devicePath = "path/somewhere";
        String fileName = "content-provider-file.txt";

        doReturn(createMockFileRow(fileName, devicePath + "/" + fileName, "text/plain"))
                .when(mMockDevice)
                .executeShellCommand(anyString());
        mockPullFileSuccess();

        try {
            // Assert that local directory is empty.
            assertEquals(pullTo.listFiles().length, 0);
            mProvider.pullDir(devicePath, pullTo);

            // Assert that a file has been pulled inside the directory.
            assertEquals(pullTo.listFiles().length, 1);
            assertEquals(pullTo.listFiles()[0].getName(), fileName);
        } finally {
            FileUtil.recursiveDelete(pullTo);
        }
    }

    /**
     * Test {@link ContentProviderHandler#pullDir(String, File)} to pull a directory that contains
     * another directory.
     */
    @Test
    public void testPullDir_RecursiveSubDir() throws Exception {
        File pullTo = FileUtil.createTempDir("content-provider-test");

        String devicePath = "path/somewhere";
        String subDirName = "test-subdir";
        String subDirPath = devicePath + "/" + subDirName;
        String fileName = "test-file.txt";

        doReturn(99).when(mMockDevice).getCurrentUser();
        // Mock the result for the directory.
        doReturn(createMockDirRow(subDirName, subDirPath))
                .when(mMockDevice)
                .executeShellCommand(
                        "content query --user 99 --uri "
                                + ContentProviderHandler.createEscapedContentUri(devicePath));

        // Mock the result for the subdir.
        doReturn(createMockFileRow(fileName, subDirPath + "/" + fileName, "text/plain"))
                .when(mMockDevice)
                .executeShellCommand(
                        "content query --user 99 --uri "
                                + ContentProviderHandler.createEscapedContentUri(
                                devicePath + "/" + subDirName));

        mockPullFileSuccess();

        try {
            // Assert that local directory is empty.
            assertEquals(pullTo.listFiles().length, 0);
            mProvider.pullDir(devicePath, pullTo);

            // Assert that a subdirectory has been created.
            assertEquals(pullTo.listFiles().length, 1);
            assertEquals(pullTo.listFiles()[0].getName(), subDirName);
            assertTrue(pullTo.listFiles()[0].isDirectory());

            // Assert that a file has been pulled inside the subdirectory.
            assertEquals(pullTo.listFiles()[0].listFiles().length, 1);
            assertEquals(pullTo.listFiles()[0].listFiles()[0].getName(), fileName);
        } finally {
            FileUtil.recursiveDelete(pullTo);
        }
    }

    @Test
    public void testCreateUri() {
        String espacedUrl =
                ContentProviderHandler.createEscapedContentUri("filepath/file name spaced (data)");
        // We expect the full url to be quoted to avoid space issues and the URL to be encoded.
        assertEquals(
                "\"content://android.tradefed.contentprovider/filepath%252Ffile+name+spaced+"
                        + "%2528data%2529\"",
                espacedUrl);
    }

    /** Test {@link ContentProviderHandler#doesFileExist(String)}. */
    @Test
    public void testDoesFileExist() throws Exception {
        String devicePath = "path/somewhere/file.txt";

        when(mMockDevice.getCurrentUser()).thenReturn(99);
        when(mMockDevice.executeShellCommand(
                        "content query --user 99 --uri "
                                + ContentProviderHandler.createEscapedContentUri(devicePath)))
                .thenReturn("");

        assertTrue(mProvider.doesFileExist(devicePath));
    }

    /**
     * Test {@link ContentProviderHandler#doesFileExist(String)} returns false when 'adb shell
     * content query' returns no results.
     */
    @Test
    public void testDoesFileExist_NotExists() throws Exception {
        String devicePath = "path/somewhere/";

        when(mMockDevice.getCurrentUser()).thenReturn(99);
        when(mMockDevice.executeShellCommand(
                        "content query --user 99 --uri "
                                + ContentProviderHandler.createEscapedContentUri(devicePath)))
                .thenReturn("No result found.\n");
        assertFalse(mProvider.doesFileExist(devicePath));
    }

    @Test
    public void testParseQueryResultRow() {
        String row =
                "Row: 1 name=name spaced with , ,comma, "
                        + "absolute_path=/storage/emulated/0/Alarms/name spaced with , ,comma, "
                        + "is_directory=true, mime_type=NULL, metadata=NULL";

        HashMap<String, String> columnValues = mProvider.parseQueryResultRow(row);

        assertEquals(
                columnValues.get(ContentProviderHandler.COLUMN_NAME), "name spaced with , ,comma");
        assertEquals(
                columnValues.get(ContentProviderHandler.COLUMN_ABSOLUTE_PATH),
                "/storage/emulated/0/Alarms/name spaced with , ,comma");
        assertEquals(columnValues.get(ContentProviderHandler.COLUMN_DIRECTORY), "true");
        assertEquals(columnValues.get(ContentProviderHandler.COLUMN_MIME_TYPE), "NULL");
        assertEquals(columnValues.get(ContentProviderHandler.COLUMN_METADATA), "NULL");
    }

    private CommandResult mockSuccess() {
        CommandResult result = new CommandResult(CommandStatus.SUCCESS);
        result.setStderr("");
        result.setStdout("");
        return result;
    }

    private void mockPullFileSuccess() throws Exception {
        doReturn(mockSuccess())
                .when(mMockDevice)
                .executeShellV2Command(anyString(), any(OutputStream.class));
    }

    private String createMockDirRow(String name, String path) {
        return String.format(
                "Row: 1 name=%s, absolute_path=%s, is_directory=%b, mime_type=NULL, metadata=NULL",
                name, path, true);
    }

    private String createMockFileRow(String name, String path, String mimeType) {
        return String.format(
                "Row: 1 name=%s, absolute_path=%s, is_directory=%b, mime_type=%s, metadata=NULL",
                name, path, false, mimeType);
    }
}
