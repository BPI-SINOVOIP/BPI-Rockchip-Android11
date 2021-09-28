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

package com.android.tradefed.host;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit test for {@link LocalHostResourceManager}; */
@RunWith(JUnit4.class)
public class LocalHostResourceManagerTest {

    private File mLocalRoot;
    private LocalHostResourceManager mHostResourceManager;

    @Before
    public void setUp() throws Exception {
        mLocalRoot = FileUtil.createTempDir(LocalHostResourceManagerTest.class.getSimpleName());
        mHostResourceManager = new LocalHostResourceManager();
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mLocalRoot);
    }

    @Test
    public void testSetupHostResource() throws Exception {
        File localFile = FileUtil.createTempFile("filename", "apk", mLocalRoot);
        OptionSetter optionSetter = new OptionSetter(mHostResourceManager);
        optionSetter.setOptionValue(
                "local-hrm:host-resource", "filename-key", localFile.getAbsolutePath());
        mHostResourceManager.setup();

        File fileResource = mHostResourceManager.getFile("filename-key");
        Assert.assertEquals(localFile.getAbsolutePath(), fileResource.getAbsolutePath());
    }

    @Test
    public void testSetupHostResource_fileDoesNotExist() throws Exception {
        OptionSetter optionSetter = new OptionSetter(mHostResourceManager);
        optionSetter.setOptionValue(
                "local-hrm:host-resource", "filename-key", "/non/exist/path/filename.apk");
        try {
            mHostResourceManager.setup();
            Assert.assertFalse("Should throw Exception.", true);
        } catch (ConfigurationException e) {
            // Expected.
        }
    }
}
