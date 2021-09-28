/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tradefed.invoker.shard.token;

import com.android.tradefed.device.ITestDevice;

/**
 * Interface describing an object that can provide the tokens of a particular device. Each
 * implementation can check one or several tokens.
 *
 * <p>A token is a special property present on the particular device.
 *
 * <p>Tokens are used during sharding to ensure tests requesting a particular tokens are run against
 * the device providing the token.
 */
public interface ITokenProvider {

    /**
     * Query and return whether or not the device has a particular token.
     *
     * @param device Device queried for its tokens.
     * @param token The token to check
     * @return True if the device has the token, false otherwise.
     */
    boolean hasToken(ITestDevice device, TokenProperty token);
}
