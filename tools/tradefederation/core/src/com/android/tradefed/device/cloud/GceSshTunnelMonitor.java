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
package com.android.tradefed.device.cloud;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.RemoteAndroidDevice;
import com.android.tradefed.device.RemoteAvdIDevice;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.TestDeviceOptions.InstanceType;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.net.HostAndPort;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Thread Monitor for the Gce ssh tunnel. */
public class GceSshTunnelMonitor extends Thread {

    public static final String VIRTUAL_DEVICE_SERIAL = "virtual-device-serial";

    // stop/start adbd has longer retries in order to support possible longer reboot time.
    private static final long ADBD_RETRY_INTERVAL_MS = 15000;
    private static final int ADBD_MAX_RETRIES = 10;
    private static final long DEFAULT_LONG_CMD_TIMEOUT = 60 * 1000;
    private static final long DEFAULT_SHORT_CMD_TIMEOUT = 20 * 1000;
    private static final int WAIT_FOR_FIRST_CONNECT = 10 * 1000;
    private static final long WAIT_AFTER_REBOOT = 60 * 1000;

    // Format string for local hostname.
    private static final String DEFAULT_LOCAL_HOST = "127.0.0.1:%d";

    // Format string for non-interactive SSH tunneling parameter;params in
    // order:local port, remote port
    private static final String TUNNEL_PARAM = "-L%d:127.0.0.1:%d";

    private boolean mQuit = false;
    private boolean mAdbRebootCalled = false;
    private ITestDevice mDevice;
    private TestDeviceOptions mDeviceOptions;

    private HostAndPort mGceHostAndPort;
    private HostAndPort mLocalHostAndPort;
    private Process mSshTunnelProcess;
    private IBuildInfo mBuildInfo;
    private int mLastUsedPort = 0;

    private Exception mLastException = null;
    private boolean mSshChecked = false;

    private File mSshTunnelLogs = null;
    private File mAdbConnectionLogs = null;

    /**
     * Constructor
     *
     * @param device {@link ITestDevice} the TF device to associate the remote GCE AVD with.
     * @param gce {@link HostAndPort} of the remote GCE AVD.
     */
    public GceSshTunnelMonitor(
            ITestDevice device,
            IBuildInfo buildInfo,
            HostAndPort gce,
            TestDeviceOptions deviceOptions) {
        super(
                String.format(
                        "GceSshTunnelMonitor-%s-%s-%s",
                        buildInfo.getBuildBranch(),
                        buildInfo.getBuildFlavor(),
                        buildInfo.getBuildId()));
        setDaemon(true);
        mDevice = device;
        mGceHostAndPort = gce;
        mBuildInfo = buildInfo;
        mDeviceOptions = deviceOptions;
        mLastException = null;
        mQuit = false;
        mSshTunnelLogs = null;
    }

    /** Returns the instance of {@link IRunUtil}. */
    @VisibleForTesting
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    /** Returns the {@link TestDeviceOptions} the tunnel was initialized with. */
    TestDeviceOptions getTestDeviceOptions() {
        return mDeviceOptions;
    }

    /** Returns True if the {@link GceSshTunnelMonitor} is still alive, false otherwise. */
    public boolean isTunnelAlive() {
        if (mSshTunnelProcess != null) {
            return mSshTunnelProcess.isAlive();
        }
        return false;
    }

    /** Set True when an adb reboot is about to be called to make sure the monitor expect it. */
    public void isAdbRebootCalled(boolean isCalled) {
        mAdbRebootCalled = isCalled;
    }

    /** Terminate the tunnel monitor */
    public void shutdown() {
        mQuit = true;
        closeConnection();
        getRunUtil().allowInterrupt(true);
        getRunUtil().interrupt(this, "shutting down the monitor thread.");
        interrupt();
    }

    /** Waits for this monitor to finish, as in {@link Thread#join()}. */
    public void joinMonitor() throws InterruptedException {
        // We use join with a timeout to ensure tearDown is never blocked forever.
        super.join(DEFAULT_LONG_CMD_TIMEOUT);
    }

    /** Close all the connections from the monitor (adb and ssh tunnel). */
    public void closeConnection() {
        // shutdown adb connection first, if we reached where there could be a connection
        CLog.d("closeConnection is triggered.");
        if (mLocalHostAndPort != null) {
            if (!((RemoteAndroidDevice) mDevice)
                    .adbTcpDisconnect(
                            mLocalHostAndPort.getHost(),
                            Integer.toString(mLocalHostAndPort.getPort()))) {
                CLog.d("Failed to disconnect from local host %s", mLocalHostAndPort.toString());
            }
        }
        if (mSshTunnelProcess != null) {
            mSshTunnelProcess.destroy();
            try {
                boolean res =
                        mSshTunnelProcess.waitFor(DEFAULT_SHORT_CMD_TIMEOUT, TimeUnit.MILLISECONDS);
                if (!res) {
                    CLog.e("SSH tunnel may not have properly terminated.");
                }
            } catch (InterruptedException e) {
                CLog.e("SSH tunnel interrupted during shutdown: %s", e.getMessage());
            }
        }
    }

