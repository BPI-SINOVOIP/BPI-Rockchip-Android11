/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.compatibility.common.tradefed.targetprep;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.TargetSetupError;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link MediaPreparer}. */
@RunWith(JUnit4.class)
public class MediaPreparerTest {

    private MediaPreparer mMediaPreparer;
    private IBuildInfo mMockBuildInfo;
    private ITestDevice mMockDevice;
    private OptionSetter mOptionSetter;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMediaPreparer = new MediaPreparer();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockBuildInfo = new BuildInfo("0", "");
        mOptionSetter = new OptionSetter(mMediaPreparer);
        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        context.addAllocatedDevice("device", mMockDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testSetMountPoint() throws Exception {
        EasyMock.expect(mMockDevice.getMountPoint(IDevice.MNT_EXTERNAL_STORAGE)).andReturn(
                "/sdcard").once();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.setMountPoint(mMockDevice);
        EasyMock.verify(mMockDevice);
        assertEquals(mMediaPreparer.mBaseDeviceShortDir, "/sdcard/test/bbb_short/");
        assertEquals(mMediaPreparer.mBaseDeviceFullDir, "/sdcard/test/bbb_full/");
    }

    @Test
    public void testDefaultModuleDirMountPoint() throws Exception {
        EasyMock.expect(mMockDevice.getMountPoint(IDevice.MNT_EXTERNAL_STORAGE)).andReturn(
                "/sdcard").once();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.setMountPoint(mMockDevice);
        EasyMock.verify(mMockDevice);
        assertEquals(mMediaPreparer.mBaseDeviceModuleDir, "/sdcard/test/android-cts-media/");
        assertEquals(mMediaPreparer.getMediaDir().getName(), "android-cts-media");
    }

    @Test
    public void testSetModuleDirMountPoint() throws Exception {
        mOptionSetter.setOptionValue("media-folder-name", "unittest");
        EasyMock.expect(mMockDevice.getMountPoint(IDevice.MNT_EXTERNAL_STORAGE)).andReturn(
                "/sdcard").once();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.setMountPoint(mMockDevice);
        EasyMock.verify(mMockDevice);
        assertEquals(mMediaPreparer.mBaseDeviceModuleDir, "/sdcard/test/unittest/");
        assertEquals(mMediaPreparer.getMediaDir().getName(), "unittest");
    }

    @Test
    public void testCopyMediaFiles() throws Exception {
        mMediaPreparer.mMaxRes = MediaPreparer.DEFAULT_MAX_RESOLUTION;
        mMediaPreparer.mBaseDeviceShortDir = "/sdcard/test/bbb_short/";
        mMediaPreparer.mBaseDeviceFullDir = "/sdcard/test/bbb_full/";
        mMediaPreparer.mBaseDeviceImagesDir = "/sdcard/test/images";
        mMediaPreparer.mBaseDeviceModuleDir = "/sdcard/test/android-cts-media/";
        for (MediaPreparer.Resolution resolution : MediaPreparer.RESOLUTIONS) {
            if (resolution.getWidth() > MediaPreparer.DEFAULT_MAX_RESOLUTION.getWidth()) {
                // Stop when we reach the default max resolution
                continue;
            }
            String shortFile = String.format("%s%s", mMediaPreparer.mBaseDeviceShortDir,
                    resolution.toString());
            String fullFile = String.format("%s%s", mMediaPreparer.mBaseDeviceFullDir,
                    resolution.toString());
            EasyMock.expect(mMockDevice.doesFileExist(shortFile)).andReturn(true).once();
            EasyMock.expect(mMockDevice.doesFileExist(fullFile)).andReturn(true).once();
        }
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceImagesDir))
                .andReturn(true).anyTimes();
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceModuleDir))
                .andReturn(false).anyTimes();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.copyMediaFiles(mMockDevice);
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testMediaFilesExistOnDeviceTrue() throws Exception {
        mMediaPreparer.mMaxRes = MediaPreparer.DEFAULT_MAX_RESOLUTION;
        mMediaPreparer.mBaseDeviceShortDir = "/sdcard/test/bbb_short/";
        mMediaPreparer.mBaseDeviceFullDir = "/sdcard/test/bbb_full/";
        mMediaPreparer.mBaseDeviceImagesDir = "/sdcard/test/images";
        for (MediaPreparer.Resolution resolution : MediaPreparer.RESOLUTIONS) {
            String shortFile = String.format("%s%s", mMediaPreparer.mBaseDeviceShortDir,
                    resolution.toString());
            String fullFile = String.format("%s%s", mMediaPreparer.mBaseDeviceFullDir,
                    resolution.toString());
            EasyMock.expect(mMockDevice.doesFileExist(shortFile)).andReturn(true).anyTimes();
            EasyMock.expect(mMockDevice.doesFileExist(fullFile)).andReturn(true).anyTimes();
        }
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceImagesDir))
                .andReturn(true).anyTimes();
        EasyMock.replay(mMockDevice);
        assertTrue(mMediaPreparer.mediaFilesExistOnDevice(mMockDevice));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testMediaFilesExistOnDeviceTrueWithPushAll() throws Exception {
        mOptionSetter.setOptionValue("push-all", "true");
        mMediaPreparer.mBaseDeviceModuleDir = "/sdcard/test/android-cts-media/";
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceModuleDir))
                .andReturn(true).anyTimes();
        EasyMock.replay(mMockDevice);
        assertTrue(mMediaPreparer.mediaFilesExistOnDevice(mMockDevice));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testMediaFilesExistOnDeviceFalse() throws Exception {
        mMediaPreparer.mMaxRes = MediaPreparer.DEFAULT_MAX_RESOLUTION;
        mMediaPreparer.mBaseDeviceShortDir = "/sdcard/test/bbb_short/";
        String firstFileChecked = "/sdcard/test/bbb_short/176x144";
        EasyMock.expect(mMockDevice.doesFileExist(firstFileChecked)).andReturn(false).once();
        EasyMock.replay(mMockDevice);
        assertFalse(mMediaPreparer.mediaFilesExistOnDevice(mMockDevice));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testSkipMediaDownload() throws Exception {
        mOptionSetter.setOptionValue("skip-media-download", "true");
        EasyMock.replay(mMockDevice);
        mMediaPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testPushAll() throws Exception {
        mOptionSetter.setOptionValue("push-all", "true");
        mOptionSetter.setOptionValue("media-folder-name", "unittest");
        mMediaPreparer.mBaseDeviceModuleDir = "/sdcard/test/unittest/";
        mMediaPreparer.mBaseDeviceShortDir = "/sdcard/test/bbb_short/";
        mMediaPreparer.mBaseDeviceFullDir = "/sdcard/test/bbb_full/";
        mMediaPreparer.mBaseDeviceImagesDir = "/sdcard/test/images";
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceModuleDir))
                .andReturn(true).anyTimes();
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceImagesDir))
                .andReturn(false).anyTimes();
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceShortDir))
                .andReturn(false).anyTimes();
        EasyMock.expect(mMockDevice.doesFileExist(mMediaPreparer.mBaseDeviceFullDir))
                .andReturn(false).anyTimes();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.copyMediaFiles(mMockDevice);
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testWithBothPushAllAndImagesOnly() throws Exception {
        mOptionSetter.setOptionValue("push-all", "true");
        mOptionSetter.setOptionValue("images-only", "true");

        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andReturn(null);

        EasyMock.replay(mMockDevice);
        try {
            mMediaPreparer.setUp(mTestInfo);
            fail("TargetSetupError expected");
        } catch (TargetSetupError e) {
            // Expected
        }
        EasyMock.verify(mMockDevice);
    }

    /** Test that if we decide to run and files are on the device, we don't download again. */
    @Test
    public void testMediaDownloadOnly_existsOnDevice() throws Exception {
        mOptionSetter.setOptionValue("local-media-path", "/fake/media/dir");
        mMediaPreparer.mBaseDeviceShortDir = "/sdcard/test/bbb_short/";
        mMediaPreparer.mBaseDeviceFullDir = "/sdcard/test/bbb_full/";

        EasyMock.expect(mMockDevice.getMountPoint(IDevice.MNT_EXTERNAL_STORAGE))
                .andReturn("/sdcard")
                .once();
        for (MediaPreparer.Resolution resolution : MediaPreparer.RESOLUTIONS) {
            if (resolution.getWidth() > MediaPreparer.DEFAULT_MAX_RESOLUTION.getWidth()) {
                // Stop when we reach the default max resolution
                continue;
            }
            String shortFile =
                    String.format(
                            "%s%s", mMediaPreparer.mBaseDeviceShortDir, resolution.toString());
            String fullFile =
                    String.format("%s%s", mMediaPreparer.mBaseDeviceFullDir, resolution.toString());
            EasyMock.expect(mMockDevice.doesFileExist(shortFile)).andReturn(true).once();
            EasyMock.expect(mMockDevice.doesFileExist(fullFile)).andReturn(true).once();
        }
        EasyMock.expect(mMockDevice.doesFileExist("/sdcard/test/images/")).andReturn(true).once();
        EasyMock.replay(mMockDevice);
        mMediaPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice);
    }
}
