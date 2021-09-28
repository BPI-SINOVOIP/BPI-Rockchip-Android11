/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.tradefed.device;

import com.android.ddmlib.IDevice;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.util.KeyguardControllerState;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Provides an reliable and slightly higher level API to a ddmlib {@link IDevice}.
 * <p/>
 * Retries device commands for a configurable amount, and provides a device recovery
 * interface for devices which are unresponsive.
 */
public interface ITestDevice extends INativeDevice {

    public enum RecoveryMode {
        /** don't attempt to recover device. */
        NONE,
        /** recover device to online state only */
        ONLINE,
        /**
         * Recover device into fully testable state - framework is up, and external storage is
         * mounted.
         */
        AVAILABLE
    }

    /**
     * A simple struct class to store information about a single mountpoint
     */
    public static class MountPointInfo {
        public String filesystem;
        public String mountpoint;
        public String type;
        public List<String> options;

        /** Simple constructor */
        public MountPointInfo() {}

        /**
         * Convenience constructor to set all members
         */
        public MountPointInfo(String filesystem, String mountpoint, String type,
                List<String> options) {
            this.filesystem = filesystem;
            this.mountpoint = mountpoint;
            this.type = type;
            this.options = options;
        }

        public MountPointInfo(String filesystem, String mountpoint, String type, String optString) {
            this(filesystem, mountpoint, type, splitMountOptions(optString));
        }

        public static List<String> splitMountOptions(String options) {
            List<String> list = Arrays.asList(options.split(","));
            return list;
        }

        @Override
        public String toString() {
            return String.format("%s %s %s %s", this.filesystem, this.mountpoint, this.type,
                    this.options);
        }
    }

    /** A simple struct class to store information about a single APEX */
    public static class ApexInfo {
        public final String name;
        public final long versionCode;
        public final String sourceDir;

        public ApexInfo(String name, long versionCode, String sourceDir) {
            this.name = name;
            this.versionCode = versionCode;
            this.sourceDir = sourceDir;
        }

        public ApexInfo(String name, long versionCode) {
            this(name, versionCode, "");
        }

        @Override
        public boolean equals(Object other) {
            if (other != null && other instanceof ApexInfo) {
                ApexInfo ai = (ApexInfo) other;
                return name.equals(ai.name) && versionCode == ai.versionCode;
            }
            return false;
        }

        @Override
        public int hashCode() {
            // no need to consider versionCode here.
            return name.hashCode();
        }

        @Override
        public String toString() {
            return "packageName: "
                    + name
                    + ", versionCode: "
                    + versionCode
                    + ", sourceDir: "
                    + sourceDir;
        }
    }

    /**
     * Install an Android package on device.
     *
     * @param packageFile the apk file to install
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     */
    public String installPackage(File packageFile, boolean reinstall, String... extraArgs)
            throws DeviceNotAvailableException;

    /**
     * Install an Android package on device.
     *
     * <p>Note: Only use cases that requires explicit control of granting runtime permission at
     * install time should call this function.
     *
     * @param packageFile the apk file to install
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param grantPermissions if all runtime permissions should be granted at install time
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public String installPackage(
            File packageFile, boolean reinstall, boolean grantPermissions, String... extraArgs)
            throws DeviceNotAvailableException;

    /**
     * Install an Android package on device for a given user.
     *
     * @param packageFile the apk file to install
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param userId the integer user id to install for.
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     */
    public String installPackageForUser(
            File packageFile, boolean reinstall, int userId, String... extraArgs)
            throws DeviceNotAvailableException;

    /**
     * Install an Android package on device for a given user.
     *
     * <p>Note: Only use cases that requires explicit control of granting runtime permission at
     * install time should call this function.
     *
     * @param packageFile the apk file to install
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param grantPermissions if all runtime permissions should be granted at install time
     * @param userId the integer user id to install for.
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public String installPackageForUser(
            File packageFile,
            boolean reinstall,
            boolean grantPermissions,
            int userId,
            String... extraArgs)
            throws DeviceNotAvailableException;

    /**
     * Uninstall an Android package from device.
     *
     * @param packageName the Android package to uninstall
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     */
    public String uninstallPackage(String packageName) throws DeviceNotAvailableException;

