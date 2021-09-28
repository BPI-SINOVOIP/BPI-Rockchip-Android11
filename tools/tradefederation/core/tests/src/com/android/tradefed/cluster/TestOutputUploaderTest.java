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
import static org.junit.Assert.assertTrue;

import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.IOException;
import java.net.URL;

/** Unit tests for {@link TestOutputUploader}. */
@RunWith(JUnit4.class)
public class TestOutputUploaderTest {

    private IRunUtil mMockRunUtil;
    private TestOutputUploader mTestOutputUploader;
    private File mOutputFile;

    @Before
    public void setUp() throws IOException {
        mMockRunUtil = Mockito.mock(IRunUtil.class);
        mTestOutputUploader =
                new TestOutputUploader() {
                    @Override
                    IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }
                };
        mOutputFile = File.createTempFile(this.getClass().getName(), ".log");
    }

    @After
    public void tearDown() {
        mOutputFile.delete();
    }

    @Test
    public void testUploadFile_fileProtocol_rootPath() throws IOException {
        File destFolder = null;
        try {
            destFolder = FileUtil.createTempDir(this.getClass().getName());
            final String uploadUrl = "file://" + destFolder.getAbsolutePath();
            mTestOutputUploader.setUploadUrl(uploadUrl);

            final String outputFileUrl = mTestOutputUploader.uploadFile(mOutputFile, null);

            assertEquals(uploadUrl + "/" + mOutputFile.getName(), outputFileUrl);
            final File uploadedFile = new File(new URL(outputFileUrl).getPath());
            assertTrue(uploadedFile.exists());
            assertTrue(FileUtil.compareFileContents(mOutputFile, uploadedFile));
        } finally {
            if (destFolder != null) {
                FileUtil.recursiveDelete(destFolder);
            }
        }
    }

    @Test
    public void testUploadFile_fileProtocol_withDestPath() throws IOException {
        File destRootFolder = null;
        try {
            destRootFolder = FileUtil.createTempDir(this.getClass().getName());
            final String uploadUrl = "file://" + destRootFolder.getAbsolutePath();
            mTestOutputUploader.setUploadUrl(uploadUrl);

            final String outputFileUrl = mTestOutputUploader.uploadFile(mOutputFile, "sub_dir");

            assertEquals(uploadUrl + "/" + "sub_dir" + "/" + mOutputFile.getName(), outputFileUrl);
            final File uploadedFile = new File(new URL(outputFileUrl).getPath());
            assertTrue(uploadedFile.exists());
            assertTrue(FileUtil.compareFileContents(mOutputFile, uploadedFile));
        } finally {
            if (destRootFolder != null) {
                FileUtil.recursiveDelete(destRootFolder);
            }
        }
    }
}
