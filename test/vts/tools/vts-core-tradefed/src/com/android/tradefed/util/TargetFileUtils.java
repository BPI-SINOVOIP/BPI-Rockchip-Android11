/*
 * Copyright (C) 2020 The Android Open Source Project
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

/* Utils for target file.*/
package com.android.tradefed.util;

import com.android.ddmlib.Log;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class TargetFileUtils {
    // 3 permission groups: owner, group, all users
    private static final int PERMISSION_GROUPS = 3;

    public enum FilePermission {
        EXECUTE(1),
        READ(4),
        WRITE(2);

        private int mPermissionNum;

        FilePermission(int permissionNum) {
            mPermissionNum = permissionNum;
        }

        public int getPermissionNum() {
            return mPermissionNum;
        }
    }

    /**
     * Determines if the permission bits grant the specify permission to any group.
     *
     * @param permission The specify permissions.
     * @param permissionBits The octal permissions string (e.g. 741).
     * @return True if any owner/group/global has the specify permission.
     */
    public static boolean hasPermission(FilePermission permission, String permissionBits) {
        for (int i = 0; i < PERMISSION_GROUPS; i++) {
            if (hasPermission(permissionBits, i, permission)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Read the file permission bits of a path.
     *
     * @param filepath Path to a file or directory.
     * @param device The test device.
     * @return  Octal permission bits for the path.
     */
    public static String getPermission(String filepath, ITestDevice device)
            throws DeviceNotAvailableException {
        CommandResult commandResult = device.executeShellV2Command("stat -c %a " + filepath);
        if (!CommandStatus.SUCCESS.equals(commandResult.getStatus())) {
            CLog.logAndDisplay(Log.LogLevel.ERROR, "Get permission error:\nstdout%s\nstderr",
                    commandResult.getStdout(), commandResult.getStderr());
            return "";
        }
        return commandResult.getStdout().trim();
    }

    /**
     * Determines if the permission bits grant a permission to a group.
     *
     * @param permissionBits The octal permissions string (e.g. 741).
     * @param groupIndex The index of the group into the permissions string.
     *                         (e.g. 0 is owner group). If set to -1, then all groups are
     *                         checked.
     * @param permission The value of the permission.
     * @return  True if the group(s) has read permission.
     */
    private static boolean hasPermission(
            String permissionBits, int groupIndex, FilePermission permission) {
        if (groupIndex >= PERMISSION_GROUPS) {
            throw new RuntimeException(String.format("Invalid group: %s", groupIndex));
        }
        if (permissionBits.length() != PERMISSION_GROUPS) {
            throw new RuntimeException(
                    String.format("Invalid permission bits: %s", permissionBits.length() + ""));
        }

        // Define the start/end group index
        int start = groupIndex;
        int end = groupIndex + 1;
        if (groupIndex < 0) {
            start = 0;
            end = PERMISSION_GROUPS;
        }

        for (int i = start; i < end; i++) {
            try {
                int perm = Integer.valueOf(permissionBits.charAt(i) + "");
                if (perm > 7) {
                    throw new RuntimeException(String.format("Invalid permission bit: %d", perm));
                }
                if ((perm & permission.getPermissionNum()) == 0) {
                    // Return false if any group lacks the permission
                    return false;
                }
            } catch (NumberFormatException e) {
                throw new RuntimeException(String.format(
                        "Permission bits \"%s\" format error, should be three digital number "
                                + "(e.q. 741).",
                        permissionBits));
            }
        }
        // Return true if no group lacks the permission
        return true;
    }

    /**
     * Helper method which executes a adb shell find command and returns the results as an {@link
     * ArrayList<String>}.
     *
     * @param path The path to search on device.
     * @param namePattern The file name pattern.
     * @param options A {@link List} of {@link String} for other options pass to find.
     * @param device The test device.
     * @return The result in {@link ArrayList<String>}.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     */
    public static ArrayList<String> findFile(String path, String namePattern, List<String> options,
            ITestDevice device) throws DeviceNotAvailableException {
        ArrayList<String> findedFiles = new ArrayList<>();
        String command = String.format("find %s -name \"%s\"", path, namePattern);
        if (options != null) {
            command += " " + String.join(" ", options);
        }
        CLog.d("command: %s", command);
        CommandResult result = device.executeShellV2Command(command);
        if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
            CLog.e("Find command: '%s' failed, returned:\nstdout:%s\nstderr:%s", command,
                    result.getStdout(), result.getStderr());
            return findedFiles;
        }
        findedFiles = new ArrayList<>(Arrays.asList(result.getStdout().split("\n")));
        findedFiles.removeIf(s -> s.contentEquals(""));
        return findedFiles;
    }

    /**
     * Check if the permission for a given path is readonly.
     *
     * @param filepath Path to a file or directory.
     * @param device The test device.
     * @return true if the path is readonly, false otherwise.
     */
    public static boolean isReadOnly(String filepath, ITestDevice device)
            throws DeviceNotAvailableException {
        String permissionBits = getPermission(filepath, device);
        return (hasPermission(FilePermission.READ, permissionBits)
                && !hasPermission(FilePermission.WRITE, permissionBits)
                && !hasPermission(FilePermission.EXECUTE, permissionBits));
    }

    /**
     * Check if the permission for a given path is readwrite.
     *
     * @param filepath Path to a file or directory.
     * @param device The test device.
     * @return true if the path is readwrite, false otherwise.
     */
    public static boolean isReadWriteOnly(String filepath, ITestDevice device)
            throws DeviceNotAvailableException {
        String permissionBits = getPermission(filepath, device);
        return (hasPermission(FilePermission.READ, permissionBits)
                && hasPermission(FilePermission.WRITE, permissionBits)
                && !hasPermission(FilePermission.EXECUTE, permissionBits));
    }
}