    /**
     * Install an Android application made of several APK files (one main and extra split packages).
     * See "https://developer.android.com/studio/build/configure-apk-splits" on how to split apk to
     * several files.
     *
     * @param packageFiles the local apk files
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installPackages(
            List<File> packageFiles, boolean reinstall, String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }

    /**
     * Install an Android application made of several APK files (one main and extra split packages)
     * that are sitting on the android device. See
     * "https://developer.android.com/studio/build/configure-apk-splits" on how to split apk to
     * several files.
     *
     * <p>Note: Only use cases that requires explicit control of granting runtime permission at
     * install time should call this function.
     *
     * @param packageFiles the remote apk file paths to install
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param grantPermissions if all runtime permissions should be granted at install time
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installPackages(
            List<File> packageFiles,
            boolean reinstall,
            boolean grantPermissions,
            String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }

    /**
     * Install an Android application made of several APK files (one main and extra split packages)
     * for a given user. See "https://developer.android.com/studio/build/configure-apk-splits" on
     * how to split apk to several files.
     *
     * @param packageFiles the local apk files
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param userId the integer user id to install for.
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installPackagesForUser(
            List<File> packageFiles, boolean reinstall, int userId, String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }

    /**
     * Install an Android application made of several APK files (one main and extra split packages)
     * for a given user. See "https://developer.android.com/studio/build/configure-apk-splits" on
     * how to split apk to several files.
     *
     * <p>Note: Only use cases that requires explicit control of granting runtime permission at
     * install time should call this function.
     *
     * @param packageFiles the local apk files
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param grantPermissions if all runtime permissions should be granted at install time
     * @param userId the integer user id to install for.
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installPackagesForUser(
            List<File> packageFiles,
            boolean reinstall,
            boolean grantPermissions,
            int userId,
            String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }

    /**
     * Install an Android application made of several APK files (one main and extra split packages)
     * that are sitting on the android device. See
     * "https://developer.android.com/studio/build/configure-apk-splits" on how to split apk to
     * several files.
     *
     * <p>Note: Only use cases that requires explicit control of granting runtime permission at
     * install time should call this function.
     *
     * @param remoteApkPaths the remote apk file paths
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param grantPermissions if all runtime permissions should be granted at install time
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installRemotePackages(
            List<String> remoteApkPaths,
            boolean reinstall,
            boolean grantPermissions,
            String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }

    /**
     * Install an Android application made of several APK files (one main and extra split packages)
     * that are sitting on the android device. See
     * "https://developer.android.com/studio/build/configure-apk-splits" on how to split apk to
     * several files.
     *
     * @param remoteApkPaths the remote apk file paths
     * @param reinstall <code>true</code> if a reinstall should be performed
     * @param extraArgs optional extra arguments to pass. See 'adb shell pm -h' for available
     *     options.
     * @return a {@link String} with an error code, or <code>null</code> if success.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     * @throws UnsupportedOperationException if runtime permission is not supported by the platform
     *     on device.
     */
    public default String installRemotePackages(
            List<String> remoteApkPaths, boolean reinstall, String... extraArgs)
            throws DeviceNotAvailableException {
        throw new UnsupportedOperationException("No support for Package Manager's features");
    }


    /**
     * Grabs a screenshot from the device.
     *
     * @return a {@link InputStreamSource} of the screenshot in png format, or <code>null</code> if
     *         the screenshot was not successful.
     * @throws DeviceNotAvailableException
     */
    public InputStreamSource getScreenshot() throws DeviceNotAvailableException;

    /**
     * Grabs a screenshot from the device.
     * Recommended to use getScreenshot(format) instead with JPEG encoding for smaller size
     * @param format supported PNG, JPEG
     * @return a {@link InputStreamSource} of the screenshot in format, or <code>null</code> if
     *         the screenshot was not successful.
     * @throws DeviceNotAvailableException
     */
    public InputStreamSource getScreenshot(String format) throws DeviceNotAvailableException;

    /**
     * Grabs a screenshot from the device. Recommended to use {@link #getScreenshot(String)} instead
     * with JPEG encoding for smaller size.
     *
     * @param format supported PNG, JPEG
     * @param rescale if screenshot should be rescaled to reduce the size of resulting image
     * @return a {@link InputStreamSource} of the screenshot in format, or <code>null</code> if the
     *     screenshot was not successful.
     * @throws DeviceNotAvailableException
     */
    public InputStreamSource getScreenshot(String format, boolean rescale)
            throws DeviceNotAvailableException;

    /**
     * Grabs a screenshot from the device given display id. Format is PNG.
     *
     * <p>TODO: extend the implementations above to support 'format' and 'rescale'
     *
     * @param displayId the display id of the screen to get screenshot from.
     * @return a {@link InputStreamSource} of the screenshot in format, or <code>null</code> if the
     *     screenshot was not successful.
     * @throws DeviceNotAvailableException
     */
    public InputStreamSource getScreenshot(long displayId) throws DeviceNotAvailableException;

