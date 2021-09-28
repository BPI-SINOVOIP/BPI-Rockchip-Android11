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
package android.contentcaptureservice.cts;

import static android.content.Context.CONTENT_CAPTURE_MANAGER_SERVICE;
import static android.contentcaptureservice.cts.Helper.RESOURCE_STRING_SERVICE_NAME;
import static android.contentcaptureservice.cts.Helper.getInternalString;
import static android.contentcaptureservice.cts.Helper.sContext;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeTrue;

import android.provider.DeviceConfig;
import android.text.TextUtils;
import android.util.Log;
import android.view.contentcapture.ContentCaptureManager;

import com.android.compatibility.common.util.DeviceConfigStateManager;
import com.android.compatibility.common.util.RequiredServiceRule;

import org.junit.Test;

/**
 * No-op test used to make sure the other tests are not passing because the feature is disabled.
 */
public class CanaryTest {

    private static final String TAG = CanaryTest.class.getSimpleName();

    @Test
    public void logHasService() {
        final boolean hasService = RequiredServiceRule.hasService(CONTENT_CAPTURE_MANAGER_SERVICE);
        Log.d(TAG, "has " + CONTENT_CAPTURE_MANAGER_SERVICE + ": " + hasService);
        assumeTrue("device doesn't have service " + CONTENT_CAPTURE_MANAGER_SERVICE, hasService);
    }

    @Test
    public void assertHasService() {
        final String serviceName = getInternalString(RESOURCE_STRING_SERVICE_NAME);
        final String enableSettings = new DeviceConfigStateManager(sContext,
                DeviceConfig.NAMESPACE_CONTENT_CAPTURE,
                ContentCaptureManager.DEVICE_CONFIG_PROPERTY_SERVICE_EXPLICITLY_ENABLED).get();
        final boolean hasService = RequiredServiceRule.hasService(CONTENT_CAPTURE_MANAGER_SERVICE);
        Log.d(TAG, "Service resource: '" + serviceName + "' Settings: '" + enableSettings
                + "' Has '" + CONTENT_CAPTURE_MANAGER_SERVICE + "': " + hasService);

        // We're only asserting when the OEM defines a service
        assumeTrue("service resource (" + serviceName + ") is not defined",
                !TextUtils.isEmpty(serviceName));
        assertWithMessage("Should be enabled when resource '%s' is not empty (settings='%s')",
                serviceName, enableSettings).that(hasService).isTrue();
    }
}
