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
package com.android.tradefed.targetprep.multi;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.ZipUtil;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.Deflater;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

/** Unit tests for {@link MixImageZipPreparer}. */
@RunWith(JUnit4.class)
public class MixImageZipPreparerTest {
    // Input build info.
    private static final String VENDOR_IMAGE_NAME = "vendor.img";
    private static final String SYSTEM_IMAGE_NAME = "system.img";
    private static final String PRODUCT_IMAGE_NAME = "product.img";
    private static final String VBMETA_IMAGE_NAME = "vbmeta.img";
    private static final String SUPER_IMAGE_NAME = "super.img";
    private static final String MISC_INFO_FILE_NAME = "misc_info.txt";
    private static final String DEVICE_BUILD_FLAVOR = "device_flavor";
    private static final String SYSTEM_BUILD_FLAVOR = "system_flavor";
    private static final String DEVICE_BUILD_ID = "device_build_id";
    private static final String SYSTEM_BUILD_ID = "123456";
    private static final String DEVICE_LABEL = "device";
    private static final String SYSTEM_LABEL = "system";
    private static final String RESOURCE_LABEL = "resource";

    // The strings written to temporary image files.
    private static final String DEVICE_CONTENT = "device content";
    private static final String SYSTEM_CONTENT = "system content";
    private static final String DUMMY_CONTENT = "\0";
    private static final String RESOURCE_CONTENT = "resource content";

    private IInvocationContext mMockContext;
    private IRunUtil mMockRunRepackSuperImage;
    private IDeviceBuildInfo mDeviceBuild;
    private IDeviceBuildInfo mSystemBuild;
    private IBuildInfo mResourceBuild;
    private File mDeviceImageZip;
    private File mSystemImageZip;
    private File mMiscInfoFile;
    private File mOtaToolsZip;
    private File mRepackSuperImageFile;
    private File mResourceDir;

    // The object under test.
    private MixImageZipPreparer mPreparer;

    private class ByteArrayInputStreamFactory implements MixImageZipPreparer.InputStreamFactory {
        private final byte[] mData;
        private List<InputStream> createdInputStreams;

        private ByteArrayInputStreamFactory(String data) {
            mData = data.getBytes();
            createdInputStreams = new ArrayList<InputStream>();
        }

        @Override
        public InputStream createInputStream() throws IOException {
            InputStream stream = Mockito.spy(new ByteArrayInputStream(mData));
            createdInputStreams.add(stream);
            return stream;
        }

        @Override
        public long getSize() {
            return mData.length;
        }

        @Override
        public long getCrc32() throws IOException {
            // calculateCrc32 closes the stream.
            return StreamUtil.calculateCrc32(createInputStream());
        }
    }

    @Before
    public void setUp() {
        mMockContext = null;
        mMockRunRepackSuperImage = null;
        mDeviceBuild = null;
        mSystemBuild = null;
        mResourceBuild = null;
        mDeviceImageZip = null;
        mSystemImageZip = null;
        mMiscInfoFile = null;
        mOtaToolsZip = null;
        mRepackSuperImageFile = null;
        mResourceDir = null;
        mPreparer = null;
    }

    private void setUpPreparerAndSystem(String... imageNames) throws IOException {
        mMockContext = Mockito.mock(InvocationContext.class);

        ITestDevice mockSystem = Mockito.mock(ITestDevice.class);
        mSystemImageZip = createImageZip(SYSTEM_CONTENT, imageNames);
        mSystemBuild = createDeviceBuildInfo(SYSTEM_BUILD_FLAVOR, SYSTEM_BUILD_ID, mSystemImageZip);

        Mockito.when(mMockContext.getDevice(SYSTEM_LABEL)).thenReturn(mockSystem);
        Mockito.when(mMockContext.getBuildInfo(mockSystem)).thenReturn(mSystemBuild);

        mPreparer =
                new MixImageZipPreparer() {
                    @Override
                    IRunUtil createRunUtil() {
                        Assert.assertNotNull(
                                "Did not setup super image in device build.",
                                mMockRunRepackSuperImage);
                        return mMockRunRepackSuperImage;
                    }
                };

        for (String imageName : imageNames) {
            mPreparer.addSystemFileName(imageName);
        }
        mPreparer.addDummyFileName(PRODUCT_IMAGE_NAME);
        mPreparer.addDummyFileName("missing_dummy.img");
    }

