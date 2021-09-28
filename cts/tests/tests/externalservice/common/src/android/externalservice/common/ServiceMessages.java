/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.externalservice.common;

import android.os.IBinder;

public class ServiceMessages {
    // No arguments. Gets the UID and PID of the service.
    public static final int MSG_IDENTIFY = IBinder.FIRST_CALL_TRANSACTION + 1;
    // Response to MSG_IDENTIFY. Bundle data contain a single key, noted below.
    public static final int MSG_IDENTIFY_RESPONSE = MSG_IDENTIFY + 100;

    // Bundle key in MSG_IDENTIFY_RESPONSE containing the RunningServiceInfo object.
    public static final String IDENTIFY_INFO = "serviceInfo";

    // Starts an external service. arg1 should be 1 if the process should be started via
    // an app zygote, and 0 if the service should be started normally.
    public static final int MSG_CREATE_EXTERNAL_SERVICE = IBinder.FIRST_CALL_TRANSACTION + 2;
    // Responds to MSG_CREATE_EXTERNAL_SERVICE. obj is the IBinder on success, null on failure.
    public static final int MSG_CREATE_EXTERNAL_SERVICE_RESPONSE =
        MSG_CREATE_EXTERNAL_SERVICE + 100;

    private ServiceMessages() {}
}
