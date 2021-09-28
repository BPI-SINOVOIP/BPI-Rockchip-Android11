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
import com.android.tradefed.host.IHostOptions;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.IRestApiHelper;
import com.android.tradefed.util.RestApiHelper;

import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.HttpTransport;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.common.annotations.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A {@link IClusterClient} implementation for interacting with the TFC backend. */
public class ClusterClient implements IClusterClient {
    private static final String DEFAULT_TFC_URL_BASE =
            "https://tradefed-cluster.googleplex.com/_ah/api/tradefed_cluster/v1/";

    /** The {@link IClusterOptions} instance used to store cluster-related settings. */
    private IClusterOptions mClusterOptions;
    /** The {@link IHostOptions} instance used to store host wide related settings. */
    private IHostOptions mHostOptions;

    private IRestApiHelper mApiHelper = null;

    private IClusterEventUploader<ClusterCommandEvent> mCommandEventUploader = null;
    private IClusterEventUploader<ClusterHostEvent> mHostEventUploader = null;

    /**
     * A {@link IClusterEventUploader} implementation for uploading {@link ClusterCommandEvent}s.
     */
    private class ClusterCommandEventUploader extends ClusterEventUploader<ClusterCommandEvent> {

        private static final String REST_API_METHOD = "command_events";
        private static final String DATA_KEY = "command_events";

        @Override
        protected void doUploadEvents(List<ClusterCommandEvent> events) throws IOException {
            try {
                JSONObject eventData = buildPostData(events, DATA_KEY);
                getApiHelper().execute("POST", new String[] {REST_API_METHOD}, null, eventData);
            } catch (JSONException e) {
                throw new IOException(e);
            }
        }
    }

    /** A {@link IClusterEventUploader} implementation for uploading {@link ClusterHostEvent}s. */
    private class ClusterHostEventUploader extends ClusterEventUploader<ClusterHostEvent> {
        private static final String REST_API_METHOD = "host_events";
        private static final String DATA_KEY = "host_events";