    /**
     * Clears the last connected wifi network. This should be called when starting a new invocation
     * to avoid connecting to the wifi network used in the previous test after device reboots.
     */
    public void clearLastConnectedWifiNetwork();

    /**
     * Connects to a wifi network.
     * <p/>
     * Turns on wifi and blocks until a successful connection is made to the specified wifi network.
     * Once a connection is made, the instance will try to restore the connection after every reboot
     * until {@link ITestDevice#disconnectFromWifi()} or
     * {@link ITestDevice#clearLastConnectedWifiNetwork()} is called.
     *
     * @param wifiSsid the wifi ssid to connect to
     * @param wifiPsk PSK passphrase or null if unencrypted
     * @return <code>true</code> if connected to wifi network successfully. <code>false</code>
     *         otherwise
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean connectToWifiNetwork(String wifiSsid, String wifiPsk)
            throws DeviceNotAvailableException;

    /**
     * Connects to a wifi network.
     * <p/>
     * Turns on wifi and blocks until a successful connection is made to the specified wifi network.
     * Once a connection is made, the instance will try to restore the connection after every reboot
     * until {@link ITestDevice#disconnectFromWifi()} or
     * {@link ITestDevice#clearLastConnectedWifiNetwork()} is called.
     *
     * @param wifiSsid the wifi ssid to connect to
     * @param wifiPsk PSK passphrase or null if unencrypted
     * @param scanSsid whether to scan for hidden SSID for this network.
     * @return <code>true</code> if connected to wifi network successfully. <code>false</code>
     *         otherwise
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean connectToWifiNetwork(String wifiSsid, String wifiPsk, boolean scanSsid)
            throws DeviceNotAvailableException;

    /**
     * A variant of {@link #connectToWifiNetwork(String, String)} that only connects if device
     * currently does not have network connectivity.
     *
     * @param wifiSsid
     * @param wifiPsk
     * @return <code>true</code> if connected to wifi network successfully. <code>false</code>
     *         otherwise
     * @throws DeviceNotAvailableException
     */
    public boolean connectToWifiNetworkIfNeeded(String wifiSsid, String wifiPsk)
            throws DeviceNotAvailableException;

    /**
     * A variant of {@link #connectToWifiNetwork(String, String)} that only connects if device
     * currently does not have network connectivity.
     *
     * @param wifiSsid
     * @param wifiPsk
     * @param scanSsid whether to scan for hidden SSID for this network
     * @return <code>true</code> if connected to wifi network successfully. <code>false</code>
     *         otherwise
     * @throws DeviceNotAvailableException
     */
    public boolean connectToWifiNetworkIfNeeded(String wifiSsid, String wifiPsk, boolean scanSsid)
            throws DeviceNotAvailableException;

    /**
     * Disconnects from a wifi network.
     * <p/>
     * Removes all networks from known networks list and disables wifi.
     *
     * @return <code>true</code> if disconnected from wifi network successfully. <code>false</code>
     *         if disconnect failed.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean disconnectFromWifi() throws DeviceNotAvailableException;

    /**
     * Test if wifi is enabled.
     * <p/>
     * Checks if wifi is enabled on device. Useful for asserting wifi status before tests that
     * shouldn't run with wifi, e.g. mobile data tests.
     *
     * @return <code>true</code> if wifi is enabled. <code>false</code> if disabled
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean isWifiEnabled() throws DeviceNotAvailableException;

    /**
     * Gets the device's IP address.
     *
     * @return the device's IP address, or <code>null</code> if device has no IP address
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public String getIpAddress() throws DeviceNotAvailableException;

    /**
     * Enables network monitoring on device.
     *
     * @return <code>true</code> if monitoring is enabled successfully. <code>false</code>
     *         if it failed.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean enableNetworkMonitor() throws DeviceNotAvailableException;

    /**
     * Disables network monitoring on device.
     *
     * @return <code>true</code> if monitoring is disabled successfully. <code>false</code>
     *         if it failed.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean disableNetworkMonitor() throws DeviceNotAvailableException;

    /**
     * Check that device has network connectivity.
     *
     * @return <code>true</code> if device has a working network connection,
     *          <code>false</code> overwise.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *          recovered.
     */
    public boolean checkConnectivity() throws DeviceNotAvailableException;

