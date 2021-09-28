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
package com.android.car.bugreport;

import android.app.ActivityThread;
import android.os.Build;
import android.os.SystemProperties;
import android.provider.DeviceConfig;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import com.google.common.collect.ImmutableSet;

import java.io.PrintWriter;

/**
 * Contains config for BugReport App.
 *
 * <p>The config is kept synchronized with {@code car} namespace. It's not defined in
 * {@link DeviceConfig}.
 *
 * <ul>To get/set the flags via adb:
 *   <li>{@code adb shell device_config get car bugreport_upload_destination}
 *   <li>{@code adb shell device_config put car bugreport_upload_destination gcs}
 *   <li>{@code adb shell device_config delete car bugreport_upload_destination}
 * </ul>
 */
final class Config {
    private static final String TAG = Config.class.getSimpleName();

    /**
     * Namespace for all Android Automotive related features.
     *
     * <p>In the future it will move to {@code DeviceConfig#NAMESPACE_CAR}.
     */
    private static final String NAMESPACE_CAR = "car";

    /**
     * A string flag, can be one of {@code null} or {@link #UPLOAD_DESTINATION_GCS}.
     */
    private static final String KEY_BUGREPORT_UPLOAD_DESTINATION = "bugreport_upload_destination";

    /**
     * A value for {@link #KEY_BUGREPORT_UPLOAD_DESTINATION}.
     *
     * Upload bugreports to GCS. Only works in {@code userdebug} or {@code eng} builds.
     */
    private static final String UPLOAD_DESTINATION_GCS = "gcs";

    /**
     * A system property to force enable the app bypassing the {@code userdebug/eng} build check.
     */
    private static final String PROP_FORCE_ENABLE = "android.car.bugreport.force_enable";

    /*
     * Enable uploading new bugreports to GCS for these devices. If the device is not in this list,
     * {@link #KEY_UPLOAD_DESTINATION} flag will be used instead.
     */
    private static final ImmutableSet<String> ENABLE_FORCE_UPLOAD_TO_GCS_FOR_DEVICES =
            ImmutableSet.of("hawk", "seahawk");

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private String mUploadDestination = null;

    void start() {
        DeviceConfig.addOnPropertiesChangedListener(NAMESPACE_CAR,
                ActivityThread.currentApplication().getMainExecutor(), this::onPropertiesChanged);
        updateConstants();
    }

    private void onPropertiesChanged(DeviceConfig.Properties properties) {
        if (properties.getKeyset().contains(KEY_BUGREPORT_UPLOAD_DESTINATION)) {
            updateConstants();
        }
    }

    /** Returns true if bugreport app is enabled for this device. */
    static boolean isBugReportEnabled() {
        return Build.IS_DEBUGGABLE || SystemProperties.getBoolean(PROP_FORCE_ENABLE, false);
    }

    /** If new bugreports should be scheduled for uploading. */
    boolean getAutoUpload() {
        // TODO(b/144851443): Enable auto-upload only if upload destination is Gcs until
        //                    we create a way to allow implementing OEMs custom upload logic.
        return isUploadDestinationGcs();
    }

    /**
     * Returns {@link true} if bugreport upload destination is GCS.
     */
    private boolean isUploadDestinationGcs() {
        if (isTempForceAutoUploadGcsEnabled()) {
            Log.d(TAG, "Setting upload dest to GCS ENABLE_AUTO_UPLOAD is true");
            return true;
        }
        // NOTE: enable it only for userdebug/eng builds.
        return UPLOAD_DESTINATION_GCS.equals(getUploadDestination()) && Build.IS_DEBUGGABLE;
    }

    private static boolean isTempForceAutoUploadGcsEnabled() {
        return ENABLE_FORCE_UPLOAD_TO_GCS_FOR_DEVICES.contains(Build.DEVICE);
    }

    /**
     * Returns value of a flag {@link #KEY_BUGREPORT_UPLOAD_DESTINATION}.
     */
    private String getUploadDestination() {
        synchronized (mLock) {
            return mUploadDestination;
        }
    }

    private void updateConstants() {
        synchronized (mLock) {
            mUploadDestination = DeviceConfig.getString(NAMESPACE_CAR,
                    KEY_BUGREPORT_UPLOAD_DESTINATION, /* defaultValue= */ null);
        }
    }

    void dump(String prefix, PrintWriter pw) {
        pw.println(prefix + "car.bugreport.Config:");

        pw.print(prefix + "  ");
        pw.print("getAutoUpload");
        pw.print("=");
        pw.println(getAutoUpload() ? "true" : "false");

        pw.print(prefix + "  ");
        pw.print("getUploadDestination");
        pw.print("=");
        pw.println(getUploadDestination());

        pw.print(prefix + "  ");
        pw.print("isUploadDestinationGcs");
        pw.print("=");
        pw.println(isUploadDestinationGcs());
    }
}
