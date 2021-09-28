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

package com.android.compatibility.common.util;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.content.Context;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Manages the state of a preference backed by {@link DeviceConfig}.
 */
public final class DeviceConfigStateManager implements StateManager<String> {

    private static final String TAG = DeviceConfigStateManager.class.getSimpleName();

    private final Context mContext;
    private final String mNamespace;
    private final String mKey;

    /**
     * Default constructor.
     *
     * @param context context used to retrieve the {@link Settings} provider.
     * @param namespace settings namespace.
     * @param key prefence key.
     */
    public DeviceConfigStateManager(@NonNull Context context, @NonNull String namespace,
            @NonNull String key) {
        debug("DeviceConfigStateManager", "namespace=%s, key=%s", namespace, key);

        mContext = Objects.requireNonNull(context);
        mNamespace = Objects.requireNonNull(namespace);
        mKey = Objects.requireNonNull(key);
    }

    @Override
    public void set(@Nullable String value) {
        debug("set", "new value is %s", value);
        runWithShellPermissionIdentity(() -> setWithPermissionsGranted(value),
                "android.permission.READ_DEVICE_CONFIG", "android.permission.WRITE_DEVICE_CONFIG");
    }

    private void setWithPermissionsGranted(@Nullable String value) {
        final OneTimeDeviceConfigListener listener = new OneTimeDeviceConfigListener(mNamespace,
                mKey);
        DeviceConfig.addOnPropertiesChangedListener(mNamespace, mContext.getMainExecutor(),
                listener);

        DeviceConfig.setProperty(mNamespace, mKey, value, /* makeDefault= */ false);
        listener.assertCalled();
    }

    @Override
    @Nullable
    public String get() {
        final AtomicReference<String> reference = new AtomicReference<>();
        runWithShellPermissionIdentity(()
                -> reference.set(DeviceConfig.getProperty(mNamespace, mKey)),
                "android.permission.READ_DEVICE_CONFIG");
        debug("get", "returning %s", reference.get());

        return reference.get();
    }

    private void debug(@NonNull String methodName, @NonNull String msg, Object...args) {
        if (!Log.isLoggable(TAG, Log.DEBUG)) return;

        final String prefix = String.format("%s(%s:%s): ", methodName, mNamespace, mKey);
        Log.d(TAG, prefix + String.format(msg, args));
    }

    @Override
    public String toString() {
        return "DeviceConfigStateManager[namespace=" + mNamespace + ", key=" + mKey + "]";
    }
}
