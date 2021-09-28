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

package com.android.internal.net.eap;

import static android.telephony.TelephonyManager.APPTYPE_USIM;

import android.net.eap.EapSessionConfig;

import java.util.HashMap;

/**
 * EapTestUtils is a util class for providing test-values of EAP-related objects.
 */
public class EapTestUtils {
    /**
     * Creates and returns a dummy EapSessionConfig instance.
     *
     * @return a new, empty EapSessionConfig instance
     */
    public static EapSessionConfig getDummyEapSessionConfig() {
        return new EapSessionConfig(new HashMap<>(), new byte[0]);
    }

    /**
     * Creates and returns a dummy EapSessionConfig instance with the given EAP-Identity.
     *
     * @param eapIdentity byte-array representing the EAP-Identity of the client
     * @return a new, empty EapSessionConfig instance with the given EAP-Identity
     */
    public static EapSessionConfig getDummyEapSessionConfig(byte[] eapIdentity) {
        return new EapSessionConfig(new HashMap<>(), eapIdentity);
    }

    /**
     * Creates and returns a dummy EapSessionConfig instance with EAP-SIM configured.
     *
     * @return a new EapSessionConfig with EAP-SIM configs set
     */
    public static EapSessionConfig getDummyEapSimSessionConfig() {
        return new EapSessionConfig.Builder().setEapSimConfig(0, APPTYPE_USIM).build();
    }
}
