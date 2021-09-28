/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.car;

import android.app.ActivityManager;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothProfile;
import android.car.ICarBluetooth;
import android.car.ICarBluetoothUserService;
import android.car.IPerUserCarService;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;
import android.util.SparseArray;

import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * CarBluetoothService - Maintains the current user's Bluetooth devices and profile connections.
 *
 * For each user, creates:
 *   1) Set of {@link BluetoothProfileDeviceManager} objects, responsible for maintaining a list of
 *      the user's known devices. These provide an interface to auto-connect devices for the given
 *      profile.
 *   2) A {@link BluetoothProfileInhibitManager} object that will maintain a set of inhibited
 *      profiles for each device, keeping a device from connecting on those profiles. This provides
 *      an interface to request and release inhibits.
 *   3) A {@link BluetoothDeviceConnectionPolicy} object, representing a default implementation of
 *      a policy based method of determining and requesting times to auto-connect devices. This
 *      default is controllable through a resource overlay if one chooses to implement their own.
 *
 * Provides an interface for other programs to request auto connections.
 */
public class CarBluetoothService extends ICarBluetooth.Stub implements CarServiceBase {
    private static final String TAG = "CarBluetoothService";
    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);
    private final Context mContext;

    // The list of profiles we wish to manage
    private static final List<Integer> sManagedProfiles = Arrays.asList(
            BluetoothProfile.HEADSET_CLIENT,
            BluetoothProfile.PBAP_CLIENT,
            BluetoothProfile.A2DP_SINK,
            BluetoothProfile.MAP_CLIENT,
            BluetoothProfile.PAN
    );

    // Each time PerUserCarService connects we need to get new Bluetooth profile proxies and refresh
    // all our internal objects to use them. When it disconnects we're to assume our proxies are
    // invalid. This lock protects all our internal objects.
    private final Object mPerUserLock = new Object();

    // Set of Bluetooth Profile Device Managers, own the priority connection lists, updated on user
    // switch
    private final SparseArray<BluetoothProfileDeviceManager> mProfileDeviceManagers =
            new SparseArray<>();

    // Profile-Inhibit Manager that will temporarily inhibit connections on profiles, per user
    @GuardedBy("mPerUserLock")
    private BluetoothProfileInhibitManager mInhibitManager = null;

    // Default Bluetooth device connection policy, per user, enabled with an overlay
    private final boolean mUseDefaultPolicy;
    @GuardedBy("mPerUserLock")
    private BluetoothDeviceConnectionPolicy mBluetoothDeviceConnectionPolicy = null;

    // Listen for user switch events from the PerUserCarService
    @GuardedBy("mPerUserLock")
    private int mUserId;
    @GuardedBy("mPerUserLock")
    private IPerUserCarService mPerUserCarService;
    @GuardedBy("mPerUserLock")
    private ICarBluetoothUserService mCarBluetoothUserService;
    private final PerUserCarServiceHelper mUserServiceHelper;
    private final PerUserCarServiceHelper.ServiceCallback mUserServiceCallback =
            new PerUserCarServiceHelper.ServiceCallback() {
        @Override
        public void onServiceConnected(IPerUserCarService perUserCarService) {
            logd("Connected to PerUserCarService");
            synchronized (mPerUserLock) {
                // Explicitly clear out existing per-user objects since we can't rely on the
                // onServiceDisconnected and onPreUnbind calls to always be called before this
                destroyUserLocked();

                mPerUserCarService = perUserCarService;

                // Create new objects with our new set of profile proxies
                initializeUserLocked();
            }
        }

        @Override
        public void onPreUnbind() {
            logd("Before Unbinding from PerUserCarService");
            synchronized (mPerUserLock) {
                destroyUserLocked();
            }
        }

        @Override
        public void onServiceDisconnected() {
            logd("Disconnected from PerUserCarService");
            synchronized (mPerUserLock) {
                destroyUserLocked();
            }
        }
    };

    /**
     * Create an instance of CarBluetoothService
     *
     * @param context - A Context object representing the context you want this service to run
     * @param userSwitchService - An instance of PerUserCarServiceHelper that we can bind a listener
     *                            to in order to receive user switch events
     */
    public CarBluetoothService(Context context, PerUserCarServiceHelper userSwitchService) {
        mUserId = UserHandle.USER_NULL;
        mContext = context;
        mUserServiceHelper = userSwitchService;
        mUseDefaultPolicy = mContext.getResources().getBoolean(
                R.bool.useDefaultBluetoothConnectionPolicy);
    }

    /**
     * Complete all necessary initialization keeping this service from being running.
     *
     * Wait for the user service helper to report a user before initializing a user.
     */
    @Override
    public void init() {
        logd("init()");
        mUserServiceHelper.registerServiceCallback(mUserServiceCallback);
    }

    /**
     * Release all resources required to run this service and stop running.
     *
     * Clean up the user context once we've detached from the user service helper, if any.
     */
    @Override
    public void release() {
        logd("release()");
        mUserServiceHelper.unregisterServiceCallback(mUserServiceCallback);
        synchronized (mPerUserLock) {
            destroyUserLocked();
        }
    }

    /**
     * Returns the list of profiles that the Autoconnection policy attempts to connect on
     *
     * @return The list of managed Bluetooth profiles
     */
    public static List<Integer> getManagedProfiles() {
        return sManagedProfiles;
    }

    /**
     * Initialize the user context using the current active user.
     *
     * Only call this following a known user switch once we've connected to the user service helper.
     */
    private void initializeUserLocked() {
        logd("Initializing new user");
        mUserId = ActivityManager.getCurrentUser();
        createBluetoothUserServiceLocked();
        createBluetoothProfileDeviceManagersLocked();
        createBluetoothProfileInhibitManagerLocked();

        // Determine if we need to begin the default policy
        mBluetoothDeviceConnectionPolicy = null;
        if (mUseDefaultPolicy) {
            createBluetoothDeviceConnectionPolicyLocked();
        }
        logd("Switched to user " + mUserId);
    }

    /**
     * Destroy the current user context, defined by the set of profile proxies, profile device
     * managers, inhibit manager and the policy.
     */
    private void destroyUserLocked() {
        logd("Destroying user " + mUserId);
        destroyBluetoothDeviceConnectionPolicyLocked();
        destroyBluetoothProfileInhibitManagerLocked();
        destroyBluetoothProfileDeviceManagersLocked();
        destroyBluetoothUserServiceLocked();
        mPerUserCarService = null;
        mUserId = UserHandle.USER_NULL;
    }

    /**
     * Sets the Per User Car Bluetooth Service (ICarBluetoothService) from the PerUserCarService
     * which acts as a top level Service running in the current user context.
     * Also sets up the connection proxy objects required to communicate with the Bluetooth
     * Profile Services.
     */
    private void createBluetoothUserServiceLocked() {
        if (mPerUserCarService != null) {
            try {
                mCarBluetoothUserService = mPerUserCarService.getBluetoothUserService();
                mCarBluetoothUserService.setupBluetoothConnectionProxies();
            } catch (RemoteException e) {
                Log.e(TAG, "Remote Service Exception on ServiceConnection Callback: "
                        + e.getMessage());
            } catch (java.lang.NullPointerException e) {
                Log.e(TAG, "Initialization Failed: " + e.getMessage());
            }
        } else {
            logd("PerUserCarService not connected. Cannot get bluetooth user proxy objects");
        }
    }

    /**
     * Close out the Per User Car Bluetooth profile proxy connections and destroys the Car Bluetooth
     * User Service object.
     */
    private void destroyBluetoothUserServiceLocked() {
        if (mCarBluetoothUserService == null) return;
        try {
            mCarBluetoothUserService.closeBluetoothConnectionProxies();
        } catch (RemoteException e) {
            Log.e(TAG, "Remote Service Exception on ServiceConnection Callback: "
                    + e.getMessage());
        }
        mCarBluetoothUserService = null;
    }

    /**
     * Clears out Profile Device Managers and re-creates them for the current user.
     */
    private void createBluetoothProfileDeviceManagersLocked() {
        if (mUserId == UserHandle.USER_NULL) {
            logd("No foreground user, cannot create profile device managers");
            return;
        }
        for (int profileId : sManagedProfiles) {
            BluetoothProfileDeviceManager deviceManager = mProfileDeviceManagers.get(profileId);
            if (deviceManager != null) {
                deviceManager.stop();
                mProfileDeviceManagers.remove(profileId);
                logd("Existing device manager removed for profile "
                        + Utils.getProfileName(profileId));
            }

            deviceManager = BluetoothProfileDeviceManager.create(mContext, mUserId,
                    mCarBluetoothUserService, profileId);
            if (deviceManager == null) {
                logd("Failed to create profile device manager for "
                        + Utils.getProfileName(profileId));
                continue;
            }
            mProfileDeviceManagers.put(profileId, deviceManager);
            logd("Created profile device manager for " + Utils.getProfileName(profileId));
        }

        for (int i = 0; i < mProfileDeviceManagers.size(); i++) {
            int key = mProfileDeviceManagers.keyAt(i);
            BluetoothProfileDeviceManager deviceManager =
                    (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(key);
            deviceManager.start();
        }
    }

    /**
     * Stops and clears the entire set of Profile Device Managers.
     */
    private void destroyBluetoothProfileDeviceManagersLocked() {
        for (int i = 0; i < mProfileDeviceManagers.size(); i++) {
            int key = mProfileDeviceManagers.keyAt(i);
            BluetoothProfileDeviceManager deviceManager =
                    (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(key);
            deviceManager.stop();
        }
        mProfileDeviceManagers.clear();
    }

    /**
     * Creates an instance of a BluetoothProfileInhibitManager under the current user
     */
    private void createBluetoothProfileInhibitManagerLocked() {
        logd("Creating inhibit manager");
        if (mUserId == UserHandle.USER_NULL) {
            logd("No foreground user, cannot create profile inhibit manager");
            return;
        }
        mInhibitManager = new BluetoothProfileInhibitManager(mContext, mUserId,
                mCarBluetoothUserService);
        mInhibitManager.start();
    }

    /**
     * Destroys the current instance of a BluetoothProfileInhibitManager, if one exists
     */
    private void destroyBluetoothProfileInhibitManagerLocked() {
        logd("Destroying inhibit manager");
        if (mInhibitManager == null) return;
        mInhibitManager.stop();
        mInhibitManager = null;
    }

    /**
     * Creates an instance of a BluetoothDeviceConnectionPolicy under the current user
     */
    private void createBluetoothDeviceConnectionPolicyLocked() {
        logd("Creating device connection policy");
        if (mUserId == UserHandle.USER_NULL) {
            logd("No foreground user, cannot create device connection policy");
            return;
        }
        mBluetoothDeviceConnectionPolicy = BluetoothDeviceConnectionPolicy.create(mContext,
                mUserId, this);
        if (mBluetoothDeviceConnectionPolicy == null) {
            logd("Failed to create default Bluetooth device connection policy.");
            return;
        }
        mBluetoothDeviceConnectionPolicy.init();
    }

    /**
     * Destroys the current instance of a BluetoothDeviceConnectionPolicy, if one exists
     */
    private void destroyBluetoothDeviceConnectionPolicyLocked() {
        logd("Destroying device connection policy");
        if (mBluetoothDeviceConnectionPolicy != null) {
            mBluetoothDeviceConnectionPolicy.release();
            mBluetoothDeviceConnectionPolicy = null;
        }
    }

     /**
     * Determine if we are using the default device connection policy or not
     *
     * @return true if the default policy is active, false otherwise
     */
    public boolean isUsingDefaultConnectionPolicy() {
        synchronized (mPerUserLock) {
            return mBluetoothDeviceConnectionPolicy != null;
        }
    }

   /**
     * Initiate automatated connecting of devices based on the prioritized device lists for each
     * profile.
     */
    @Override
    public void connectDevices() {
        enforceBluetoothAdminPermission();
        logd("Connect devices for each profile");
        synchronized (mPerUserLock) {
            for (int i = 0; i < mProfileDeviceManagers.size(); i++) {
                int key = mProfileDeviceManagers.keyAt(i);
                BluetoothProfileDeviceManager deviceManager =
                        (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(key);
                deviceManager.beginAutoConnecting();
            }
        }
    }

    /**
     * Get the Auto Connect priority for a paired Bluetooth Device.
     *
     * @param profile  - BluetoothProfile to get priority list for
     * @return A list of BluetoothDevice objects, ordered by highest priority first
     */
    public List<BluetoothDevice> getProfileDevicePriorityList(int profile) {
        enforceBluetoothAdminPermission();
        synchronized (mPerUserLock) {
            BluetoothProfileDeviceManager deviceManager =
                    (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(profile);
            if (deviceManager != null) {
                return deviceManager.getDeviceListSnapshot();
            }
        }
        return new ArrayList<BluetoothDevice>();
    }

    /**
     * Get the Auto Connect priority for a paired Bluetooth Device.
     *
     * @param profile  - BluetoothProfile to get priority for
     * @param device   - Device to get priority for
     * @return integer priority value, or -1 if no priority available.
     */
    public int getDeviceConnectionPriority(int profile, BluetoothDevice device) {
        enforceBluetoothAdminPermission();
        synchronized (mPerUserLock) {
            BluetoothProfileDeviceManager deviceManager =
                    (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(profile);
            if (deviceManager != null) {
                return deviceManager.getDeviceConnectionPriority(device);
            }
        }
        return -1;
    }

    /**
     * Set the Auto Connect priority for a paired Bluetooth Device.
     *
     * @param profile  - BluetoothProfile to set priority for
     * @param device   - Device to set priority (Tag)
     * @param priority - What priority level to set to
     */
    public void setDeviceConnectionPriority(int profile, BluetoothDevice device, int priority) {
        enforceBluetoothAdminPermission();
        synchronized (mPerUserLock) {
            BluetoothProfileDeviceManager deviceManager =
                    (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(profile);
            if (deviceManager != null) {
                deviceManager.setDeviceConnectionPriority(device, priority);
            }
        }
        return;
    }

    /**
     * Request to disconnect the given profile on the given device, and prevent it from reconnecting
     * until either the request is released, or the process owning the given token dies.
     *
     * @param device  The device on which to inhibit a profile.
     * @param profile The {@link android.bluetooth.BluetoothProfile} to inhibit.
     * @param token   A {@link IBinder} to be used as an identity for the request. If the process
     *                owning the token dies, the request will automatically be released
     * @return True if the profile was successfully inhibited, false if an error occurred.
     */
    boolean requestProfileInhibit(BluetoothDevice device, int profile, IBinder token) {
        logd("Request profile inhibit: profile " + Utils.getProfileName(profile)
                + ", device " + device.getAddress());
        synchronized (mPerUserLock) {
            if (mInhibitManager == null) return false;
            return mInhibitManager.requestProfileInhibit(device, profile, token);
        }
    }

    /**
     * Undo a previous call to {@link #requestProfileInhibit} with the same parameters,
     * and reconnect the profile if no other requests are active.
     *
     * @param device  The device on which to release the inhibit request.
     * @param profile The profile on which to release the inhibit request.
     * @param token   The token provided in the original call to
     *                {@link #requestBluetoothProfileInhibit}.
     * @return True if the request was released, false if an error occurred.
     */
    boolean releaseProfileInhibit(BluetoothDevice device, int profile, IBinder token) {
        logd("Release profile inhibit: profile " + Utils.getProfileName(profile)
                + ", device " + device.getAddress());
        synchronized (mPerUserLock) {
            if (mInhibitManager == null) return false;
            return mInhibitManager.releaseProfileInhibit(device, profile, token);
        }
    }

    /**
     * Make sure the caller has the Bluetooth permissions that are required to execute any function
     */
    private void enforceBluetoothAdminPermission() {
        if (mContext != null
                && PackageManager.PERMISSION_GRANTED == mContext.checkCallingOrSelfPermission(
                android.Manifest.permission.BLUETOOTH_ADMIN)) {
            return;
        }
        if (mContext == null) {
            Log.e(TAG, "CarBluetoothPrioritySettings does not have a Context");
        }
        throw new SecurityException("requires permission "
                + android.Manifest.permission.BLUETOOTH_ADMIN);
    }

    /**
     * Print out the verbose debug status of this object
     */
    @Override
    public void dump(PrintWriter writer) {
        writer.println("*" + TAG + "*");
        synchronized (mPerUserLock) {
            writer.println("\tUser ID: " + mUserId);
            writer.println("\tUser Proxies: " + (mCarBluetoothUserService != null ? "Yes" : "No"));

            // Profile Device Manager statuses
            for (int i = 0; i < mProfileDeviceManagers.size(); i++) {
                int key = mProfileDeviceManagers.keyAt(i);
                BluetoothProfileDeviceManager deviceManager =
                        (BluetoothProfileDeviceManager) mProfileDeviceManagers.get(key);
                deviceManager.dump(writer, "\t");
            }

            // Profile Inhibits
            if (mInhibitManager != null) mInhibitManager.dump(writer, "\t");
            else writer.println("\tBluetoothProfileInhibitManager: null");

            // Device Connection Policy
            writer.println("\tUsing default policy? " + (mUseDefaultPolicy ? "Yes" : "No"));
            if (mBluetoothDeviceConnectionPolicy == null) {
                writer.println("\tBluetoothDeviceConnectionPolicy: null");
            } else {
                mBluetoothDeviceConnectionPolicy.dump(writer, "\t");
            }
        }
    }

    /**
     * Log to debug if debug output is enabled
     */
    private static void logd(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }
}
