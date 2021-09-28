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

package com.android.internal.net.ipsec.ike;

import android.content.Context;
import android.net.eap.EapSessionConfig;
import android.os.Looper;

import com.android.internal.net.eap.EapAuthenticator;
import com.android.internal.net.eap.IEapCallback;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;

/** Package private factory for building EapAuthenticator instances. */
final class IkeEapAuthenticatorFactory {
    /**
     * Builds and returns a new EapAuthenticator
     *
     * @param looper Looper for running a message loop
     * @param cb IEapCallback for callbacks to the client
     * @param context Context for the EapAuthenticator
     * @param eapSessionConfig EAP session configuration
     * @param randomnessFactory the randomness factory
     */
    public EapAuthenticator newEapAuthenticator(
            Looper looper,
            IEapCallback cb,
            Context context,
            EapSessionConfig eapSessionConfig,
            RandomnessFactory randomnessFactory) {
        return new EapAuthenticator(looper, cb, context, eapSessionConfig, randomnessFactory);
    }
}
