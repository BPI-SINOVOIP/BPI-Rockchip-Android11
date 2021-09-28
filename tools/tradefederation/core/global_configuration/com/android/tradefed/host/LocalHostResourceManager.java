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
package com.android.tradefed.host;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

/** Manager host resource. The host resources are local files. */
@OptionClass(alias = "local-hrm", global_namespace = false)
public class LocalHostResourceManager implements IHostResourceManager {

    @Option(
        name = "host-resource",
        description = "The host resources to download for the current host."
    )
    private Map<String, String> mHostResources = new HashMap<>();

    @Option(name = "disable", description = "Disable the host resource manager or not.")
    private boolean mDisable = false;

    private Map<String, File> mDownloadedHostResources = new HashMap<>();

    /** {@inheritDoc} */
    @Override
    public void setup() throws ConfigurationException {
        if (mDisable) {
            return;
        }
        for (Map.Entry<String, String> entry : mHostResources.entrySet()) {
            File localFile = fetchHostResource(entry.getKey(), entry.getValue());
            mDownloadedHostResources.put(entry.getKey(), localFile);
        }
    }

    /** {@inheritDoc} */
    @Override
    public File getFile(String name) {
        return mDownloadedHostResources.get(name);
    }

    /** {@inheritDoc} */
    @Override
    public void cleanup() {
        if (mDisable) {
            return;
        }
        for (Map.Entry<String, File> entry : mDownloadedHostResources.entrySet()) {
            clearHostResource(entry.getKey(), entry.getValue());
        }
    }

    /**
     * Clear a local host resource.
     *
     * @param name the id of the host resource.
     * @param localFile the local file.
     */
    protected void clearHostResource(String name, File localFile) {
        // For LocalHostResourceManager, do nothing.
    }

    /**
     * Use a local file a host resource.
     *
     * @param name the name of the host resource.
     * @param value the local path of the host resource.
     * @return the local file.
     */
    protected File fetchHostResource(String name, String value) throws ConfigurationException {
        File localFile = new File(value);
        if (!localFile.exists()) {
            throw new ConfigurationException(String.format("File %s doesn't exist.", value));
        }
        return localFile;
    }
}
