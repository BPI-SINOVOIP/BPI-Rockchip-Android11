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

package com.android.networkstack.apishim.api29;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import com.android.networkstack.apishim.common.CaptivePortalDataShim;
import com.android.networkstack.apishim.common.UnsupportedApiLevelException;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Compatibility implementation of {@link CaptivePortalDataShim}.
 *
 * <p>Use {@link com.android.networkstack.apishim.CaptivePortalDataShimImpl} instead of this
 * fallback implementation.
 */
public abstract class CaptivePortalDataShimImpl implements CaptivePortalDataShim {
    protected CaptivePortalDataShimImpl() {}

    /**
     * Parse a {@link android.net.CaptivePortalData} from JSON.
     *
     * <p>Use
     * {@link com.android.networkstack.apishim.CaptivePortalDataShimImpl#fromJson(JSONObject)}
     * instead of this API 29 compatibility version.
     */
    @NonNull
    public static CaptivePortalDataShim fromJson(JSONObject object) throws JSONException,
            UnsupportedApiLevelException {
        // Data class not supported in API 29
        throw new UnsupportedApiLevelException("CaptivePortalData not supported on API 29");
    }

    @VisibleForTesting
    public static boolean isSupported() {
        return false;
    }
}
