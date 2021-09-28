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
package com.android.tradefed.contentprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import com.android.tradefed.device.contentprovider.ContentProviderHandler;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Host tests for the Tradefed Content Provider. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ContentProviderTest extends BaseHostJUnit4Test {
    private static final String EXTERNAL_STORAGE_PATH = "/storage/emulated/%d/";

    private ContentProviderHandler mHandler;
    private String mCurrentUserStoragePath;
    private List<String> mToBeDeleted = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        mHandler = new ContentProviderHandler(getDevice());
        mCurrentUserStoragePath =
                String.format(EXTERNAL_STORAGE_PATH, getDevice().getCurrentUser());
        assertTrue(mHandler.setUp());
    }

    @After
    public void tearDown() throws Exception {
        for (String delete : mToBeDeleted) {
            getDevice().deleteFile(delete);
        }
        mToBeDeleted.clear();
        if (mHandler != null) {
            mHandler.tearDown();
        }
    }

    /** Test pushing a file with special characters in the name. */
    @Test
    public void testPushFile_encode() throws Exception {
        // Name with space and parenthesis
        File tmpFile = FileUtil.createTempFile("tmpFileToPush (test)", ".txt");
        try {
            boolean res = mHandler.pushFile(tmpFile, "/sdcard/" + tmpFile.getName());
            assertTrue(res);
            assertTrue(getDevice().doesFileExist(mCurrentUserStoragePath + tmpFile.getName()));
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /**
     * '+' character is special in URLs as it can be decoded incorrectly as a space. Ensure it works
     * and our encoding/decoding handles it well.
     */
    @Test
    public void testPushFile_encode_plus() throws Exception {
        // Name with '+'
        File tmpFile = FileUtil.createTempFile("tmpFileToPush+(test)", ".txt");
        try {
            boolean res = mHandler.pushFile(tmpFile, "/sdcard/" + tmpFile.getName());
            assertTrue(res);
            assertTrue(getDevice().doesFileExist(mCurrentUserStoragePath + tmpFile.getName()));
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    @Test
    public void testPushFile() throws Exception {
        File tmpFile = FileUtil.createTempFile("tmpFileToPush", ".txt");
        try {
            String devicePath = "/sdcard/" + tmpFile.getName();
            mToBeDeleted.add(devicePath);
            boolean res = mHandler.pushFile(tmpFile, devicePath);
            assertTrue(res);
            assertTrue(getDevice().doesFileExist(mCurrentUserStoragePath + tmpFile.getName()));
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    /** Test that we can delete a file via Content Provider. */
    @Test
    public void testDeleteFile() throws Exception {
        File tmpFile = FileUtil.createTempFile("tmpFileToPush", ".txt");
        try {
            String devicePath = "/sdcard/" + tmpFile.getName();
            // Push the file first
            boolean res = mHandler.pushFile(tmpFile, devicePath);
            assertTrue(res);
            assertTrue(getDevice().doesFileExist(mCurrentUserStoragePath + tmpFile.getName()));
            // Attempt to delete it.
            assertTrue(mHandler.deleteFile(devicePath));
        } finally {
            FileUtil.deleteFile(tmpFile);
        }
    }

    @Test
    public void testPullFile() throws Exception {
        String fileContent = "some test content";

        // First, push the file onto a device.
        File tmpFile = FileUtil.createTempFile("tmpFileToPush", ".txt");
        FileUtil.writeToFile(fileContent, tmpFile);
        mHandler.pushFile(tmpFile, "/sdcard/" + tmpFile.getName());
        mToBeDeleted.add("/sdcard/" + tmpFile.getName());

        File tmpPullFile = new File("fileToPullTo.txt");
        // Local file does not exist before we pull the content from the device.
        assertFalse(tmpPullFile.exists());

        try {
            boolean res = mHandler.pullFile("/sdcard/" + tmpFile.getName(), tmpPullFile);
            assertTrue(res);
            assertTrue(tmpPullFile.exists()); // Verify existence.
            assertEquals(FileUtil.readStringFromFile(tmpPullFile), fileContent); // Verify content.
        } finally {
            FileUtil.deleteFile(tmpFile);
            FileUtil.deleteFile(tmpPullFile);
        }
    }

    @Test
    public void testPullDir() throws Exception {
        String fileContent = "some test content";
        String dirName = "test_dir_path";
        String subDirName = "test_subdir_path";
        String subDirPath = dirName + "/" + subDirName;

        // First, push the file onto a device to create nested structure= dirName->subDirName->file
        File tmpFile = FileUtil.createTempFile("tmpFileToPush", ".txt");
        FileUtil.writeToFile(fileContent, tmpFile);
        mHandler.pushFile(tmpFile, "/sdcard/" + subDirPath + "/" + tmpFile.getName());
        mToBeDeleted.add("/sdcard/" + dirName);

        File tmpPullDir = FileUtil.createTempDir("dirToPullTo");

        try {
            boolean res = mHandler.pullDir("/sdcard/" + dirName, tmpPullDir);
            assertTrue(res);
            assertEquals(tmpPullDir.listFiles()[0].getName(), subDirName); // Verify subDir
            assertEquals(tmpPullDir.listFiles()[0].listFiles().length, 1); // Verify subDir content.
            assertEquals(
                    FileUtil.readStringFromFile(tmpPullDir.listFiles()[0].listFiles()[0]),
                    fileContent); // Verify content.
        } finally {
            FileUtil.recursiveDelete(tmpPullDir);
            FileUtil.deleteFile(tmpFile);
        }
    }
}
