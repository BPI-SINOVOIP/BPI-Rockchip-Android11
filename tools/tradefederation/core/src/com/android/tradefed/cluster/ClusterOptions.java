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
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.util.MultiMap;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/*
 * A {@link IClusterOptions} implementation which contains cluster-related options.
 */
@OptionClass(alias = "cluster", global_namespace = false)
public class ClusterOptions implements IClusterOptions {

    /**
     * The unique configuration object type name. Used to retrieve the singleton instance from the
     * {@link GlobalConfiguration}.
     *
     * @see IConfiguration#getConfigurationObject(String)
     */
    public static final String TYPE_NAME = "cluster_options";

    @Option(name = "service-url", description = "the base url of the tradefed cluster REST API")
    public String mServiceUrl = null;

    // TODO: The "service-account-keyfile" option should be HostOption (AOSP HostOptions.java).
    @Option(
            name = "service-account-keyfile",
            description =
                    "The service account json key file. "
                            + "This is used by tradefed test scheduler (e.g. Tradefed Cluster) to "
                            + "authenticate the tradefed host. "
                            + "See google doc for the definition and how service account key works. "
                            + "https://cloud.google.com/iam/docs/service-accounts ")
    private File mSchedulerServiceAccountKeyfile = null;

    @Option(name = "cluster", description = "the cluster id for this TF instance", mandatory = true)
    public String mClusterId = null;

    @Option(
            name = "next-cluster",
            description =
                    "seconadary clusters for this TF instance to run commands from. If "
                            + "this option is set, TF will try to lease commands from these clusters in "
                            + "the order they are specified if it still has available devices after "
                            + "leasing commands from the primary cluster.")
    public List<String> mNextClusterIds = new ArrayList<>();

    @Option(name = "run-target-format", description = "the format for labelling run targets.")
    private String mRunTargetFormat = null;

    @Option(name = "disable-device-monitor", description = "disable Cluster device reporting")
    private boolean mIsDeviceMonitorDisabled = false;

    @Option(
            name = "device-monitor-interval",
            isTimeVal = true,
            description = "the time interval between each device snapshot")
    private long mDeviceMonitorSnapshotInterval = 60 * 1000;

    @Option(
            name = "device-group",
            description =
                    "A multi-map from device group to device serials."
                            + " The key is a device group name and value is device serial.")
    private MultiMap<String, String> mDeviceGroup = new MultiMap<String, String>();

    @Option(
            name = "device-tag",
            description =
                    "A map for tagging device serials; each device may "
                            + "have one tag. This can be used for reporting in run-target")
    private Map<String, String> mDeviceTag = new HashMap<>();

    @Option(
            name = "check-flashing-permits-on-lease",
            description = "Check available flashing permits when leasing tasks")
    private boolean mCheckFlashingPermitsOnLease = true;

    @Option(
            name = "invocation-heartbeat-interval",
            isTimeVal = true,
            description = "The time interval between invocation heartbeats")
    private long mInvocationHeartbeatInterval = 5 * 60 * 1000;

    @Option(name = "upload-invocation-status", description = "Upload invocation status to TFC")
    private Boolean mShouldUploadInvocationStatus = false;

    @Option(
            name = "check-command-state",
            description = "Check cluster command state to detect canceled invocations")
    private boolean mCheckCommandState = false;

    @Option(name = "connect-timeout", description = "HTTP connect timeout.", isTimeVal = true)
    private int mConnectTimeout = 60000;

    @Option(name = "read-timeout", description = "HTTP read timeout.", isTimeVal = true)
    private int mReadTimeout = 60000;

    @Option(name = "label", description = "Labels to describe the host.")
    private List<String> mLabels = new ArrayList<>();

    @Option(name = "lab-name", description = "The name of the lab the host belong to.")
    private String mLabName;

    @Option(
            name = "collect-early-test-summary",
            description = "Collect early test summary from ITestSummaryListener to scheduler.")
    private boolean mCollectEarlyTestSummary = false;

    /** {@inheritDoc} */
    @Override
    public String getServiceUrl() {
        return mServiceUrl;
    }

    /** {@inheritDoc} */
    @Override
    public String getClusterId() {
        return mClusterId;
    }

