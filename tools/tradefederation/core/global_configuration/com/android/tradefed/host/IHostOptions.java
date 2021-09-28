/*
 * Copyright (C) 2016 The Android Open Source Project
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

import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.targetprep.DeviceFlashPreparer;

import java.io.File;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Host options holder interface.
 * This interface is used to access host-wide options.
 */
public interface IHostOptions {

    /**
     * Returns the max number of concurrent flashing to allow. Used by {@link DeviceFlashPreparer}.
     *
     * @return the concurrent flasher limit.
     */
    Integer getConcurrentFlasherLimit();

    /**
     * Returns the max number of concurrent downloads allowed. Used by {@link IBuildProvider} that
     * downloads remote builds.
     */
    Integer getConcurrentDownloadLimit();

    /** Returns the path that fastboot should use as temporary folder. */
    File getFastbootTmpDir();

    /** Returns whether or not fastbootd mode support is enabled. */
    boolean isFastbootdEnable();

    /** Returns the path used for storing downloaded artifacts. */
    File getDownloadCacheDir();

    /** Check if it should use the SingleSignOn client or not. */
    Boolean shouldUseSsoClient();

    /** Returns a Map of service account json key files. */
    Map<String, File> getServiceAccountJsonKeyFiles();

    /** Validate that the options set on {@link IHostOptions} are valid. */
    void validateOptions() throws ConfigurationException;

    /** Get labels for the host. */
    public List<String> getLabels();

    /** Known tcp-device associated with a specific IP. */
    Set<String> getKnownTcpDeviceIpPool();

    /** Known gce-device associated with a specific IP. */
    Set<String> getKnownGceDeviceIpPool();

    /** Check if it should use the zip64 format in partial download or not. */
    boolean getUseZip64InPartialDownload();

    /** Returns the network interface used to connect to remote test devices. */
    String getNetworkInterface();
}
