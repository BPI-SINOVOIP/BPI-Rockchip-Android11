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

import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.cloud.GceRemoteCmdFormatter.ScpMode;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Utility class to handle file from a remote instance */
public class RemoteFileUtil {

    /**
     * Fetch a remote file in the container instance.
     *
     * @param remoteInstance The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeout in millisecond for the fetch to complete
     * @param remoteFilePath The remote path where to find the file.
     * @return The pulled filed if successful, null otherwise
     */
    public static File fetchRemoteFile(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            IRunUtil runUtil,
            long timeout,
            String remoteFilePath) {
        String fileName = new File(remoteFilePath).getName();
        File localFile = null;
        try {
            localFile =
                    FileUtil.createTempFile(
                            FileUtil.getBaseName(fileName) + "_", FileUtil.getExtension(fileName));
            if (fetchRemoteFile(
                    remoteInstance, options, runUtil, timeout, remoteFilePath, localFile)) {
                return localFile;
            }
        } catch (IOException e) {
            CLog.e(e);
        }
        FileUtil.deleteFile(localFile);
        return null;
    }

    /**
     * Fetch a remote file in the device or container instance.
     *
     * @param remoteInstance The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeout in millisecond for the fetch to complete
     * @param remoteFilePath The remote path where to find the file.
     * @param localFile The local {@link File} where the remote file will be pulled
     * @return True if successful, False otherwise
     */
    public static boolean fetchRemoteFile(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            IRunUtil runUtil,
            long timeout,
            String remoteFilePath,
            File localFile) {
        return internalScpExec(
                remoteInstance,
                options,
                null,
                runUtil,
                timeout,
                remoteFilePath,
                localFile,
                ScpMode.PULL);
    }

    /**
     * Fetch a remote directory from the remote host.
     *
     * @param remoteInstance The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeout in millisecond for the fetch to complete
     * @param remoteDirPath The remote path where to find the directory.
     * @param localDir The local directory where to put the pulled files.
     * @return True if successful, False otherwise
     */
    public static boolean fetchRemoteDir(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            IRunUtil runUtil,
            long timeout,
            String remoteDirPath,
            File localDir) {
        return internalScpExec(
                remoteInstance,
                options,
                Arrays.asList("-r"),
                runUtil,
                timeout,
                remoteDirPath,
                localDir,
                ScpMode.PULL);
    }

    /**
     * Fetch a remote directory from the remote host.
     *
     * @param remoteInstance The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeout in millisecond for the fetch to complete
     * @param remoteDirPath The remote path where to find the directory.
     * @return The pulled directory {@link File} if successful, null otherwise
     */
    public static File fetchRemoteDir(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            IRunUtil runUtil,
            long timeout,
            String remoteDirPath) {
        String dirName = new File(remoteDirPath).getName();
        File localFile = null;
        try {
            localFile = FileUtil.createTempDir(dirName);
            if (internalScpExec(
                    remoteInstance,
                    options,
                    Arrays.asList("-r"),
                    runUtil,
                    timeout,
                    remoteDirPath,
                    localFile,
                    ScpMode.PULL)) {
                return localFile;
            }
        } catch (IOException e) {
            CLog.e(e);
        }
        FileUtil.deleteFile(localFile);
        return null;
    }

    /**
     * Push a {@link File} from the local host to the remote instance
     *
     * @param remoteInstance The {@link GceAvdInfo} that describe the device.
     * @param options a {@link TestDeviceOptions} describing the device options to be used for the
     *     GCE device.
     * @param scpArgs extra args to be passed to the scp command
     * @param runUtil a {@link IRunUtil} to execute commands.
     * @param timeout in millisecond for the fetch to complete
     * @param remoteFilePath The remote path where to find the file.
     * @param localFile The local {@link File} where the remote file will be pulled
     * @return True if successful, False otherwise
     */
    public static boolean pushFileToRemote(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            List<String> scpArgs,
            IRunUtil runUtil,
            long timeout,
            String remoteFilePath,
            File localFile) {
        return internalScpExec(
                remoteInstance,
                options,
                scpArgs,
                runUtil,
                timeout,
                remoteFilePath,
                localFile,
                ScpMode.PUSH);
    }

    private static boolean internalScpExec(
            GceAvdInfo remoteInstance,
            TestDeviceOptions options,
            List<String> scpArgs,
            IRunUtil runUtil,
            long timeout,
            String remoteFilePath,
            File localFile,
            ScpMode mode) {
        List<String> scpCmd =
                GceRemoteCmdFormatter.getScpCommand(
                        options.getSshPrivateKeyPath(),
                        scpArgs,
                        options.getInstanceUser(),
                        remoteInstance.hostAndPort().getHost(),
                        remoteFilePath,
                        localFile.getAbsolutePath(),
                        mode);
        CommandResult resScp = runUtil.runTimedCmd(timeout, scpCmd.toArray(new String[0]));
        if (!CommandStatus.SUCCESS.equals(resScp.getStatus())) {
            StringBuilder builder = new StringBuilder();
            builder.append("Issue when ");
            if (ScpMode.PULL.equals(mode)) {
                builder.append("pulling ");
            } else {
                builder.append("pushing ");
            }
            builder.append(String.format("file, status: %s", resScp.getStatus()));
            CLog.e(builder.toString());
            CLog.e("%s", resScp.getStderr());
            return false;
        } else {
            return true;
        }
    }
}
