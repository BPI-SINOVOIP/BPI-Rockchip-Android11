/*
 * Copyright 2020 The Android Open Source Project
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

package android.hardware.keymaster;

/**
 * Hardware authentication type, used by HardwareAuthTokens to specify the mechanism used to
 * authentiate the user, and in KeyCharacteristics to specify the allowable mechanisms for
 * authenticating to activate a key.
 */
@VintfStability
@Backing(type="int")
enum HardwareAuthenticatorType {
    NONE = 0,
    PASSWORD = 1 << 0,
    FINGERPRINT = 1 << 1,
    // Additional entries must be powers of 2.
    ANY = 0xFFFFFFFF,
}
