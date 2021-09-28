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

import java.io.File;

/** Interface to manager host resource. */
public interface IHostResourceManager {

    /**
     * Set up host resources. Host resources might come from different places, remote on cloud or
     * local files. This interface provides a unified way for tradefed to get the host resource it
     * needs. setup should properly download files to local and later tradefed can get the local by
     * the host resource name through getFile.
     *
     * @throws ConfigurationException
     */
    public void setup() throws ConfigurationException;

    /**
     * Get host resource local file by the resource id.
     *
     * @param name the resource id of the host resource.
     * @return The local file of the host resource.
     */
    public File getFile(String name);

    /** Clean up host resources. */
    public void cleanup();
}
