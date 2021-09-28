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
import com.android.tradefed.device.ITestDevice;

import java.io.File;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Holds information about the build under test. */
public interface IBuildInfo extends Serializable {

    /** Some properties that a {@link IBuildInfo} can have to tweak some handling of it. */
    public enum BuildInfoProperties {
        DO_NOT_COPY_ON_SHARDING,
        DO_NOT_LINK_TESTS_DIR,
        /**
         * If a copy of the build is requested, do not copy the device image file. Represented by
         * {@link BuildInfoFileKey#DEVICE_IMAGE} key.
         */
        DO_NOT_COPY_IMAGE_FILE,
    }

    /** Default value when build ID is unknown. */
    public static final String UNKNOWN_BUILD_ID = "-1";

    /** Prefix used in name to indicate the file is set to be delayed download. */
    public static final String REMOTE_FILE_PREFIX = "remote_file:";

    /** Remote file is not versioned. */
    public static final String REMOTE_FILE_VERSION = "";

    /**
     * Returns the unique identifier of build under test. Should never be null. Defaults to {@link
     * #UNKNOWN_BUILD_ID}.
     */
    public String getBuildId();

    /**
     * Sets the unique identifier of build under test. Should never be null.
     */
    public void setBuildId(String buildId);

    /**
     * Return a unique name for the tests being run.
     */
    public String getTestTag();

    /**
     * Sets the unique name for the tests being run.
     */
    public void setTestTag(String testTag);

    /**
     * Return complete name for the build being tested.
     * <p/>
     * A common implementation is to construct the build target name from a combination of
     * the build flavor and branch name. [ie (branch name)-(build flavor)]
     */
    public String getBuildTargetName();

    /**
     * Optional method to return the type of build being tested.
     * <p/>
     * A common implementation for Android platform builds is to return
     * (build product)-(build os)-(build variant).
     * ie generic-linux-userdebug
     *
     * @return the build flavor or <code>null</code> if unset/not applicable
     */
    public String getBuildFlavor();

    /**
     * @return the {@link ITestDevice} serial that this build was executed on. Returns <code>null
     * </code> if no device is associated with this build.
     */
    public String getDeviceSerial();

    /**
     * Set the build flavor.
     *
     * @param buildFlavor
     */
    public void setBuildFlavor(String buildFlavor);

    /**
     * Optional method to return the source control branch that the build being tested was
     * produced from.
     *
     * @return the build branch or <code>null</code> if unset/not applicable
     */
    public String getBuildBranch();

    /**
     * Set the build branch
     *
     * @param branch the branch name
     */
    public void setBuildBranch(String branch);

    /**
     * Set the {@link ITestDevice} serial associated with this build.
     *
     * @param serial the serial number of the {@link ITestDevice} that this build was executed with.
     */
    public void setDeviceSerial(String serial);

    /**
     * Get a set of name-value pairs of additional attributes describing the build.
     *
     * @return a {@link Map} of build attributes. Will not be <code>null</code>, but may be empty.
     */
    public Map<String, String> getBuildAttributes();

    /**
     * Add a build attribute
     *
     * @param attributeName the unique attribute name
     * @param attributeValue the attribute value
     */
    public void addBuildAttribute(String attributeName, String attributeValue);

    /**
     * Add build attributes
     *
     * @param buildAttributes Map of attributes to be added
     */
    public default void addBuildAttributes(Map<String, String> buildAttributes) {}

    /**
     * Set the {@link BuildInfoProperties} for the {@link IBuildInfo} instance. Override any
     * existing properties set before.
     *
     * @param properties The list of properties to add.
     */
    public void setProperties(BuildInfoProperties... properties);

    /** Returns a copy of the properties currently set on the {@link IBuildInfo}. */
    public Set<BuildInfoProperties> getProperties();

    /**
     * Helper method to retrieve a file with given a {@link BuildInfoFileKey}.
     *
     * @param key the {@link BuildInfoFileKey} that is requested.
     * @return the image file or <code>null</code> if not found
     */
    public default File getFile(BuildInfoFileKey key) {
        // Default implementation for projects that don't extend BuildInfo class.
        return null;
    }

    /** Returns the set of keys available to query {@link VersionedFile} via {@link #getFile}. */
    public default Set<String> getVersionedFileKeys() {
        return null;
    }