    private void setUpPreparerAndSystem() throws IOException {
        setUpPreparerAndSystem(SYSTEM_IMAGE_NAME);
    }

    private void setUpDevice() throws IOException {
        ITestDevice mockDevice = Mockito.mock(ITestDevice.class);
        mDeviceImageZip =
                createImageZip(
                        DEVICE_CONTENT,
                        VENDOR_IMAGE_NAME,
                        SYSTEM_IMAGE_NAME,
                        PRODUCT_IMAGE_NAME,
                        VBMETA_IMAGE_NAME);
        mDeviceBuild = createDeviceBuildInfo(DEVICE_BUILD_FLAVOR, DEVICE_BUILD_ID, mDeviceImageZip);
        Mockito.when(mMockContext.getDevice(DEVICE_LABEL)).thenReturn(mockDevice);
        Mockito.when(mMockContext.getBuildInfo(mockDevice)).thenReturn(mDeviceBuild);
    }

    private void setUpDeviceWithSuper() throws IOException {
        ITestDevice mockDevice = Mockito.mock(ITestDevice.class);
        mDeviceImageZip =
                createImageZip(
                        DEVICE_CONTENT, VENDOR_IMAGE_NAME, VBMETA_IMAGE_NAME, SUPER_IMAGE_NAME);
        mDeviceBuild = createDeviceBuildInfo(DEVICE_BUILD_FLAVOR, DEVICE_BUILD_ID, mDeviceImageZip);
        mMiscInfoFile = FileUtil.createTempFile("misc_info", ".txt");
        FileUtil.writeToFile(DEVICE_CONTENT, mMiscInfoFile);
        mDeviceBuild.setFile(MISC_INFO_FILE_NAME, mMiscInfoFile, DEVICE_BUILD_ID);

        Mockito.when(mMockContext.getDevice(DEVICE_LABEL)).thenReturn(mockDevice);
        Mockito.when(mMockContext.getBuildInfo(mockDevice)).thenReturn(mDeviceBuild);

        // Add repacking tools to system build.
        mOtaToolsZip = FileUtil.createTempFile("otatools", ".zip");
        mSystemBuild.setFile("otatools.zip", mOtaToolsZip, SYSTEM_BUILD_ID);
        mRepackSuperImageFile = FileUtil.createTempFile("repack_super_image", "");
        mSystemBuild.setFile("repack_super_image", mRepackSuperImageFile, SYSTEM_BUILD_ID);

        Answer<CommandResult> defaultAnswer =
                new Answer<CommandResult>() {
                    @Override
                    public CommandResult answer(InvocationOnMock invocation) throws Throwable {
                        Assert.assertTrue(invocation.getArguments().length >= 10);
                        CommandResult failure = new CommandResult(CommandStatus.FAILED);
                        failure.setStdout("stdout");
                        failure.setStderr("stderr");
                        return failure;
                    }
                };

        mMockRunRepackSuperImage = Mockito.mock(IRunUtil.class, defaultAnswer);

        CommandResult success = new CommandResult(CommandStatus.SUCCESS);
        success.setStdout("stdout");
        success.setStderr("stderr");

        Mockito.when(
                        mMockRunRepackSuperImage.runTimedCmd(
                                Mockito.anyLong(),
                                Mockito.eq(mRepackSuperImageFile.getAbsolutePath()),
                                Mockito.eq("--temp-dir"),
                                Mockito.anyString(),
                                Mockito.eq("--ota-tools"),
                                Mockito.eq(mOtaToolsZip.getAbsolutePath()),
                                Mockito.eq("--misc-info"),
                                Mockito.eq(mMiscInfoFile.getAbsolutePath()),
                                Mockito.anyString(),
                                Mockito.startsWith("system=")))
                .thenReturn(success);
    }

    private void setUpResource() throws IOException {
        ITestDevice mockResource = Mockito.mock(ITestDevice.class);
        mResourceDir = createImageDir(RESOURCE_CONTENT, VBMETA_IMAGE_NAME);
        mResourceBuild = createBuildInfo(mResourceDir);

        Mockito.when(mMockContext.getDevice(RESOURCE_LABEL)).thenReturn(mockResource);
        Mockito.when(mMockContext.getBuildInfo(mockResource)).thenReturn(mResourceBuild);

        mPreparer.addResourceFileName(VBMETA_IMAGE_NAME);
    }

