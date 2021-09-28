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
package com.android.internal.net.ipsec.ike.utils;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.eap.EapAuthenticator.EapRandomFactory;
import com.android.internal.net.ipsec.ike.testmode.DeterministicSecureRandom;

import java.security.SecureRandom;

/** Factory class that creates a DeterministicSecureRandom when test mode is enabled */
@VisibleForTesting
public class RandomnessFactory implements EapRandomFactory {
    // This constant is mirrored of android.net.NetworkCapabilities.TRANSPORT_TEST due to lack of
    // @TestApi guarantees in mainline modules
    public static final int TRANSPORT_TEST = 7;

    private final boolean mIsTestModeEnabled;

    public RandomnessFactory(Context context, Network network) {
        ConnectivityManager connectManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkCapabilities networkCapabilities = connectManager.getNetworkCapabilities(network);

        mIsTestModeEnabled =
                networkCapabilities != null && networkCapabilities.hasTransport(TRANSPORT_TEST);
    }

    /**
     * Returns a DeterministicSecureRandom instance when test mode is enabled.
     *
     * <p>Returns a DeterministicSecureRandom instance when test mode is enabled, otherwise returns
     * null
     *
     * <p>TODO(b/154941518): figure out how to let this method always return a random without
     * relying on nullability behavior
     */
    @Override
    public SecureRandom getRandom() {
        if (mIsTestModeEnabled) {
            return new DeterministicSecureRandom();
        }

        return null;
    }
}
