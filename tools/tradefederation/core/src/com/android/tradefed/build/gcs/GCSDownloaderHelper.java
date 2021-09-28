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
package com.android.tradefed.build.gcs;

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.FileDownloadCacheWrapper;
import com.android.tradefed.build.IFileDownloader;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.util.GCSFileDownloader;

import java.io.File;

/** Downloader for GCS bucket that takes care of caching and resolving the global config. */
public class GCSDownloaderHelper {

    private IFileDownloader mFileDownloader = null;

    /**
     * Fetch the resource from the GS path.
     *
     * @param gsPath the path where the resource is located. For example: gs://bucket/path/file
     * @return The {@link File} pointing to the retrieved resource.
     * @throws BuildRetrievalError
     */
    public File fetchTestResource(String gsPath) throws BuildRetrievalError {
        return getGCSFileDownloader().downloadFile(gsPath);
    }

    private IFileDownloader getGCSFileDownloader() {
        if (mFileDownloader == null) {
            mFileDownloader =
                    new FileDownloadCacheWrapper(
                            getHostOptions().getDownloadCacheDir(), new GCSFileDownloader());
        }
        return mFileDownloader;
    }

    /** Gets the {@link IHostOptions} instance to use. */
    private IHostOptions getHostOptions() {
        return GlobalConfiguration.getInstance().getHostOptions();
    }
}