    @After
    public void tearDown() {
        if (mDeviceBuild != null) {
            mDeviceBuild.cleanUp();
        }
        if (mSystemBuild != null) {
            mSystemBuild.cleanUp();
        }
        if (mResourceBuild != null) {
            mResourceBuild.cleanUp();
        }
        FileUtil.deleteFile(mDeviceImageZip);
        FileUtil.deleteFile(mSystemImageZip);
        FileUtil.deleteFile(mMiscInfoFile);
        FileUtil.deleteFile(mOtaToolsZip);
        FileUtil.deleteFile(mRepackSuperImageFile);
        FileUtil.recursiveDelete(mResourceDir);
    }

    private IDeviceBuildInfo createDeviceBuildInfo(
            String buildFlavor, String buildId, File imageZip) {
        IDeviceBuildInfo buildInfo = new DeviceBuildInfo();
        buildInfo.setBuildFlavor(buildFlavor);
        buildInfo.setBuildId(buildId);
        buildInfo.setDeviceImageFile(imageZip, buildId);
        return buildInfo;
    }

    private IBuildInfo createBuildInfo(File rootDir) {
        BuildInfo buildInfo = new BuildInfo();
        for (File file : rootDir.listFiles()) {
            buildInfo.setFile(file.getName(), file, "0");
        }
        return buildInfo;
    }

    private File createImageDir(String content, String... fileNames) throws IOException {
        File tempDir = FileUtil.createTempDir("createImageDir");
        for (String fileName : fileNames) {
            FileUtil.writeToFile(content, new File(tempDir, fileName));
        }
        return tempDir;
    }

    private File createImageZip(String content, String... fileNames) throws IOException {
        // = new ArrayList<File>(fileNames.length);
        File tempDir = null;
        try {
            tempDir = createImageDir(content, fileNames);

            ArrayList<File> tempFiles = new ArrayList<File>(fileNames.length);
            for (String fileName : fileNames) {
                tempFiles.add(new File(tempDir, fileName));
            }

            return ZipUtil.createZip(tempFiles);
        } finally {
            FileUtil.recursiveDelete(tempDir);
        }
    }

    private void verifyImageReader(String content, Reader reader) throws IOException {
        char[] buffer = new char[content.length()];
        Assert.assertEquals(buffer.length, reader.read(buffer));
        Assert.assertEquals(content, new String(buffer));
        Assert.assertTrue("Image contains extra content.", reader.read() < 0);
    }

    private void verifyImage(String content, File dir, String fileName)
            throws FileNotFoundException, IOException {
        try (FileReader reader = new FileReader(new File(dir, fileName))) {
            verifyImageReader(content, reader);
        }
    }

    private void verifyImages(File mixedImageZip) throws FileNotFoundException, IOException {
        File mixedImageDir = ZipUtil.extractZipToTemp(mixedImageZip, "verifyImages");
        try {
            verifyImage(DEVICE_CONTENT, mixedImageDir, VENDOR_IMAGE_NAME);

            if (mMockRunRepackSuperImage != null) {
                // setUpDeviceWithSuper was called.
                Mockito.verify(mMockRunRepackSuperImage)
                        .runTimedCmd(Mockito.anyLong(), Mockito.any());
            } else {
                // setUpDevice was called.
                verifyImage(SYSTEM_CONTENT, mixedImageDir, SYSTEM_IMAGE_NAME);
                verifyImage(DUMMY_CONTENT, mixedImageDir, PRODUCT_IMAGE_NAME);
            }

            if (mResourceBuild != null) {
                // setUpResource was called.
                verifyImage(RESOURCE_CONTENT, mixedImageDir, VBMETA_IMAGE_NAME);
            }
        } finally {
            FileUtil.recursiveDelete(mixedImageDir);
        }
    }

