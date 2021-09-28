/*
 * Copyright (C) 2010 The Android Open Source Project
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
package com.android.tradefed.build;

import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.proto.BuildInformation;
import com.android.tradefed.build.proto.BuildInformation.BuildFile;
import com.android.tradefed.build.proto.BuildInformation.KeyBuildFilePair;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.UniqueMultiMap;

import com.google.common.base.MoreObjects;
import com.google.common.base.Objects;

import java.io.File;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Generic implementation of a {@link IBuildInfo} that should be associated
 * with a {@link ITestDevice}.
 */
public class BuildInfo implements IBuildInfo {
    private static final long serialVersionUID = BuildSerializedVersion.VERSION;
    private static final String BUILD_ALIAS_KEY = "build_alias";

    private String mBuildId = UNKNOWN_BUILD_ID;
    private String mTestTag = "stub";
    private String mBuildTargetName = "stub";
    private final UniqueMultiMap<String, String> mBuildAttributes =
            new UniqueMultiMap<String, String>();
    // TODO: once deployed make non-transient
    private Map<String, VersionedFile> mVersionedFileMap;
    private transient MultiMap<String, VersionedFile> mVersionedFileMultiMap;
    private String mBuildFlavor = null;
    private String mBuildBranch = null;
    private String mDeviceSerial = null;

    /** File handling properties: Some files of the BuildInfo might requires special handling */
    private final Set<BuildInfoProperties> mProperties = new HashSet<>();

    private static final String[] FILE_NOT_TO_CLONE =
            new String[] {
                BuildInfoFileKey.TESTDIR_IMAGE.getFileKey(),
                BuildInfoFileKey.HOST_LINKED_DIR.getFileKey(),
                BuildInfoFileKey.TARGET_LINKED_DIR.getFileKey(),
            };

    /**
     * Creates a {@link BuildInfo} using default attribute values.
     */
    public BuildInfo() {
        mVersionedFileMap = new Hashtable<String, VersionedFile>();
        mVersionedFileMultiMap = new MultiMap<String, VersionedFile>();
    }

    /**
     * Creates a {@link BuildInfo}
     *
     * @param buildId the build id
     * @param buildTargetName the build target name
     */
    public BuildInfo(String buildId, String buildTargetName) {
        this();
        mBuildId = buildId;
        mBuildTargetName = buildTargetName;
    }

