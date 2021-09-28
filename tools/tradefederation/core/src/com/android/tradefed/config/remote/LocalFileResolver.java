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

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.result.error.InfraErrorIdentifier;

import java.io.File;

import javax.annotation.Nonnull;

/** Implementation of {@link IRemoteFileResolver} that allows linking local files */
public class LocalFileResolver implements IRemoteFileResolver {

    public static final String PROTOCOL = "file";

    @Override
    public File resolveRemoteFiles(File consideredFile) throws BuildRetrievalError {
        // Don't use absolute path as it would not start with gs:
        String path = consideredFile.getPath();
        String pathWithoutProtocol = path.replaceFirst(PROTOCOL + ":", "");
        File localFile = new File(pathWithoutProtocol);
        if (localFile.exists()) {
            return localFile;
        }
        throw new BuildRetrievalError(
                String.format("Failed to find local file %s.", localFile),
                InfraErrorIdentifier.ARTIFACT_NOT_FOUND);
    }

    @Override
    public @Nonnull String getSupportedProtocol() {
        return PROTOCOL;
    }
}
