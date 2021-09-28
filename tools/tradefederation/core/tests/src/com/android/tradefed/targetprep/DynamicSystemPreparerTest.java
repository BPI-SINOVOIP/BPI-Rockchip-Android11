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
package com.android.tradefed.targetprep;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doAnswer;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

/** Unit tests for {@link DynamicSystemPreparer}. */
@RunWith(JUnit4.class)
public class DynamicSystemPreparerTest {
    // Input build info.
    private static final String SYSTEM_IMAGE_NAME = "system.img";

    private IBuildInfo mBuildInfo;
    private ITestDevice mMockDevice;
    private File mSystemImageZip;
    // The object under test.
    private DynamicSystemPreparer mPreparer;

    @Before
    public void setUp() throws IOException {
        mMockDevice = Mockito.mock(ITestDevice.class);
        mSystemImageZip = createImageZip(SYSTEM_IMAGE_NAME);
        mBuildInfo = new BuildInfo();
        mBuildInfo.setFile("system-img.zip", mSystemImageZip, "0");

        mPreparer = new DynamicSystemPreparer();
    }

    @After
    public void tearDown() {
        if (mBuildInfo != null) {
            mBuildInfo.cleanUp();
            mBuildInfo = null;
        }
        FileUtil.deleteFile(mSystemImageZip);
    }

    private File createImageDir(String... fileNames) throws IOException {
        File tempDir = FileUtil.createTempDir("createImageDir");
        for (String fileName : fileNames) {
            new File(tempDir, fileName).createNewFile();
        }
        return tempDir;
    }

    private File createImageZip(String... fileNames) throws IOException {
        File tempDir = null;
        try {
            tempDir = createImageDir(fileNames);

            ArrayList<File> tempFiles = new ArrayList<File>(fileNames.length);
            for (String fileName : fileNames) {
                tempFiles.add(new File(tempDir, fileName));
            }

            return ZipUtil.createZip(tempFiles);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    @Test
    public void testSetUp() throws TargetSetupError, BuildError, DeviceNotAvailableException {
        Mockito.when(mMockDevice.pushFile(Mockito.any(), Mockito.eq("/sdcard/system.raw.gz")))
                .thenReturn(Boolean.TRUE);
        doAnswer(
                        new Answer<Object>() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                byte[] outputBytes = "running".getBytes();
                                ((CollectingOutputReceiver) invocation.getArguments()[1])
                                        .addOutput(outputBytes, 0, outputBytes.length);
                                return null;
                            }
                        })
                .when(mMockDevice)
                .executeShellCommand(
                        matches("gsi_tool status"), any(CollectingOutputReceiver.class));
        CommandResult res = new CommandResult();
        res.setStdout("");
        res.setStatus(CommandStatus.SUCCESS);
        Mockito.when(mMockDevice.executeShellV2Command("gsi_tool enable")).thenReturn(res);
        mPreparer.setUp(mMockDevice, mBuildInfo);
    }
}