    private void runPreparerTest()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, ZipException,
                    IOException {
        TestInformation testInformation =
                TestInformation.newBuilder().setInvocationContext(mMockContext).build();
        mPreparer.setUp(testInformation);

        ArgumentCaptor<IBuildInfo> argument = ArgumentCaptor.forClass(IBuildInfo.class);
        Mockito.verify(mMockContext)
                .addDeviceBuildInfo(Mockito.eq(DEVICE_LABEL), argument.capture());
        IDeviceBuildInfo addedBuildInfo = ((IDeviceBuildInfo) argument.getValue());
        try {
            Assert.assertFalse("Device build is not cleaned up.", mDeviceImageZip.exists());
            mDeviceImageZip = null;
            mDeviceBuild = null;

            Assert.assertEquals(SYSTEM_BUILD_FLAVOR, addedBuildInfo.getBuildFlavor());
            Assert.assertEquals(SYSTEM_BUILD_ID, addedBuildInfo.getDeviceBuildId());
            verifyImages(addedBuildInfo.getDeviceImageFile());
        } finally {
            addedBuildInfo.cleanUp();
        }
    }

    /**
     * Test that the mixed {@link IDeviceBuildInfo} contains the resource file and works with
     * non-default compression level.
     */
    @Test
    public void testSetUpWithResource()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, IOException {
        setUpPreparerAndSystem();
        setUpDevice();
        setUpResource();
        mPreparer.setCompressionLevel(0);
        runPreparerTest();
    }

    /**
     * Test that the mixed {@link IDeviceBuildInfo} contains the system build's image, build flavor,
     * and build id.
     */
    @Test
    public void testSetUpWithSystem()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, IOException {
        setUpPreparerAndSystem();
        setUpDevice();
        runPreparerTest();
    }

    /**
     * Test that the mixed {@link IDeviceBuildInfo} contains the mixed super image, system's build
     * flavor, and build id.
     */
    @Test
    public void testSetUpWithSuper()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, IOException {
        setUpPreparerAndSystem();
        setUpDeviceWithSuper();
        runPreparerTest();
    }

    /** Test that an image is not found in device build. */
    @Test(expected = TargetSetupError.class)
    public void testSetUpWithMissingImage()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, IOException {
        setUpPreparerAndSystem(SYSTEM_IMAGE_NAME, "missing.img");
        setUpDevice();
        runPreparerTest();
    }

    /** Test that an image is not found in unpacked super image. */
    @Test(expected = TargetSetupError.class)
    public void testSetUpWithSuperAndMissingImage()
            throws TargetSetupError, BuildError, DeviceNotAvailableException, IOException {
        setUpPreparerAndSystem(SYSTEM_IMAGE_NAME, "missing.img");
        setUpDeviceWithSuper();
        runPreparerTest();
    }

    private void runCreateZipTest(int compressionLevel) throws IOException {
        Map<String, ByteArrayInputStreamFactory> data =
                new HashMap<String, ByteArrayInputStreamFactory>();
        data.put("entry1", new ByteArrayInputStreamFactory("abcabcabcabcabcabc"));
        data.put("entry2", new ByteArrayInputStreamFactory("01230123012301230123"));

        File file = null;
        ZipFile zipFile = null;
        try {
            file = MixImageZipPreparer.createZip(data, compressionLevel);
            zipFile = new ZipFile(file);

            Assert.assertEquals(data.size(), zipFile.stream().count());
            for (Map.Entry<String, ByteArrayInputStreamFactory> entry : data.entrySet()) {
                ByteArrayInputStreamFactory expected = entry.getValue();
                ZipEntry actual = zipFile.getEntry(entry.getKey());
                Assert.assertEquals(expected.getSize(), actual.getSize());
                Assert.assertEquals(expected.getCrc32(), actual.getCrc());
                if (compressionLevel == Deflater.NO_COMPRESSION) {
                    Assert.assertEquals(expected.getSize(), actual.getCompressedSize());
                } else {
                    Assert.assertTrue(expected.getSize() > actual.getCompressedSize());
                }

                for (InputStream stream : expected.createdInputStreams) {
                    Mockito.verify(stream).close();
                }
            }
        } finally {
            ZipUtil.closeZip(zipFile);
            FileUtil.deleteFile(file);
        }
    }

    /** Verify createZip with default compression level. */
    @Test
    public void testCreateZip() throws IOException {
        runCreateZipTest(Deflater.DEFAULT_COMPRESSION);
    }

    /** Verify createZip with no compression. */
    @Test
    public void testCreateZipWithNoCompression() throws IOException {
        runCreateZipTest(Deflater.NO_COMPRESSION);
    }
}