    /**
     * Attempt to dismiss any error dialogs currently displayed on device UI.
     *
     * @return <code>true</code> if no dialogs were present or dialogs were successfully cleared.
     *         <code>false</code> otherwise.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *             recovered.
     */
    public boolean clearErrorDialogs() throws DeviceNotAvailableException;

    /**
     * Return an object to get the current state of the keyguard or null if not supported.
     *
     * @return a {@link KeyguardControllerState} containing a snapshot of the state of the keyguard
     *     and returns Null if the Keyguard query is not supported.
     * @throws DeviceNotAvailableException if connection with device is lost and cannot be
     *     recovered.
     */
    public KeyguardControllerState getKeyguardState() throws DeviceNotAvailableException;

    /**
     * Fetch the test options for the device.
     *
     * @return {@link TestDeviceOptions} related to the device under test.
     */
    public TestDeviceOptions getOptions();

    /**
     * Fetch the application package names present on the device.
     *
     * @return {@link Set} of {@link String} package names currently installed on the device.
     * @throws DeviceNotAvailableException
     */
    public Set<String> getInstalledPackageNames() throws DeviceNotAvailableException;

    /**
     * Query the device for a given package name to check if it's currently installed or not.
     *
     * @return True if the package is reported as installed. False otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean isPackageInstalled(String packageName) throws DeviceNotAvailableException;

    /**
     * Query the device for a given package name and given user id to check if it's currently
     * installed or not for that user.
     *
     * @param packageName the package we are checking if it's installed.
     * @param userId The user id we are checking the package is installed for. If null, primary user
     *     zero will be used.
     * @return True if the package is reported as installed. False otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean isPackageInstalled(String packageName, String userId)
            throws DeviceNotAvailableException;

    /**
     * Fetch the information about APEXes activated on the device.
     *
     * @return {@link Set} of {@link ApexInfo} currently activated on the device
     * @throws DeviceNotAvailableException
     */
    public Set<ApexInfo> getActiveApexes() throws DeviceNotAvailableException;

    /**
     * Fetch the application package names that can be uninstalled. This is presently defined as
     * non-system packages, and updated system packages.
     *
     * @return {@link Set} of uninstallable {@link String} package names currently installed on the
     *         device.
     * @throws DeviceNotAvailableException
     */
    public Set<String> getUninstallablePackageNames() throws DeviceNotAvailableException;

    /**
     * Fetch information about a package installed on device.
     *
     * @return the {@link PackageInfo} or <code>null</code> if information could not be retrieved
     * @throws DeviceNotAvailableException
     */
    public PackageInfo getAppPackageInfo(String packageName) throws DeviceNotAvailableException;

    /**
     * Fetch information of packages installed on the device.
     *
     * @return {@link List} of {@link PackageInfo}s installed on the device.
     * @throws DeviceNotAvailableException
     */
    public List<PackageInfo> getAppPackageInfos() throws DeviceNotAvailableException;

    /**
     * Determines if multi user is supported.
     *
     * @return true if multi user is supported, false otherwise
     * @throws DeviceNotAvailableException
     */
    public boolean isMultiUserSupported() throws DeviceNotAvailableException;

    /**
     * Create a user with a given name and default flags 0.
     *
     * @param name of the user to create on the device
     * @return the integer for the user id created
     * @throws DeviceNotAvailableException
     */
    public int createUser(String name) throws DeviceNotAvailableException, IllegalStateException;

    /**
     * Create a user with a given name and default flags 0.
     *
     * @param name of the user to create on the device
     * @return the integer for the user id created or -1 for error.
     * @throws DeviceNotAvailableException
     */
    public int createUserNoThrow(String name) throws DeviceNotAvailableException;

    /**
     * Create a user with a given name and the provided flags
     *
     * @param name of the user to create on the device
     * @param guest enable the user flag --guest during creation
     * @param ephemeral enable the user flag --ephemeral during creation
     * @return id of the created user
     * @throws DeviceNotAvailableException
     */
    public int createUser(String name, boolean guest, boolean ephemeral)
            throws DeviceNotAvailableException, IllegalStateException;

    /**
     * Remove a given user from the device.
     *
     * @param userId of the user to remove
     * @return true if we were successful in removing the user, false otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean removeUser(int userId) throws DeviceNotAvailableException;

    /**
     * Gets the list of users on the device. Will throw {@link DeviceRuntimeException} if output
     * from device is not as expected.
     *
     * @return the list of user ids.
     * @throws DeviceNotAvailableException
     * @throws DeviceRuntimeException
     */
    ArrayList<Integer> listUsers() throws DeviceNotAvailableException;