        @Override
        protected void doUploadEvents(List<ClusterHostEvent> events) throws IOException {
            try {
                JSONObject eventData = buildPostData(events, DATA_KEY);
                getApiHelper().execute("POST", new String[] {REST_API_METHOD}, null, eventData);
            } catch (JSONException e) {
                throw new IOException(e);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public List<ClusterCommand> leaseHostCommands(
            final String clusterId,
            final String hostname,
            final List<ClusterDeviceInfo> deviceInfos,
            final List<String> nextClusterIds,
            final int maxTasksTolease)
            throws JSONException {
        // Make an API call to lease some work
        final Map<String, Object> options = new HashMap<>();
        options.put("cluster", clusterId);
        options.put("hostname", hostname);
        options.put("num_tasks", Integer.toString(maxTasksTolease));

        JSONObject data = new JSONObject();
        if (nextClusterIds != null && !nextClusterIds.isEmpty()) {
            JSONArray ids = new JSONArray();
            for (String id : nextClusterIds) {
                ids.put(id);
            }
            data.put("next_cluster_ids", ids);
        }
        JSONArray deviceInfoJsons = new JSONArray();
        for (ClusterDeviceInfo d : deviceInfos) {
            deviceInfoJsons.put(d.toJSON());
        }
        // Add device infos in the request body. TFC will match devices based on those infos.
        data.put("device_infos", deviceInfoJsons);
        try {
            // By default, execute(..) will throw an exception on HTTP error codes.
            HttpResponse httpResponse =
                    getApiHelper()
                            .execute(
                                    "POST",
                                    new String[] {"tasks", "leasehosttasks"},
                                    options,
                                    data);
            return parseCommandTasks(httpResponse);
        } catch (IOException e) {
            // May be transient. Log a warning and we'll try again later.
            CLog.w("Failed to lease commands: %s", e);
            return Collections.<ClusterCommand>emptyList();
        }
    }

    /** {@inheritDoc} */
    @Override
    public TestEnvironment getTestEnvironment(final String requestId)
            throws IOException, JSONException {
        final Map<String, Object> options = new HashMap<>();
        final HttpResponse response =
                getApiHelper()
                        .execute(
                                "GET",
                                new String[] {"requests", requestId, "test_environment"},
                                options,
                                null);
        final String content = StreamUtil.getStringFromStream(response.getContent());
        CLog.d(content);
        return TestEnvironment.fromJson(new JSONObject(content));
    }

    /** {@inheritDoc} */
    @Override
    public List<TestResource> getTestResources(final String requestId)
            throws IOException, JSONException {
        final Map<String, Object> options = new HashMap<>();
        final HttpResponse response =
                getApiHelper()
                        .execute(
                                "GET",
                                new String[] {"requests", requestId, "test_resources"},
                                options,
                                null);
        final String content = StreamUtil.getStringFromStream(response.getContent());
        CLog.d(content);
        final JSONArray jsonArray = new JSONObject(content).optJSONArray("test_resources");
        if (jsonArray == null) {
            return new ArrayList<>();
        }
        return TestResource.fromJsonArray(jsonArray);
    }

    /** {@inheritDoc} */
    @Override
    public TestContext getTestContext(final String requestId, final String commandId)
            throws IOException, JSONException {
        final Map<String, Object> options = new HashMap<>();
        final HttpResponse response =
                getApiHelper()
                        .execute(
                                "GET",
                                new String[] {
                                    "requests", requestId, "commands", commandId, "test_context"
                                },
                                options,
                                null);
        final String content = StreamUtil.getStringFromStream(response.getContent());
        CLog.d(content);
        return TestContext.fromJson(new JSONObject(content));
    }

    /** {@inheritDoc} */
    @Override
    public void updateTestContext(
            final String requestId, final String commandId, TestContext testContext)
            throws IOException, JSONException {
        final Map<String, Object> options = new HashMap<>();
        final HttpResponse response =
                getApiHelper()
                        .execute(
                                "POST",
                                new String[] {
                                    "requests", requestId, "commands", commandId, "test_context"
                                },
                                options,
                                testContext.toJson());
        final String content = StreamUtil.getStringFromStream(response.getContent());
        CLog.d(content);
    }

    @Override
    public ClusterCommand.State getCommandState(String requestId, String commandId) {
        try {
            HttpResponse response =
                    getApiHelper()
                            .execute(
                                    "GET",
                                    new String[] {"requests", requestId, "commands", commandId},
                                    new HashMap<>(),
                                    null);
            String content = StreamUtil.getStringFromStream(response.getContent());
            String value = new JSONObject(content).getString("state");
            return ClusterCommand.State.valueOf(value);
        } catch (IOException | JSONException | IllegalArgumentException e) {
            CLog.w("Failed to get state of request %s command %s", requestId, commandId);
            CLog.e(e);
            return ClusterCommand.State.UNKNOWN;
        }
    }

    /**
     * Parse command tasks from the http response
     *
     * @param httpResponse the http response
     * @return a list of command tasks
     * @throws IOException
     */
    private static List<ClusterCommand> parseCommandTasks(HttpResponse httpResponse)
            throws IOException {
        String response = "";
        InputStream content = httpResponse.getContent();
        if (content == null) {
            throw new IOException("null response");
        }
        response = StreamUtil.getStringFromStream(content);
        // Parse the response
        try {
            JSONObject jsonResponse = new JSONObject(response);
            if (jsonResponse.has("tasks")) {
                // Convert the JSON commands to ClusterCommand objects
                JSONArray jsonCommands = jsonResponse.getJSONArray("tasks");
                List<ClusterCommand> commandTasks = new ArrayList<>(jsonCommands.length());
                for (int i = 0; i < jsonCommands.length(); i++) {
                    JSONObject jsonCommand = jsonCommands.getJSONObject(i);
                    commandTasks.add(ClusterCommand.fromJson(jsonCommand));
                }
                return commandTasks;
            } else {
                // No work to be done
                return Collections.<ClusterCommand>emptyList();
            }
        } catch (JSONException e) {
            // May be a server-side issue. Log a warning and we'll try again later.
            CLog.w("Failed to parse response from server: %s", response);
            return Collections.<ClusterCommand>emptyList();
        }
    }

    /**
     * Helper method to convert a list of {@link IClusterEvent}s into a json format that TFC can
     * understand.
     *
     * @param events The list of events to convert
     * @param key The key string to use to identify the list of events
     */
    private static JSONObject buildPostData(List<? extends IClusterEvent> events, String key)
            throws JSONException {

        JSONArray array = new JSONArray();
        for (IClusterEvent event : events) {
            array.put(event.toJSON());
        }
        JSONObject postData = new JSONObject();
        postData.put(key, array);
        return postData;
    }

    /** {@inheritDoc} */
    @Override
    public IClusterEventUploader<ClusterCommandEvent> getCommandEventUploader() {
        if (mCommandEventUploader == null) {
            mCommandEventUploader = new ClusterCommandEventUploader();
        }
        return mCommandEventUploader;
    }

    /** {@inheritDoc} */
    @Override
    public IClusterEventUploader<ClusterHostEvent> getHostEventUploader() {
        if (mHostEventUploader == null) {
            mHostEventUploader = new ClusterHostEventUploader();
        }
        return mHostEventUploader;
    }

    /**
     * Get the shared {@link IRestApiHelper} instance.
     *
     * <p>Exposed for testing.
     *
     * @return the shared {@link IRestApiHelper} instance.
     */
    @VisibleForTesting
    IRestApiHelper getApiHelper() {
        if (mApiHelper == null) {
            HttpTransport transport = null;
            transport = new NetHttpTransport();
            HttpRequestFactory requestFactory = transport.createRequestFactory();
            String baseUrl = getClusterOptions().getServiceUrl();
            if (baseUrl == null) {
                baseUrl = DEFAULT_TFC_URL_BASE;
            }
            mApiHelper = new RestApiHelper(requestFactory, baseUrl);
        }
        return mApiHelper;
    }

    /** Get the {@link IClusterOptions} instance used to store cluster-related settings. */
    IClusterOptions getClusterOptions() {
        if (mClusterOptions == null) {
            mClusterOptions =
                    (IClusterOptions)
                            GlobalConfiguration.getInstance()
                                    .getConfigurationObject(ClusterOptions.TYPE_NAME);
            if (mClusterOptions == null) {
                throw new IllegalStateException(
                        "cluster_options not defined. You must add this "
                                + "object to your global config. See google/atp/cluster.xml.");
            }
        }
        return mClusterOptions;
    }

    IHostOptions getHostOptions() {
        if (mHostOptions == null) {
            mHostOptions = GlobalConfiguration.getInstance().getHostOptions();
        }
        return mHostOptions;
    }
}
