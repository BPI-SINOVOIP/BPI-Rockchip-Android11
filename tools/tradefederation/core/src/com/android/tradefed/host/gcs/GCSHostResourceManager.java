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
package com.android.tradefed.host.gcs;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.FileDownloadCacheWrapper;
import com.android.tradefed.build.IFileDownloader;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.host.LocalHostResourceManager;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.GCSFileDownloader;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;

/** Download host resource from GCS (Google cloud storage). */
@OptionClass(alias = "gcs-hrm", global_namespace = false)
public class GCSHostResourceManager extends LocalHostResourceManager {

    private IFileDownloader mFileDownloader = null;

    /**
     * Download host resource to a local file from GCS.
     *
     * @param name the name of the host resource.
     * @param value the remote path of the host resource.
     * @return the local file.
     */
    @Override
    protected File fetchHostResource(String name, String value) throws ConfigurationException {
        try {
            return getGCSFileDownloader().downloadFile(value);
        } catch (BuildRetrievalError e) {
            throw new ConfigurationException(e.getMessage(), e);
        }
    }

    /**
     * Clear a local host resource.
     *
     * @param name the id of the host resource.
     * @param localFile the local file.
     */
    @Override
    protected void clearHostResource(String name, File localFile) {
        FileUtil.recursiveDelete(localFile);
    }

    @VisibleForTesting
    IFileDownloader getGCSFileDownloader() {
        if (mFileDownloader == null) {
            mFileDownloader =
                    new FileDownloadCacheWrapper(
                            getHostOptions().getDownloadCacheDir(), new GCSFileDownloader());
        }
        return mFileDownloader;
    }

    private IHostOptions getHostOptions() {
        return GlobalConfiguration.getInstance().getHostOptions();
    }
}