    /**
     * Gets the Map of useId to {@link UserInfo} on the device. Will throw {@link
     * DeviceRuntimeException} if output from device is not as expected.
     *
     * @return the list of UserInfo objects.
     * @throws DeviceNotAvailableException
     * @throws DeviceRuntimeException
     */
    public Map<Integer, UserInfo> getUserInfos() throws DeviceNotAvailableException;

    /**
     * Get the maximum number of supported users. Defaults to 0.
     *
     * @return an integer indicating the number of supported users
     * @throws DeviceNotAvailableException
     */
    public int getMaxNumberOfUsersSupported() throws DeviceNotAvailableException;

    /**
     * Get the maximum number of supported simultaneously running users. Defaults to 0.
     *
     * @return an integer indicating the number of simultaneously running users
     * @throws DeviceNotAvailableException
     */
    public int getMaxNumberOfRunningUsersSupported() throws DeviceNotAvailableException;

    /**
     * Starts a given user in the background if it is currently stopped. If the user is already
     * running in the background, this method is a NOOP.
     * @param userId of the user to start in the background
     * @return true if the user was successfully started in the background.
     * @throws DeviceNotAvailableException
     */
    public boolean startUser(int userId) throws DeviceNotAvailableException;

    /**
     * Starts a given user in the background if it is currently stopped. If the user is already
     * running in the background, this method is a NOOP. Possible to provide extra flag to wait for
     * the operation to have effect.
     *
     * @param userId of the user to start in the background
     * @param waitFlag will make the command wait until user is started and unlocked.
     * @return true if the user was successfully started in the background.
     * @throws DeviceNotAvailableException
     */
    public boolean startUser(int userId, boolean waitFlag) throws DeviceNotAvailableException;

    /**
     * Stops a given user. If the user is already stopped, this method is a NOOP.
     * Cannot stop current and system user.
     *
     * @param userId of the user to stop.
     * @return true if the user was successfully stopped.
     * @throws DeviceNotAvailableException
     */
    public boolean stopUser(int userId) throws DeviceNotAvailableException;

    /**
     * Stop a given user. Possible to provide extra flags to wait for the operation to have effect,
     * and force terminate the user. Cannot stop current and system user.
     *
     * @param userId of the user to stop.
     * @param waitFlag will make the command wait until user is stopped.
     * @param forceFlag will force stop the user.
     * @return true if the user was successfully stopped.
     * @throws DeviceNotAvailableException
     */
    public boolean stopUser(int userId, boolean waitFlag, boolean forceFlag)
            throws DeviceNotAvailableException;

    /**
     * Returns the primary user id.
     *
     * @return the userId of the primary user if there is one, and null if there is no primary user.
     * @throws DeviceNotAvailableException
     * @throws DeviceRuntimeException if the output from the device is not as expected.
     */
    public Integer getPrimaryUserId() throws DeviceNotAvailableException;

    /**
     * Return the id of the current running user. In case of error, return -10000.
     *
     * @throws DeviceNotAvailableException
     */
    public int getCurrentUser() throws DeviceNotAvailableException;

    /**
     * Find and return the flags of a given user.
     * Flags are defined in "android.content.pm.UserInfo" class in Android Open Source Project.
     *
     * @return the flags associated with the userId provided if found, -10000 in any other cases.
     * @throws DeviceNotAvailableException
     */
    public int getUserFlags(int userId) throws DeviceNotAvailableException;

    /**
     * Return whether the specified user is a secondary user according to it's flags.
     *
     * @return true if the user is secondary, false otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean isUserSecondary(int userId) throws DeviceNotAvailableException;

    /**
     * Return the serial number associated to the userId if found, -10000 in any other cases.
     *
     * @throws DeviceNotAvailableException
     */
    public int getUserSerialNumber(int userId) throws DeviceNotAvailableException;

    /**
     * Switch to another userId with a default timeout. {@link #switchUser(int, long)}.
     *
     * @return True if the new userId matches the userId provider. False otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean switchUser(int userId) throws DeviceNotAvailableException;

    /**
     * Switch to another userId with the provided timeout as deadline.
     * Attempt to disable keyguard after user change is successful.
     *
     * @param timeout to wait before returning false for switch-user failed.
     * @return True if the new userId matches the userId provider. False otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean switchUser(int userId, long timeout) throws DeviceNotAvailableException;

    /**
     * Check if a given user is running.
     *
     * @return True if the user is running, false in every other cases.
     * @throws DeviceNotAvailableException
     */
    public boolean isUserRunning(int userId) throws DeviceNotAvailableException;

