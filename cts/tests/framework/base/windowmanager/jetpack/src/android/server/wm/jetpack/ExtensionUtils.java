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

package android.server.wm.jetpack;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeFalse;

import android.content.Context;
import android.server.wm.jetpack.wrapper.TestDeviceState;
import android.server.wm.jetpack.wrapper.extensionwrapperimpl.TestExtensionCompat;
import android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarCompat;
import android.server.wm.jetpack.wrapper.TestInterfaceCompat;
import android.server.wm.jetpack.wrapper.sidecarwrapperimpl.TestSidecarDeviceState;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.window.extensions.ExtensionProvider;
import androidx.window.sidecar.SidecarProvider;

/** Helper class to get the vendor provided Extension/Sidecar implementation. */
final class ExtensionUtils {
    private static final String TAG = "TestInterfaceProvider";

    /** Skips devices that don't implement the wm extension library. */
    static void assumeSupportedDevice(Context context) {
        assumeFalse(TextUtils.isEmpty(getVersion()) && getInterfaceCompat(context) == null);
    }

    /** Asserts that the vendor provided version is in the correct format and range. */
    static void assertValidVersion() {
        if (getExtensionVersion() != null) {
            String versionStr = getExtensionVersion();
            assertThat(Version.isValidVersion(versionStr)).isTrue();
            assertThat(Version.parse(versionStr)).isAtLeast(Version.VERSION_1_0);
        } else if (getSidecarVersion() != null) {
            String versionStr = getSidecarVersion();
            assertThat(Version.isValidVersion(versionStr)).isTrue();
            assertThat(Version.parse(versionStr)).isEqualTo(Version.VERSION_0_1);
        }
    }

    static void assertEqualsState(TestDeviceState left, TestDeviceState right) {
        if (left instanceof TestSidecarDeviceState && right instanceof TestSidecarDeviceState) {
            assertThat(left.getPosture()).isEqualTo(right.getPosture());
        } else {
            assertThat(left).isEqualTo(right);
        }
    }

    /**
     * Gets the vendor provided Extension implementation if available. If not available, gets the
     * Sidecar implementation (deprecated). If neither is available, returns {@code null}.
     */
    @Nullable
    static TestInterfaceCompat getInterfaceCompat(Context context) {
        // TODO(b/158876142) Reinstate android.window.extension
        if (!TextUtils.isEmpty(getSidecarVersion())) {
            return getSidecarInterfaceCompat(context);
        }
        return null;
    }

    @Nullable
    private static String getVersion() {
        if (!TextUtils.isEmpty(getExtensionVersion())) {
            return getExtensionVersion();
        } else if (!TextUtils.isEmpty(getSidecarVersion())) {
            return getSidecarVersion();
        }
        return null;
    }

    @Nullable
    private static String getExtensionVersion() {
        try {
            return ExtensionProvider.getApiVersion();
        } catch (NoClassDefFoundError e) {
            Log.d(TAG, "Extension version not found");
            return null;
        } catch (UnsupportedOperationException e) {
            Log.d(TAG, "Stub Extension");
            return null;
        }
    }

    @Nullable
    private static TestExtensionCompat getExtensionInterfaceCompat(Context context) {
        try {
            return TestExtensionCompat.create(ExtensionProvider.getExtensionImpl(context));
        } catch (NoClassDefFoundError e) {
            Log.d(TAG, "Extension implementation not found");
            return null;
        } catch (UnsupportedOperationException e) {
            Log.d(TAG, "Stub Extension");
            return null;
        }
    }

    @SuppressWarnings("deprecation")
    @Nullable
    private static String getSidecarVersion() {
        try {
            return SidecarProvider.getApiVersion();
        } catch (NoClassDefFoundError e) {
            Log.d(TAG, "Sidecar version not found");
            return null;
        } catch (UnsupportedOperationException e) {
            Log.d(TAG, "Stub Sidecar");
            return null;
        }
    }

    @SuppressWarnings("deprecation")
    @Nullable
    private static TestSidecarCompat getSidecarInterfaceCompat(Context context) {
        try {
            return TestSidecarCompat.create(SidecarProvider.getSidecarImpl(context));
        } catch (NoClassDefFoundError e) {
            Log.d(TAG, "Sidecar implementation not found");
            return null;
        } catch (UnsupportedOperationException e) {
            Log.d(TAG, "Stub Sidecar");
            return null;
        }
    }

    private ExtensionUtils() {}
}
