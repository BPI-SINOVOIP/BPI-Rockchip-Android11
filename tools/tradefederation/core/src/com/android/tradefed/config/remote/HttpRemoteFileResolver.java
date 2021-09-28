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
package com.android.tradefed.config.remote;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.net.HttpHelper;
import com.android.tradefed.util.net.IHttpHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Map;

import javax.annotation.Nonnull;

/** Implementation of {@link IRemoteFileResolver} that allows downloading remote file via http */
public class HttpRemoteFileResolver implements IRemoteFileResolver {

    public static final String PROTOCOL_HTTP = "http";

    @Override
    public File resolveRemoteFiles(File consideredFile, Map<String, String> queryArgs)
            throws BuildRetrievalError {
        // Don't use absolute path as it would not start with gs:
        String path = consideredFile.getPath();
        // Replace the very first / by // to be http:// again.
        path = path.replaceFirst(":/", "://");

        IHttpHelper downloader = getDownloader();
        File downloadedFile = null;
        try {
            downloadedFile =
                    FileUtil.createTempFile(
                            FileUtil.getBaseName(consideredFile.getName()),
                            FileUtil.getExtension(consideredFile.getName()));
            downloader.doGet(path, new FileOutputStream(downloadedFile));
            return DynamicRemoteFileResolver.unzipIfRequired(downloadedFile, queryArgs);
        } catch (IOException | RuntimeException e) {
            FileUtil.deleteFile(downloadedFile);
            throw new BuildRetrievalError(
                    String.format("Failed to download %s due to: %s", path, e.getMessage()), e);
        }
    }

    @Override
    public @Nonnull String getSupportedProtocol() {
        return PROTOCOL_HTTP;
    }

    @VisibleForTesting
    protected IHttpHelper getDownloader() {
        return new HttpHelper();
    }
}
