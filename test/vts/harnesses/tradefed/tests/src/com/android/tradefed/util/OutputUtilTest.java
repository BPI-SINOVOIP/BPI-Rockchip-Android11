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

package com.android.tradefed.util;

import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.result.LogDataType;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.MockitoAnnotations;

import java.io.File;

/**
 * Unit tests for {@link CmdUtil}.
 */
@RunWith(JUnit4.class)
public class OutputUtilTest {
    OutputUtil mOutputUtil = null;

    private ITestLogger mLogger;
    private File mTestDir = null;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mLogger = EasyMock.createMock(ITestLogger.class);
        mOutputUtil = new OutputUtil(mLogger);
        mTestDir = FileUtil.createTempDir("output-util-test");
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mTestDir);
    }

    /**
     * Test using {@link OutputUtil#ZipVtsRunnerOutputDir(File)} on an empty dir. it shouldn't log
     * anything.
     */
    @Test
    public void testZipVtsRunnerOutputDir_emptyDir() {
        EasyMock.replay(mLogger);
        mOutputUtil.ZipVtsRunnerOutputDir(mTestDir);
        EasyMock.verify(mLogger);
    }

    @Test
    public void testZipVtsRunnerOutputDir() {
        File latest = new File(mTestDir, "latest");
        latest.mkdir();

        mLogger.testLog(EasyMock.anyObject(), EasyMock.eq(LogDataType.ZIP), EasyMock.anyObject());
        EasyMock.replay(mLogger);
        mOutputUtil.ZipVtsRunnerOutputDir(mTestDir);
        EasyMock.verify(mLogger);
    }
}