    /** Check that the ssh key file is readable so that our commands can go through. */
    @VisibleForTesting
    void checkSshKey() {
        if (!getTestDeviceOptions().getSshPrivateKeyPath().canRead()) {
            if (mSshChecked) {
                // The key was available before and for some reason is not now.
                getRunUtil().sleep(DEFAULT_SHORT_CMD_TIMEOUT);
                if (getTestDeviceOptions().getSshPrivateKeyPath().canRead()) {
                    CLog.w("ssh key was not available for a temporary period of time.");
                    // TODO: Add metric logging
                    return;
                }
            }
            throw new RuntimeException(
                    String.format(
                            "Ssh private key is unavailable %s",
                            getTestDeviceOptions().getSshPrivateKeyPath().getAbsolutePath()));
        }
        mSshChecked = true;
    }

    /** Perform some initial setup on the GCE AVD. */
    void initGce() {
        checkSshKey();

        // HACK: stop/start adbd first, otherwise device seems to be in offline mode.
        if (InstanceType.GCE.equals(mDeviceOptions.getInstanceType())
                || InstanceType.REMOTE_AVD.equals(mDeviceOptions.getInstanceType())) {
            List<String> stopAdb =
                    GceRemoteCmdFormatter.getSshCommand(
                            getTestDeviceOptions().getSshPrivateKeyPath(),
                            null,
                            getTestDeviceOptions().getInstanceUser(),
                            mGceHostAndPort.getHost(),
                            "stop",
                            "adbd");
            CLog.d("Running %s", stopAdb);
            CommandResult result =
                    getRunUtil()
                            .runTimedCmdSilentlyRetry(
                                    DEFAULT_SHORT_CMD_TIMEOUT,
                                    ADBD_RETRY_INTERVAL_MS,
                                    ADBD_MAX_RETRIES,
                                    stopAdb.toArray(new String[0]));
            if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                CLog.w("failed to stop adbd %s", result.getStderr());
                throw new RuntimeException("failed to stop adbd");
            }
            if (mQuit) {
                throw new RuntimeException("Shutdown has been requested. stopping init.");
            }
            List<String> startAdb =
                    GceRemoteCmdFormatter.getSshCommand(
                            getTestDeviceOptions().getSshPrivateKeyPath(),
                            null,
                            getTestDeviceOptions().getInstanceUser(),
                            mGceHostAndPort.getHost(),
                            "start",
                            "adbd");
            result =
                    getRunUtil()
                            .runTimedCmdSilentlyRetry(
                                    DEFAULT_SHORT_CMD_TIMEOUT,
                                    ADBD_RETRY_INTERVAL_MS,
                                    ADBD_MAX_RETRIES,
                                    startAdb.toArray(new String[0]));
            CLog.d("Running %s", startAdb);
            if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                CLog.w("failed to start adbd", result);
                throw new RuntimeException("failed to start adbd");
            }
        }
    }

    @Override
    public void run() {
        while (!mQuit) {
            try {
                if (mQuit) {
                    CLog.d("Final shutdown of the tunnel has been requested. terminating.");
                    return;
                }
                initGce();
            } catch (RuntimeException e) {
                mLastException = e;
                CLog.d("Failed to init remote GCE. Terminating due to:");
                CLog.e(e);
                return;
            }
            if (mAdbConnectionLogs == null) {
                try {
                    mAdbConnectionLogs = FileUtil.createTempFile("adb-connection", ".txt");
                } catch (IOException e) {
                    FileUtil.deleteFile(mAdbConnectionLogs);
                    CLog.e(e);
                }
            }
            if (mAdbConnectionLogs != null) {
                if (mDevice instanceof RemoteAndroidDevice) {
                    ((RemoteAndroidDevice) mDevice).setAdbLogFile(mAdbConnectionLogs);
                }
            }

            mSshTunnelProcess =
                    createSshTunnel(
                            mDevice,
                            mGceHostAndPort.getHost(),
                            mGceHostAndPort.getPortOrDefault(TestDeviceOptions.DEFAULT_ADB_PORT));
            if (mSshTunnelProcess == null) {
                CLog.e("Failed creating the ssh tunnel to GCE.");
                return;
            }
            // Device serial should contain tunnel host and port number.
            getRunUtil().sleep(WAIT_FOR_FIRST_CONNECT);
            // Checking if it is actually running.
            if (isTunnelAlive()) {
                mLocalHostAndPort = HostAndPort.fromString(mDevice.getSerialNumber());
                if (!((RemoteAndroidDevice) mDevice)
                        .adbTcpConnect(
                                mLocalHostAndPort.getHost(),
                                Integer.toString(mLocalHostAndPort.getPort()))) {
                    CLog.e("Adb connect failed, re-init GCE connection.");
                    closeConnection();
                    continue;
                }
                try {
                    mSshTunnelProcess.waitFor();
                } catch (InterruptedException e) {
                    CLog.d("SSH tunnel terminated %s", e.getMessage());
                }
                CLog.d("Reached end of loop, tunnel is going to re-init.");
                if (mAdbRebootCalled) {
                    mAdbRebootCalled = false;
                    CLog.d(
                            "Tunnel reached end of loop due to adbReboot, "
                                    + "waiting a little for device to come online");
                    getRunUtil().sleep(WAIT_AFTER_REBOOT);
                }
            } else {
                CLog.e("SSH Tunnel is not alive after starting it. It must have returned.");
            }
        }
    }

    /**
     * Create an ssh tunnel to a given remote host and assign the endpoint to a device.
     *
     * @param device {@link ITestDevice} to which we want to associate this ssh tunnel.
     * @param remoteHost the hostname/ip of the remote tcp ip Android device.
     * @param remotePort the port of the remote tcp ip device.
     * @return {@link Process} of the ssh command.
     */
    @VisibleForTesting
    Process createSshTunnel(ITestDevice device, String remoteHost, int remotePort) {
        try {
            ServerSocket s = null;
            try {
                s = new ServerSocket(mLastUsedPort);
            } catch (SocketException se) {
                // if we fail to allocate the previous port, we take a random available one.
                s = new ServerSocket(0);
                CLog.w(
                        "Our previous port: %s was already in use, switching to: %s",
                        mLastUsedPort, s.getLocalPort());
            }
            // even after being closed, socket may remain in TIME_WAIT state
            // reuse address allows to connect to it even in this state.
            s.setReuseAddress(true);
            mLastUsedPort = s.getLocalPort();
            String serial = String.format(DEFAULT_LOCAL_HOST, mLastUsedPort);
            if (mQuit) {
                CLog.d("Shutdown has been requested. Skipping creation of the ssh process");
                StreamUtil.close(s);
                return null;
            }
            CLog.d("Setting device %s serial to %s", device.getSerialNumber(), serial);
            ((IManagedTestDevice) device).setIDevice(new RemoteAvdIDevice(serial));
            ((IManagedTestDevice) device).setFastbootEnabled(false);
            // Do not set setDeviceSerial to keep track of it consistently with the placeholder
            // serial
            mBuildInfo.addBuildAttribute(VIRTUAL_DEVICE_SERIAL, serial);
            StreamUtil.close(s);
            // Note there is a race condition here. between when we close
            // the server socket and when we try to connect to the tunnel.
            List<String> tunnelParam = new ArrayList<>();
            tunnelParam.add(String.format(TUNNEL_PARAM, mLastUsedPort, remotePort));
            tunnelParam.add("-N");
            List<String> sshTunnel =
                    GceRemoteCmdFormatter.getSshCommand(
                            getTestDeviceOptions().getSshPrivateKeyPath(),
                            tunnelParam,
                            getTestDeviceOptions().getInstanceUser(),
                            remoteHost,
                            "" /* no command */);
            if (mSshTunnelLogs == null || !mSshTunnelLogs.exists()) {
                mSshTunnelLogs = FileUtil.createTempFile("ssh-tunnel-logs", ".txt");
                FileUtil.writeToFile("=== Beginning ===\n", mSshTunnelLogs);
            }
            Process p =
                    getRunUtil()
                            .runCmdInBackground(
                                    sshTunnel, new FileOutputStream(mSshTunnelLogs, true));
            return p;
        } catch (IOException e) {
            CLog.d("Failed to connect to remote GCE using ssh tunnel %s", e.getMessage());
        }
        return null;
    }

    /** Returns the last exception captured in the ssh tunnel thread. */
    public Exception getLastException() {
        return mLastException;
    }

    /** Log all the interesting log files generated from the ssh tunnel. */
    public void logSshTunnelLogs(ITestLogger logger) {
        if (mDevice instanceof RemoteAndroidDevice) {
            ((RemoteAndroidDevice) mDevice).setAdbLogFile(null);
        }
        if (mSshTunnelLogs != null) {
            try (InputStreamSource sshBridge = new FileInputStreamSource(mSshTunnelLogs, true)) {
                logger.testLog("ssh-bridge-logs", LogDataType.TEXT, sshBridge);
            }
        }
        if (mAdbConnectionLogs != null) {
            try (InputStreamSource adbLogs = new FileInputStreamSource(mAdbConnectionLogs, true)) {
                logger.testLog("adb-connect-logs", LogDataType.TEXT, adbLogs);
            }
        }
    }
}
