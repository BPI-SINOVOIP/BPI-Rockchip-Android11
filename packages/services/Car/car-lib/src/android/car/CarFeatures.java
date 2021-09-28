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

package android.car;

import android.annotation.NonNull;
import android.car.annotation.OptionalFeature;
import android.os.RemoteException;
import android.util.ArrayMap;

import com.android.internal.annotations.GuardedBy;

/**
 * This class declares all car features which does not have API as {@code String}.
 *
 * <p>Note that all {@code Car*Managers'} feature string is their service name
 * {@code Car.*_SERVICE.}
 * For features with APIs, subfeature {@code String} will be also defined inside the API.
 *
 * <p>To prevent potential conflict in feature / subfeature name, all feature name should use
 * implementation package name space like {@code com.android.car.user.FeatureA} unless it is
 * {@code Car.*_SERVICE}.
 *
 * <p>To define a subfeature, main feature should be already declared and sub feature name should
 * have the format of "main_feature/sub_feature_name". Note that feature name cannot use '/' and
 * should use only alphabet, digit, '_', '-' and '.'.
 *
 * @hide
 */
public final class CarFeatures {
    /**
     * Service to show initial user notice screen. This feature has no API and thus defined here.
     * @hide */
    @OptionalFeature
    public static String FEATURE_CAR_USER_NOTICE_SERVICE =
            "com.android.car.user.CarUserNoticeService";

    // Local cache for making feature query fast.
    // Key: feature name, value: supported or not.
    @GuardedBy("mCachedFeatures")
    private final ArrayMap<String, Boolean> mCachedFeatures = new ArrayMap<>();

    /** @hide */
    boolean isFeatureEnabled(@NonNull ICar service, @NonNull String featureName) {
        synchronized (mCachedFeatures) {
            Boolean supported = mCachedFeatures.get(featureName);
            if (supported != null) {
                return supported;
            }
        }
        // Need to fetch from car service. This should happen only once
        try {
            boolean supported = service.isFeatureEnabled(featureName);
            synchronized (mCachedFeatures) {
                mCachedFeatures.put(featureName, Boolean.valueOf(supported));
            }
            return supported;
        } catch (RemoteException e) {
            // car service has crashed. return false.
        }
        return false;
    }

    /** @hide */
    void resetCache() {
        synchronized (mCachedFeatures) {
            mCachedFeatures.clear();
        }
    }
}
