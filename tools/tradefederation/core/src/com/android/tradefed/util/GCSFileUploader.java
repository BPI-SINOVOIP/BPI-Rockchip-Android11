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
 * limitations under the License
 */

package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil;

import com.google.api.client.http.InputStreamContent;
import com.google.api.services.storage.Storage;
import com.google.api.services.storage.model.StorageObject;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.Collections;

/** File uploader to upload file data to google cloud storage (GCS). */
public class GCSFileUploader extends GCSCommon {
    private static final Collection<String> WRITE_SCOPE =
            Collections.singleton("https://www.googleapis.com/auth/devstorage.read_write");

    public GCSFileUploader(File jsonKeyFile) {
        super(jsonKeyFile);
    }

    public GCSFileUploader() {}

    /**
     * Upload data to a GCS bucket file. gs://[bucketName]/[gcsFileName]
     *
     * @param bucketName GCS bucket name
     * @param gcsFilename the filename.
     * @param fileContents InputStream of data to be written to the GCS File.
     * @param contentType is the MIME media type of the object being uploaded.
     * @param allowOverwrite True will allow this method to overwrite a file on GCS.
     */
    public void uploadFile(
            String bucketName,
            String gcsFilename,
            InputStream fileContents,
            String contentType,
            boolean allowOverwrite)
            throws IOException {
        InputStreamContent inputStreamData = new InputStreamContent(contentType, fileContents);

        StorageObject object = new StorageObject().setName(gcsFilename);
        if (!allowOverwrite) {
            if (getWritableStorage()
                            .objects()
                            .list(bucketName)
                            .setPrefix(gcsFilename)
                            .execute()
                            .getItems()
                    != null) {
                throw new IOException(
                        "Can't write file.  Already exists and allowOverwrite is false.");
            }
        }

        getWritableStorage().objects().insert(bucketName, object, inputStreamData).execute();

        LogUtil.CLog.d("Created file at: %s", gcsFilename);
    }

    private Storage getWritableStorage() throws IOException {
        return getStorage(WRITE_SCOPE);
    }
}
