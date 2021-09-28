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
package com.android.tradefed.host.gcs;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IFileDownloader;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;

/** Unit test for {@link GCSHostResourceManager} */
@RunWith(JUnit4.class)
public class GCSHostResourceManagerTest {
    private static final String HOST_RESOURCE1 = "gs://b/this/is/a/file1.txt";
    private static final String HOST_RESOURCE2 = "gs://b/this/is/a/file2.txt";

    private File mRoot;
    private GCSHostResourceManager mHostResourceManager;

    @Before
    public void setUp() throws Exception {
        mRoot = FileUtil.createTempDir(GCSHostResourceManagerTest.class.getSimpleName());

        mHostResourceManager =
                new GCSHostResourceManager() {

                    @Override
                    IFileDownloader getGCSFileDownloader() {
                        return new IFileDownloader() {

                            @Override
                            public void downloadFile(String relativeRemotePath, File destFile)
                                    throws BuildRetrievalError {
                                try {
                                    FileUtil.writeToFile(relativeRemotePath, destFile);
                                } catch (IOException e) {
                                    throw new BuildRetrievalError(e.getMessage(), e);
                                }
                            }

                            @Override
                            public File downloadFile(String remoteFilePath)
                                    throws BuildRetrievalError {
                                try {
                                    File destFile =
                                            FileUtil.createTempFileForRemote(remoteFilePath, mRoot);
                                    downloadFile(remoteFilePath, destFile);
                                    return destFile;
                                } catch (IOException e) {
                                    throw new BuildRetrievalError(e.getMessage(), e);
                                }
                            }
                        };
                    }
                };

        OptionSetter setter = new OptionSetter(mHostResourceManager);
        setter.setOptionValue("host-resource", "key1", HOST_RESOURCE1);
        setter.setOptionValue("host-resource", "key2", HOST_RESOURCE2);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mRoot);
    }

    @Test
    public void testSetUpHostResources() throws Exception {
        mHostResourceManager.setup();
        File file1 = mHostResourceManager.getFile("key1");
        File file2 = mHostResourceManager.getFile("key2");
        Assert.assertEquals(HOST_RESOURCE1, FileUtil.readStringFromFile(file1));
        Assert.assertEquals(HOST_RESOURCE2, FileUtil.readStringFromFile(file2));
        mHostResourceManager.cleanup();
        Assert.assertFalse(file1.exists());
        Assert.assertFalse(file2.exists());
    }
}