    /**
     * Creates a {@link BuildInfo}, populated with attributes given in another build.
     *
     * @param buildToCopy
     */
    BuildInfo(BuildInfo buildToCopy) {
        this(buildToCopy.getBuildId(), buildToCopy.getBuildTargetName());
        addAllBuildAttributes(buildToCopy);
        try {
            addAllFiles(buildToCopy);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getBuildId() {
        return mBuildId;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuildId(String buildId) {
        mBuildId = buildId;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setTestTag(String testTag) {
        mTestTag = testTag;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getTestTag() {
        return mTestTag;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getDeviceSerial() {
        return mDeviceSerial;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Map<String, String> getBuildAttributes() {
        return mBuildAttributes.getUniqueMap();
    }

    /** {@inheritDoc} */
    @Override
    public void setProperties(BuildInfoProperties... properties) {
        mProperties.clear();
        mProperties.addAll(Arrays.asList(properties));
    }

    /** {@inheritDoc} */
    @Override
    public Set<BuildInfoProperties> getProperties() {
        return new HashSet<>(mProperties);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getBuildTargetName() {
        return mBuildTargetName;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void addBuildAttribute(String attributeName, String attributeValue) {
        mBuildAttributes.put(attributeName, attributeValue);
    }

    /** {@inheritDoc} */
    @Override
    public void addBuildAttributes(Map<String, String> buildAttributes) {
        mBuildAttributes.putAll(buildAttributes);
    }

    /**
     * Helper method to copy build attributes, branch, and flavor from other build.
     */
    protected void addAllBuildAttributes(BuildInfo build) {
        mBuildAttributes.putAll(build.getAttributesMultiMap());
        setBuildFlavor(build.getBuildFlavor());
        setBuildBranch(build.getBuildBranch());
        setTestTag(build.getTestTag());
    }

    protected MultiMap<String, String> getAttributesMultiMap() {
        return mBuildAttributes;
    }

    /**
     * Helper method to copy all files from the other build.
     *
     * <p>Creates new hardlinks to the files so that each build will have a unique file path to the
     * file.
     *
     * @throws IOException if an exception is thrown when creating the hardlink.
     */
    protected void addAllFiles(BuildInfo build) throws IOException {
        for (Map.Entry<String, VersionedFile> fileEntry : build.getVersionedFileMap().entrySet()) {
            File origFile = fileEntry.getValue().getFile();
            if (applyBuildProperties(fileEntry.getValue(), build, this)) {
                continue;
            }
            File copyFile;
            if (origFile.isDirectory()) {
                copyFile = FileUtil.createTempDir(fileEntry.getKey());
                FileUtil.recursiveHardlink(origFile, copyFile);
            } else {
                // Only using createTempFile to create a unique dest filename
                copyFile = FileUtil.createTempFile(fileEntry.getKey(),
                        FileUtil.getExtension(origFile.getName()));
                copyFile.delete();
                FileUtil.hardlinkFile(origFile, copyFile);
            }
            setFile(fileEntry.getKey(), copyFile, fileEntry.getValue().getVersion());
        }
    }

    /**
     * Allow to apply some of the {@link com.android.tradefed.build.IBuildInfo.BuildInfoProperties}
     * and possibly do a different handling.
     *
     * @param origFileConsidered The currently looked at {@link VersionedFile}.
     * @param build the original build being cloned
     * @param receiver the build receiving the information.
     * @return True if we applied the properties and further handling should be skipped. False
     *     otherwise.
     */
    protected boolean applyBuildProperties(
            VersionedFile origFileConsidered, IBuildInfo build, IBuildInfo receiver) {
        // If the no copy on sharding is set, that means the tests dir will be shared and should
        // not be copied.
        if (getProperties().contains(BuildInfoProperties.DO_NOT_COPY_ON_SHARDING)) {
            for (String name : FILE_NOT_TO_CLONE) {
                if (origFileConsidered.getFile().equals(build.getFile(name))) {
                    receiver.setFile(
                            name, origFileConsidered.getFile(), origFileConsidered.getVersion());
                    return true;
                }
            }
        }
        if (getProperties().contains(BuildInfoProperties.DO_NOT_COPY_IMAGE_FILE)) {
            if (origFileConsidered.equals(build.getVersionedFile(BuildInfoFileKey.DEVICE_IMAGE))) {
                CLog.d("Skip copying of device_image.");
                return true;
            }
        }
        return false;
    }

    protected Map<String, VersionedFile> getVersionedFileMap() {
        return mVersionedFileMultiMap.getUniqueMap();
    }

    protected MultiMap<String, VersionedFile> getVersionedFileMapFull() {
        return new MultiMap<>(mVersionedFileMultiMap);
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getVersionedFileKeys() {
        return mVersionedFileMultiMap.keySet();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public File getFile(String name) {
        List<VersionedFile> fileRecords = mVersionedFileMultiMap.get(name);
        if (fileRecords == null || fileRecords.isEmpty()) {
            return null;
        }
        return fileRecords.get(0).getFile();
    }

    /** {@inheritDoc} */
    @Override
    public File getFile(BuildInfoFileKey key) {
        return getFile(key.getFileKey());
    }

    /** {@inheritDoc} */
    @Override
    public final VersionedFile getVersionedFile(String name) {
        List<VersionedFile> fileRecords = mVersionedFileMultiMap.get(name);
        if (fileRecords == null || fileRecords.isEmpty()) {
            return null;
        }
        return fileRecords.get(0);
    }

    /** {@inheritDoc} */
    @Override
    public VersionedFile getVersionedFile(BuildInfoFileKey key) {
        return getVersionedFile(key.getFileKey());
    }

    /** {@inheritDoc} */
    @Override
    public final List<VersionedFile> getVersionedFiles(BuildInfoFileKey key) {
        if (!key.isList()) {
            throw new UnsupportedOperationException(
                    String.format("Key %s does not support list of files.", key.getFileKey()));
        }
        return mVersionedFileMultiMap.get(key.getFileKey());
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Collection<VersionedFile> getFiles() {
        return mVersionedFileMultiMap.values();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getVersion(String name) {
        List<VersionedFile> fileRecords = mVersionedFileMultiMap.get(name);
        if (fileRecords == null || fileRecords.isEmpty()) {
            return null;
        }
        return fileRecords.get(0).getVersion();
    }

    /** {@inheritDoc} */
    @Override
    public String getVersion(BuildInfoFileKey key) {
        return getVersion(key.getFileKey());
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setFile(String name, File file, String version) {
        if (!mVersionedFileMap.containsKey(name)) {
            mVersionedFileMap.put(name, new VersionedFile(file, version));
        }
        if (mVersionedFileMultiMap.containsKey(name)) {
            BuildInfoFileKey key = BuildInfoFileKey.fromString(name);
            // If the key is a list, we will add it to the map.
            if (key == null || !key.isList()) {
                CLog.e(
                        "Device build already contains a file for %s in thread %s",
                        name, Thread.currentThread().getName());
                return;
            }
        }
        mVersionedFileMultiMap.put(name, new VersionedFile(file, version));
    }

    /** {@inheritDoc} */
    @Override
    public void setFile(BuildInfoFileKey key, File file, String version) {
        setFile(key.getFileKey(), file, version);
    }

    /** {@inheritDoc} */
    @Override
    public List<VersionedFile> getAppPackageFiles() {
        List<VersionedFile> origList = getVersionedFiles(BuildInfoFileKey.PACKAGE_FILES);
        List<VersionedFile> listCopy = new ArrayList<VersionedFile>();
        if (origList != null) {
            listCopy.addAll(origList);
        }
        return listCopy;
    }

    /** {@inheritDoc} */
    @Override
    public void addAppPackageFile(File appPackageFile, String version) {
        setFile(BuildInfoFileKey.PACKAGE_FILES, appPackageFile, version);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void cleanUp() {
        for (VersionedFile fileRecord : mVersionedFileMultiMap.values()) {
            FileUtil.recursiveDelete(fileRecord.getFile());
        }
        mVersionedFileMultiMap.clear();
    }

    /** {@inheritDoc} */
    @Override
    public void cleanUp(List<File> doNotClean) {
        if (doNotClean == null) {
            cleanUp();
        }
        for (VersionedFile fileRecord : mVersionedFileMultiMap.values()) {
            if (!doNotClean.contains(fileRecord.getFile())) {
                FileUtil.recursiveDelete(fileRecord.getFile());
            }
        }
        refreshVersionedFiles();
    }

    /**
     * Run through all the {@link VersionedFile} and remove from the map the one that do not exists.
     */
    private void refreshVersionedFiles() {
        Set<String> keys = new HashSet<>(mVersionedFileMultiMap.keySet());
        for (String key : keys) {
            for (VersionedFile file : mVersionedFileMultiMap.get(key)) {
                if (!file.getFile().exists()) {
                    mVersionedFileMultiMap.remove(key);
                }
            }
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public IBuildInfo clone() {
        BuildInfo copy = null;
        try {
            copy =
                    this.getClass()
                            .getDeclaredConstructor(String.class, String.class)
                            .newInstance(getBuildId(), getBuildTargetName());
        } catch (InstantiationException
                | IllegalAccessException
                | IllegalArgumentException
                | InvocationTargetException
                | NoSuchMethodException
                | SecurityException e) {
            CLog.e("Failed to clone the build info.");
            throw new RuntimeException(e);
        }
        copy.addAllBuildAttributes(this);
        copy.setProperties(this.getProperties().toArray(new BuildInfoProperties[0]));
        try {
            copy.addAllFiles(this);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        copy.setBuildBranch(mBuildBranch);
        copy.setBuildFlavor(mBuildFlavor);
        copy.setDeviceSerial(mDeviceSerial);

        return copy;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getBuildFlavor() {
        return mBuildFlavor;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuildFlavor(String buildFlavor) {
        mBuildFlavor = buildFlavor;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String getBuildBranch() {
        return mBuildBranch;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuildBranch(String branch) {
        mBuildBranch = branch;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDeviceSerial(String serial) {
        mDeviceSerial = serial;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int hashCode() {
        return Objects.hashCode(mBuildAttributes, mBuildBranch, mBuildFlavor, mBuildId,
                mBuildTargetName, mTestTag, mDeviceSerial);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (getClass() != obj.getClass()) {
            return false;
        }
        BuildInfo other = (BuildInfo) obj;
        return Objects.equal(mBuildAttributes, other.mBuildAttributes)
                && Objects.equal(mBuildBranch, other.mBuildBranch)
                && Objects.equal(mBuildFlavor, other.mBuildFlavor)
                && Objects.equal(mBuildId, other.mBuildId)
                && Objects.equal(mBuildTargetName, other.mBuildTargetName)
                && Objects.equal(mTestTag, other.mTestTag)
                && Objects.equal(mDeviceSerial, other.mDeviceSerial);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public String toString() {
        return MoreObjects.toStringHelper(this.getClass())
                .omitNullValues()
                .add("build_alias", getBuildAttributes().get(BUILD_ALIAS_KEY))
                .add("bid", mBuildId)
                .add("target", mBuildTargetName)
                .add("build_flavor", mBuildFlavor)
                .add("branch", mBuildBranch)
                .add("serial", mDeviceSerial)
                .toString();
    }

    /** {@inheritDoc} */
    @Override
    public BuildInformation.BuildInfo toProto() {
        BuildInformation.BuildInfo.Builder protoBuilder = BuildInformation.BuildInfo.newBuilder();
        if (getBuildId() != null) {
            protoBuilder.setBuildId(getBuildId());
        }
        if (getBuildFlavor() != null) {
            protoBuilder.setBuildFlavor(getBuildFlavor());
        }
        if (getBuildBranch() != null) {
            protoBuilder.setBranch(getBuildBranch());
        }
        // Attributes
        protoBuilder.putAllAttributes(getBuildAttributes());
        // Populate the versioned file
        for (String fileKey : mVersionedFileMultiMap.keySet()) {
            KeyBuildFilePair.Builder buildFile = KeyBuildFilePair.newBuilder();
            buildFile.setBuildFileKey(fileKey);
            for (VersionedFile vFile : mVersionedFileMultiMap.get(fileKey)) {
                BuildFile.Builder fileInformation = BuildFile.newBuilder();
                fileInformation.setVersion(vFile.getVersion());
                if (fileKey.startsWith(IBuildInfo.REMOTE_FILE_PREFIX)) {
                    // Remote file doesn't exist on local cache, so don't save absolute path.
                    fileInformation.setLocalPath(vFile.getFile().toString());
                } else {
                    fileInformation.setLocalPath(vFile.getFile().getAbsolutePath());
                }
                buildFile.addFile(fileInformation);
            }
            protoBuilder.addVersionedFile(buildFile);
        }
        protoBuilder.setBuildInfoClass(this.getClass().getCanonicalName());
        return protoBuilder.build();
    }

    /** Copy all the {@link VersionedFile} from a given build to this one. */
    public final void copyAllFileFrom(BuildInfo build) {
        MultiMap<String, VersionedFile> versionedMap = build.getVersionedFileMapFull();
        for (String versionedFile : versionedMap.keySet()) {
            for (VersionedFile vFile : versionedMap.get(versionedFile)) {
                setFile(versionedFile, vFile.getFile(), vFile.getVersion());
            }
        }
    }

    /** Special serialization to handle the new underlying type. */
    private void writeObject(ObjectOutputStream outputStream) throws IOException {
        outputStream.defaultWriteObject();
        outputStream.writeObject(mVersionedFileMultiMap);
    }

    /** Special java method that allows for custom deserialization. */
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
        try {
            mVersionedFileMultiMap = (MultiMap<String, VersionedFile>) in.readObject();
        } catch (IOException | ClassNotFoundException e) {
            mVersionedFileMultiMap = new MultiMap<>();
        }
    }

    /** Inverse operation to {@link #toProto()} to get the instance back. */
    public static IBuildInfo fromProto(BuildInformation.BuildInfo protoBuild) {
        IBuildInfo buildInfo;
        String buildClass = protoBuild.getBuildInfoClass();
        if (buildClass.isEmpty()) {
            buildInfo = new BuildInfo();
        } else {
            // Restore the original type of build info.
            try {
                buildInfo =
                        (BuildInfo)
                                Class.forName(buildClass).getDeclaredConstructor().newInstance();
            } catch (InstantiationException
                    | IllegalAccessException
                    | ClassNotFoundException
                    | InvocationTargetException
                    | NoSuchMethodException e) {
                throw new RuntimeException(e);
            }
        }
        // Build id
        if (!protoBuild.getBuildId().isEmpty()) {
            buildInfo.setBuildId(protoBuild.getBuildId());
        }
        // Build Flavor
        if (!protoBuild.getBuildFlavor().isEmpty()) {
            buildInfo.setBuildFlavor(protoBuild.getBuildFlavor());
        }
        // Build Branch
        if (!protoBuild.getBranch().isEmpty()) {
            buildInfo.setBuildBranch(protoBuild.getBranch());
        }
        // Attributes
        for (String key : protoBuild.getAttributes().keySet()) {
            buildInfo.addBuildAttribute(key, protoBuild.getAttributes().get(key));
        }
        // Versioned File
        for (KeyBuildFilePair filePair : protoBuild.getVersionedFileList()) {
            for (BuildFile buildFile : filePair.getFileList()) {
                buildInfo.setFile(
                        filePair.getBuildFileKey(),
                        new File(buildFile.getLocalPath()),
                        buildFile.getVersion());
            }
        }
        return buildInfo;
    }

    /** {@inheritDoc} */
    @Override
    public Set<File> getRemoteFiles() {
        Set<File> remoteFiles = new HashSet<>();
        for (String fileKey : mVersionedFileMultiMap.keySet()) {
            if (fileKey.startsWith(IBuildInfo.REMOTE_FILE_PREFIX)) {
                // Remote file is not versioned, there should be only one entry.
                remoteFiles.add(mVersionedFileMultiMap.get(fileKey).get(0).getFile());
            }
        }
        return remoteFiles;
    }

    /** {@inheritDoc} */
    @Override
    public File stageRemoteFile(String fileName, File workingDir) {
        InvocationMetricLogger.addInvocationMetrics(
                InvocationMetricKey.STAGE_TESTS_INDIVIDUAL_DOWNLOADS, fileName);
        List<String> includeFilters = Arrays.asList(String.format("/%s$", fileName));
        for (File file : getRemoteFiles()) {
            try {
                new DynamicRemoteFileResolver()
                        .resolvePartialDownloadZip(
                                workingDir, file.toString(), includeFilters, null);
            } catch (BuildRetrievalError e) {
                throw new RuntimeException(e);
            }

            File stagedFile = FileUtil.findFile(workingDir, fileName);
            if (stagedFile != null) {
                return stagedFile;
            }
        }
        return null;
    }
}