    /** {@inheritDoc} */
    @Override
    public List<String> getNextClusterIds() {
        return mNextClusterIds;
    }

    /** {@inheritDoc} */
    @Override
    public MultiMap<String, String> getDeviceGroup() {
        return mDeviceGroup;
    }

    /** {@inheritDoc} */
    @Override
    public Map<String, String> getDeviceTag() {
        return mDeviceTag;
    }

    /** {@inheritDoc} */
    @Override
    public boolean checkFlashingPermitsOnLease() {
        return mCheckFlashingPermitsOnLease;
    }

    /** {@inheritDoc} */
    @Override
    public String getRunTargetFormat() {
        return mRunTargetFormat;
    }

    /** {@inheritDoc} */
    @Override
    public boolean isDeviceMonitorDisabled() {
        return mIsDeviceMonitorDisabled;
    }

    /** {@inheritDoc} */
    @Override
    public long getDeviceMonitorSnapshotInterval() {
        return mDeviceMonitorSnapshotInterval;
    }

    /**
     * Set the base url of the tradefed cluster REST API.
     *
     * <p>Exposed for testing.
     */
    void setServiceUrl(String url) {
        mServiceUrl = url;
    }

    /**
     * Set the cluster id for this TF instance.
     *
     * <p>Exposed for testing.
     */
    void setClusterId(String id) {
        mClusterId = id;
    }

    /**
     * Set the format for labelling run targets.
     *
     * <p>Exposed for testing.
     */
    void setRunTargetFormat(String format) {
        mRunTargetFormat = format;
    }

    /**
     * Set whether Cluster device reporting is disabled.
     *
     * <p>Exposed for testing.
     */
    void setDeviceMonitorDisabled(boolean disabled) {
        mIsDeviceMonitorDisabled = disabled;
    }

    /**
     * Set the time interval between each device snapshot in ms.
     *
     * <p>Exposed for testing.
     */
    void setDeviceMonitorSnapshotInterval(long interval) {
        mDeviceMonitorSnapshotInterval = interval;
    }

    /**
     * Set whether the scheduler should check if there are available flashing permits.
     *
     * <p>Exposed for testing.
     */
    void setCheckFlashingPermitsLease(boolean checkFlashingPermitsLease) {
        mCheckFlashingPermitsOnLease = checkFlashingPermitsLease;
    }

    /**
     * Set whether the scheduler should collect early test summary.
     *
     * <p>Exposed for testing.
     */
    void setCollectEarlyTestSummary(boolean collectEarlyTestSummary) {
        mCollectEarlyTestSummary = collectEarlyTestSummary;
    }

    /** {@inheritDoc} */
    @Override
    public long getInvocationHeartbeatInterval() {
        return mInvocationHeartbeatInterval;
    }

    /** {@inheritDoc} */
    @Override
    public Boolean shouldUploadInvocationStatus() {
        return mShouldUploadInvocationStatus;
    }

    /** Set the service account key file. */
    @VisibleForTesting
    void setSchedulerServiceAccountKeyfile(File keyFile) {
        mSchedulerServiceAccountKeyfile = keyFile;
    }

    /** {@inheritDoc} */
    @Override
    public File getSchedulerServiceAccountKeyfile() {
        return mSchedulerServiceAccountKeyfile;
    }

    /** {@inheritDoc} */
    @Override
    public String getSchedulerServiceUrl() {
        return mServiceUrl;
    }

    /** {@inheritDoc} */
    @Override
    public int getConnectTimeout() {
        return mConnectTimeout;
    }

    /** {@inheritDoc} */
    @Override
    public int getReadTimeout() {
        return mReadTimeout;
    }

    @Override
    public boolean checkCommandState() {
        return mCheckCommandState;
    }

    @VisibleForTesting
    void setCheckCommandState(boolean checkCommandState) {
        mCheckCommandState = checkCommandState;
    }

    /** {@inheritDoc} */
    @Override
    public List<String> getLabels() {
        return new ArrayList<>(mLabels);
    }

    /** {@inheritDoc} */
    @Override
    public String getLabName() {
        return mLabName;
    }

    /** {@inheritDoc} */
    @Override
    public boolean shouldCollectEarlyTestSummary() {
        return mCollectEarlyTestSummary;
    }
}
