/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// INetworkInterceptService.aidl
package com.google.android.setupwizard.util;

/**
 * Provides functions for enabling and disabling network intents interception
 * of setupwizard.
 *
 * During init setup flow, "SecureInterceptActivity" and "WebDialogActivity"
 * are enabled to intercept network intents. By disabling these two activities
 * the network intent will be allowed in setup flow.
 */
interface INetworkInterceptService {
    /**
     * Disables the network intents intercept and returns
     * true if disable the network interception successfully.
     */
    boolean disableNetworkIntentIntercept();

    /**
     * Enables the  network intents intercept and returns
     * true if enable the network interception successfully.
     */
    boolean enableNetworkIntentIntercept();

    /**
     * Returns true if setupwizard intercept the network intents.
     */
    boolean isNetworkIntentIntercepted();
}