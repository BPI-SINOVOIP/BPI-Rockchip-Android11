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

package com.android.car.dialer;

/** Dialer constants. */
public final class Constants {
    private Constants() {
    }

    /** Constants used to build {@link android.content.Intent}s. */
    public static class Intents {
        /** Intent extra flag to show incoming call. */
        public static final String EXTRA_SHOW_INCOMING_CALL = "show_incoming_call";

        public static final String EXTRA_SHOW_DIALPAD = "show_dialpad";
    }

    /** Constants used by {@link androidx.core.app.JobIntentService}s. */
    public static class JobIds {
        public static final int NOTIFICATION_SERVICE = 2019;
    }

    /** See {@link android.content.res.Resources#getIdentifier(String, String, String)} */
    public static int INVALID_RES_ID = 0;

    /** See {@link com.android.bluetooth.hfpclient.connserv.HfpClientConnectionService}. */
    public static final String HFP_CLIENT_CONNECTION_SERVICE_CLASS_NAME =
            "com.android.bluetooth.hfpclient.connserv.HfpClientConnectionService";
}