    /**
     * Check if a feature is available on a device.
     *
     * @param feature which format should be "feature:<name>" or "<name>" directly.
     * @return True if feature is found, false otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean hasFeature(String feature) throws DeviceNotAvailableException;

    /**
     * See {@link #getSetting(int, String, String)} and performed on system user.
     *
     * @throws DeviceNotAvailableException
     */
    public String getSetting(String namespace, String key) throws DeviceNotAvailableException;

    /**
     * Return the value of the requested setting.
     * namespace must be one of: {"system", "secure", "global"}
     *
     * @return the value associated with the namespace:key of a user. Null if not found.
     * @throws DeviceNotAvailableException
     */
    public String getSetting(int userId, String namespace, String key)
            throws DeviceNotAvailableException;

    /**
     * Return key value pairs of requested namespace.
     *
     * @param namespace must be one of {"system", "secure", "global"}
     * @return the map of key value pairs. Null if namespace is not supported.
     * @throws DeviceNotAvailableException
     */
    public Map<String, String> getAllSettings(String namespace) throws DeviceNotAvailableException;

    /**
     * See {@link #setSetting(int, String, String, String)} and performed on system user.
     *
     * @throws DeviceNotAvailableException
     */
    public void setSetting(String namespace, String key, String value)
            throws DeviceNotAvailableException;

    /**
     * Add a setting value to the namespace of a given user. Some settings will only be available
     * after a reboot.
     * namespace must be one of: {"system", "secure", "global"}
     *
     * @throws DeviceNotAvailableException
     */
    public void setSetting(int userId, String namespace, String key, String value)
            throws DeviceNotAvailableException;

    /**
     * Find and return the android-id associated to a userId, null if not found.
     *
     * @throws DeviceNotAvailableException
     */
    public String getAndroidId(int userId) throws DeviceNotAvailableException;

    /**
     * Create a Map of android ids found matching user ids. There is no insurance that each user
     * id will found an android id associated in this function so some user ids may match null.
     *
     * @return Map of android ids found matching user ids.
     * @throws DeviceNotAvailableException
     */
    public Map<Integer, String> getAndroidIds() throws DeviceNotAvailableException;

    /**
     * Set a device admin component as device owner in given user.
     *
     * @param componentName of device admin to be device owner.
     * @param userId of the user that the device owner lives in.
     * @return True if it is successful, false otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean setDeviceOwner(String componentName, int userId)
            throws DeviceNotAvailableException;

    /**
     * Remove given device admin in given user and return {@code true} if it is successful, {@code
     * false} otherwise.
     *
     * @param componentName of device admin to be removed.
     * @param userId of user that the device admin lives in.
     * @return True if it is successful, false otherwise.
     * @throws DeviceNotAvailableException
     */
    public boolean removeAdmin(String componentName, int userId) throws DeviceNotAvailableException;

    /**
     * Remove all existing device profile owners with the best effort.
     *
     * @throws DeviceNotAvailableException
     */
    public void removeOwners() throws DeviceNotAvailableException;

    /**
     * Attempts to disable the keyguard.
     * <p>
     * First wait for the input dispatch to become ready, this happens around the same time when the
     * device reports BOOT_COMPLETE, apparently asynchronously, because current framework
     * implementation has occasional race condition. Then command is sent to dismiss keyguard (works
     * on non-secure ones only)
     */
    public void disableKeyguard() throws DeviceNotAvailableException;

    /**
     * Attempt to dump the heap from the system_server. It is the caller responsibility to clean up
     * the dumped file.
     *
     * @param process the name of the device process to dumpheap on.
     * @param devicePath the path on the device where to put the dump. This must be a location where
     *     permissions allow it.
     * @return the {@link File} containing the report. Null if something failed.
     * @throws DeviceNotAvailableException
     */
    public File dumpHeap(String process, String devicePath) throws DeviceNotAvailableException;

    /**
     * Collect the list of available displays id on the device as reported by "dumpsys
     * SurfaceFlinger".
     *
     * @return The list of displays. Default always returns the default display 0.
     * @throws DeviceNotAvailableException
     */
    public Set<Long> listDisplayIds() throws DeviceNotAvailableException;
}
