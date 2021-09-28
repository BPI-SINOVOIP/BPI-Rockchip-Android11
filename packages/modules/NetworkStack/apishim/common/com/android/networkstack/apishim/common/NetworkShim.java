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

package com.android.networkstack.apishim.common;

/**
 * Interface used to access API methods in {@link android.net.Network}, with appropriate fallbacks
 * if the methods are not yet part of the released API.
 *
 * <p>This interface makes it easier for callers to use NetworkShimImpl, as it's more obvious what
 * methods must be implemented on each API level, and it abstracts from callers the need to
 * reference classes that have different implementations (which also does not work well with IDEs).
 */
public interface NetworkShim {
    /**
     * @see android.net.Network.netId.
     *
     * @throws UnsupportedApiLevelException if API is not available in the API level.
     */
    int getNetId() throws UnsupportedApiLevelException;
}