    /**
     * Helper method to retrieve a file with given name.
     * @param name
     * @return the image file or <code>null</code> if not found
     */
    public File getFile(String name);

    /**
     * Helper method to retrieve a {@link VersionedFile} with a given name.
     *
     * @param name
     * @return The versioned file or <code>null</code> if not found
     */
    public default VersionedFile getVersionedFile(String name) {
        // Default implementation for projects that don't extend BuildInfo class.
        return null;
    }

    /**
     * Helper method to retrieve a {@link VersionedFile} with a given {@link BuildInfoFileKey}.
     *
     * @param key The {@link BuildInfoFileKey} requested.
     * @return The versioned file or <code>null</code> if not found
     */
    public default VersionedFile getVersionedFile(BuildInfoFileKey key) {
        // Default implementation for projects that don't extend BuildInfo class.
        return null;
    }

    /**
     * Helper method to retrieve a list of {@link VersionedFile}s associated with a given {@link
     * BuildInfoFileKey}. If the key allows to store a list.
     *
     * @param key The {@link BuildInfoFileKey} requested.
     * @return The versioned file or <code>null</code> if not found
     */
    public default List<VersionedFile> getVersionedFiles(BuildInfoFileKey key) {
        // Default implementation for projects that don't extend BuildInfo class.
        return null;
    }

    /**
     * Returns all {@link VersionedFile}s stored in this {@link BuildInfo}.
     */
    public Collection<VersionedFile> getFiles();

    /**
     * Helper method to retrieve a file version with given name.
     * @param name
     * @return the image version or <code>null</code> if not found
     */
    public String getVersion(String name);

    /**
     * Helper method to retrieve a file version with given a {@link BuildInfoFileKey}.
     *
     * @param key The {@link BuildInfoFileKey} requested.
     * @return the image version or <code>null</code> if not found
     */
    public default String getVersion(BuildInfoFileKey key) {
        // Default implementation for project that don't extends BuildInfo class.
        return null;
    }

    /**
     * Stores an file with given name in this build info.
     *
     * @param name the unique name of the file
     * @param file the local {@link File}
     * @param version the file version
     */
    public void setFile(String name, File file, String version);

    /**
     * Stores an file given a {@link BuildInfoFileKey} in this build info.
     *
     * @param key the unique name of the file based on {@link BuildInfoFileKey}.
     * @param file the local {@link File}
     * @param version the file version
     */
    public default void setFile(BuildInfoFileKey key, File file, String version) {
        // Default implementation for projects that don't extend BuildInfo class.
    }

    /**
     * Gets a copy of the set of local app apk file(s) and their versions. The returned order
     * matches the order in which the apks were added to the {@code IAppBuildInfo}.
     */
    public default List<VersionedFile> getAppPackageFiles() {
        return new ArrayList<>();
    }

    /**
     * Adds the local apk file and its associated version. Note that apks will be returned from
     * {@link #getAppPackageFiles()} in the order in which they were added by this method.
     */
    public default void addAppPackageFile(File appPackageFile, String version) {
        // Default implementation for projects that don't extend BuildInfo class.
    }

    /**
     * Clean up any temporary build files
     */
    public void cleanUp();

    /** Version of {@link #cleanUp()} where some files are not deleted. */
    public void cleanUp(List<File> doNotDelete);

    /**
     * Clones the {@link IBuildInfo} object.
     */
    public IBuildInfo clone();

    /** Serialize a the BuildInfo instance into a protobuf. */
    public default BuildInformation.BuildInfo toProto() {
        // Default implementation for project that don't extends BuildInfo class.
        return null;
    }

    /** Get the paths for build artifacts that are delayed download. */
    public default Set<File> getRemoteFiles() {
        return null;
    }

    /**
     * Stage a file that's part of remote files in the build info's root dir.
     *
     * <p>TODO(b/138416078): Remove this interface and its caller when modules required by a test
     * can be properly built output to the test module's directory itself.
     *
     * @param fileName Name of the file to be located in remote files.
     * @param workingDir a {@link File} object of the directory to stage the file.
     * @return the {@link File} object of the file staged in local workingDir.
     */
    public default File stageRemoteFile(String fileName, File workingDir) {
        return null;
    }
}
