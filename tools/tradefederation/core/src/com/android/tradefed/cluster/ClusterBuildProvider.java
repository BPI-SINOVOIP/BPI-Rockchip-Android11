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
package com.android.tradefed.cluster;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.util.ZipUtil2;

import org.apache.commons.compress.archivers.zip.ZipFile;
import java.io.File;
import java.io.IOException;
import java.util.Map;
import java.util.Map.Entry;
import java.util.TreeMap;

/** A {@link IBuildProvider} to download TFC test resources. */
@OptionClass(alias = "cluster", global_namespace = false)
public class ClusterBuildProvider implements IBuildProvider {

    private static final String DEFAULT_FILE_VERSION = "0";

    @Option(name = "root-dir", description = "A root directory", mandatory = true)
    private File mRootDir;

    @Option(name = "test-resource", description = "Test resources", mandatory = true)
    private Map<String, String> mTestResources = new TreeMap<>();

    @Option(name = "build-id", description = "Build ID")
    private String mBuildId = IBuildInfo.UNKNOWN_BUILD_ID;

    @Option(name = "build-target", description = "Build target name")
    private String mBuildTarget = "stub";

    @Override
    public IBuildInfo getBuild() throws BuildRetrievalError {
        try {
            mRootDir.mkdirs();
            final IBuildInfo buildInfo = new ClusterBuildInfo(mRootDir, mBuildId, mBuildTarget);
            final TestResourceDownloader downloader = new TestResourceDownloader(mRootDir);
            for (final Entry<String, String> entry : mTestResources.entrySet()) {
                final TestResource resource = new TestResource(entry.getKey(), entry.getValue());
                final File file = downloader.download(resource);
                buildInfo.setFile(resource.getName(), file, DEFAULT_FILE_VERSION);
                if (file.getName().endsWith(".zip")) {
                    // If a zip file is downloaded to a subfolder, unzip there.
                    extractZip(file, file.getParentFile());
                }
            }
            return buildInfo;
        } catch (IOException e) {
            throw new BuildRetrievalError("failed to get test resources", e);
        }
    }

    /** Extracts the zip to a root dir. */
    private void extractZip(File zip, File destDir) throws IOException {
        try (ZipFile zipFile = new ZipFile(zip)) {
            ZipUtil2.extractZip(zipFile, destDir);
        } catch (IOException e) {
            throw e;
        }
    }

    @Override
    public void buildNotTested(IBuildInfo info) {}

    @Override
    public void cleanUp(IBuildInfo info) {
        if (!(info instanceof ClusterBuildInfo)) {
            throw new IllegalArgumentException("info is not an instance of ClusterBuildInfo");
        }
    }

    @VisibleForTesting
    File getRootDir() {
        return mRootDir;
    }

    @VisibleForTesting
    Map<String, String> getTestResources() {
        return mTestResources;
    }
}
