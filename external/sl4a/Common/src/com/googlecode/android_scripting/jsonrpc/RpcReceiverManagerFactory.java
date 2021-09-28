/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.googlecode.android_scripting.jsonrpc;

import java.util.Map;

public interface RpcReceiverManagerFactory {

    /**
     * Creates an {@code RpcReceiverManager} with the key {@param id}.
     *
     * @param sessionId the session id for the new RpcReceiverManager
     * @return the newly created RpcReceiverManager
     */
    RpcReceiverManager create(String sessionId);

    /**
     * Destroys the RpcReceiverManager that corresponds to the given id.
     *
     * @param sessionId the session id
     * @return true if the RpcReceiverManager existed, false otherwise
     */
    boolean destroy(String sessionId);

    /**
     * Returns the map of Session Id -> RpcReceiverManagers.
     * @return
     */
    Map<String, RpcReceiverManager> getRpcReceiverManagers();
}
