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

import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;

import org.json.JSONException;

import java.io.IOException;
import java.util.List;

/** An interface for interacting with the TFC backend. */
public interface IClusterClient {
    /**
     * The unique configuration object type name. Used to retrieve the singleton instance from the
     * {@link GlobalConfiguration}.
     *
     * @see IConfiguration#getConfigurationObject(String)
     */
    public static final String TYPE_NAME = "cluster_client";

    /**
     * Get a {@link IClusterEventUploader} that can be used to upload {@link ClusterCommandEvent}s.
     */
    public IClusterEventUploader<ClusterCommandEvent> getCommandEventUploader();

    /** Get a {@link IClusterEventUploader} that can be used to upload {@link ClusterHostEvent}s. */
    public IClusterEventUploader<ClusterHostEvent> getHostEventUploader();

    /**
     * Lease {@link ClusterCommand} for the give host.
     *
     * @param clusterId cluster id for the host
     * @param hostname hostname
     * @param devices deviceInfos the host has
     * @param nextClusterIds a list of next cluster IDs to lease commands from.
     * @param maxTasksTolease the max number of tasks that can current be leased
     * @return a list of {@link ClusterCommand}
     * @throws JSONException
     */
    public List<ClusterCommand> leaseHostCommands(
            final String clusterId,
            final String hostname,
            final List<ClusterDeviceInfo> devices,
            final List<String> nextClusterIds,
            final int maxTasksTolease)
            throws JSONException;

    /**
     * Get {@link TestEnvironment} for a request.
     *
     * @param requestId
     * @return a {@link TestEnvironment} object.
     * @throws IOException
     * @throws JSONException
     */
    public TestEnvironment getTestEnvironment(final String requestId)
            throws IOException, JSONException;

    /**
     * Get {@link TestResource}s for a request.
     *
     * @param requestId
     * @return a list of {@link TestResource}.
     * @throws IOException
     * @throws JSONException
     */
    public List<TestResource> getTestResources(final String requestId)
            throws IOException, JSONException;

    public TestContext getTestContext(final String requestId, final String commandId)
            throws IOException, JSONException;

    public void updateTestContext(
            final String requestId, final String commandId, TestContext testContext)
            throws IOException, JSONException;

    /**
     * Determine the state of a cluster command.
     *
     * @param requestId cluster request ID
     * @param commandId cluster command ID
     * @return cluster command's state, or {@link ClusterCommand.State#UNKNOWN} if state could not
     *     be determined
     */
    public ClusterCommand.State getCommandState(String requestId, String commandId);
}
