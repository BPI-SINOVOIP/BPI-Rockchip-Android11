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

import java.util.HashMap;
import java.util.Map;

/**
 * Helper that gives the provider associated to a particular token, in order to find out if a device
 * supports the token.
 */
public class TokenProviderHelper {

    private static Map<TokenProperty, ITokenProvider> sHandlerMap = new HashMap<>();

    static {
        sHandlerMap.put(TokenProperty.SIM_CARD, new TelephonyTokenProvider());
        sHandlerMap.put(TokenProperty.UICC_SIM_CARD, new TelephonyTokenProvider());
        sHandlerMap.put(TokenProperty.SECURE_ELEMENT_SIM_CARD, new TelephonyTokenProvider());
    }

    /** Returns the {@link ITokenProvider} associated with the requested token. */
    public static ITokenProvider getTokenProvider(TokenProperty param) {
        return sHandlerMap.get(param);
    }
}
