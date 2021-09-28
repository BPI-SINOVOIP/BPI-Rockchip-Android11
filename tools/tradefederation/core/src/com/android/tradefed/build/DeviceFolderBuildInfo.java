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

import java.io.File;

/**
 * A {@link IDeviceBuildInfo} that also contains other build artifacts contained in a directory on
 * the local filesystem.
 */
public class DeviceFolderBuildInfo extends DeviceBuildInfo
        implements IDeviceBuildInfo, IFolderBuildInfo {

    private static final long serialVersionUID = BuildSerializedVersion.VERSION;

    /**
     * @see DeviceBuildInfo#DeviceBuildInfo(String, String)
     */
    public DeviceFolderBuildInfo(String buildId, String buildName) {
        super(buildId, buildName);
    }

    /**
     * @see DeviceBuildInfo#DeviceBuildInfo()
     */
    public DeviceFolderBuildInfo() {
        super();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public File getRootDir() {
        return getFile(BuildInfoFileKey.ROOT_DIRECTORY);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setRootDir(File rootDir) {
        setFile(
                BuildInfoFileKey.ROOT_DIRECTORY,
                rootDir,
                BuildInfoFileKey.ROOT_DIRECTORY.getFileKey());
    }

    /** Copy all the files from the {@link IFolderBuildInfo}. */
    public void setFolderBuild(IFolderBuildInfo folderBuild) {
        copyAllFileFrom((BuildInfo) folderBuild);
    }

    /** Copy all the files from the {@link IDeviceBuildInfo}. */
    public void setDeviceBuild(IDeviceBuildInfo deviceBuild) {
        copyAllFileFrom((BuildInfo) deviceBuild);
    }
}
