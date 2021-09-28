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

import com.android.tradefed.util.MultiMap;
import java.io.File;
import java.util.List;
import java.util.Map;

/** An interface for getting cluster-related options. */
public interface IClusterOptions {

    /** Get the base url of the tradefed cluster REST API. */
    public String getServiceUrl();

    /** Get the cluster id for this TF instance. */
    public String getClusterId();

    /** Get the secondary cluster ids for this TF instance. */
    public List<String> getNextClusterIds();

    /** Get the device group to device mapping. */
    public MultiMap<String, String> getDeviceGroup();

    /** Get the device serial to tag mapping. */
    public Map<String, String> getDeviceTag();

    /** Check if it should check for available flashing permits before leasing. */
    boolean checkFlashingPermitsOnLease();

    /** Get the format for labelling run targets. */
    public String getRunTargetFormat();

    /** Returns whether Cluster device reporting is disabled. */
    public boolean isDeviceMonitorDisabled();

    /** Get the time interval between each device snapshot in ms. */
    public long getDeviceMonitorSnapshotInterval();

    /** Returns whether TF should upload invocation status. */
    public Boolean shouldUploadInvocationStatus();

    /** Get the time interval between invocation heartbeats in ms. */
    public long getInvocationHeartbeatInterval();

    /** Get http connect timeout. */
    public int getConnectTimeout();

    /** Get http read timeout. */
    public int getReadTimeout();

    /** Get the tradefed test scheduler service account key file. */
    public File getSchedulerServiceAccountKeyfile();

    /** Get the tradefed test scheduler service URL. */
    public String getSchedulerServiceUrl();

    /** Whether the command state (on the TF cluster) should be checked during heartbeat. */
    public boolean checkCommandState();

    /** Get the name of the lab the host belong to. */
    public String getLabName();

    /** Get labels for the host. */
    public List<String> getLabels();

    /** Returns whether scheduler should collect early test summary. */
    public boolean shouldCollectEarlyTestSummary();
}
