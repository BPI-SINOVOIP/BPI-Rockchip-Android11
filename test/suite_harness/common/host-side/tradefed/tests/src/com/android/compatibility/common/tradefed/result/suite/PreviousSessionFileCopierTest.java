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
package com.android.compatibility.common.tradefed.result.suite;

import static org.junit.Assert.assertEquals;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FileNotFoundException;

/** Unit tests for {@link PreviousSessionFileCopier}. */
@RunWith(JUnit4.class)
public class PreviousSessionFileCopierTest {

    private PreviousSessionFileCopier mCopier;
    private File mPreviousDir;
    private File mCurrentDir;
    private IInvocationContext mContext;

    @Before
    public void setUp() throws Exception {
        mCurrentDir = FileUtil.createTempDir("current-copier-tests");
        mCopier =
                new PreviousSessionFileCopier() {
                    @Override
                    protected CompatibilityBuildHelper createCompatibilityHelper(IBuildInfo info) {
                        return new CompatibilityBuildHelper(info) {
                            @Override
                            public File getResultDir() throws FileNotFoundException {
                                return mCurrentDir;
                            }
                        };
                    }
                };
        mPreviousDir = FileUtil.createTempDir("previous-copier-tests");
        mContext = new InvocationContext();
        mContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
        mCopier.setPreviousSessionDir(mPreviousDir);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mPreviousDir);
    }

    @Test
    public void testCopy() throws Exception {
        new File(mPreviousDir, "newFile").createNewFile();
        assertEquals(0, mCurrentDir.listFiles().length);
        mCopier.invocationStarted(mContext);
        mCopier.invocationEnded(500L);
        assertEquals(1, mCurrentDir.listFiles().length);
    }

    @Test
    public void testCopy_fileExists() throws Exception {
        File original = new File(mCurrentDir, "newFile");
        original.createNewFile();
        FileUtil.writeToFile("CURRENT", original);

        File previous = new File(mPreviousDir, "newFile");
        previous.createNewFile();
        FileUtil.writeToFile("PREVIOUS", previous);

        assertEquals(1, mCurrentDir.listFiles().length);
        mCopier.invocationStarted(mContext);
        mCopier.invocationEnded(500L);
        assertEquals(1, mCurrentDir.listFiles().length);
        // File are not overriden
        assertEquals("CURRENT", FileUtil.readStringFromFile(original));
    }

    @Test
    public void testCopy_fileExcluded() throws Exception {
        new File(mPreviousDir, "proto").mkdir();
        mCopier.invocationStarted(mContext);
        mCopier.invocationEnded(500L);
        // Nothing was copied
        assertEquals(0, mCurrentDir.listFiles().length);
    }
}
