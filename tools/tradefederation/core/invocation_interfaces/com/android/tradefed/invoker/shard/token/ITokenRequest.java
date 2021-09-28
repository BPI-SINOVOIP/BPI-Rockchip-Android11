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

import com.android.tradefed.testtype.IRemoteTest;

import java.util.Set;

/**
 * Interface a {@link IRemoteTest} can implement to expose the fact that it requires a device with a
 * special token.
 */
public interface ITokenRequest extends IRemoteTest {

    /** Returns the list of required tokens by the test. Returns null if no token support. */
    public Set<TokenProperty> getRequiredTokens();
}
