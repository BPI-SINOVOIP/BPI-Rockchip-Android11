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
package android.automotive.computepipe.registry;

import android.automotive.computepipe.registry.IClientInfo;
import android.automotive.computepipe.runner.IPipeRunner;

/**
 * Provides mechanism for pipe/graph runner discovery
 */
@VintfStability
interface IPipeQuery {
    /**
     * A client will lookup the registered graphs using this method
     * The registry implementation will return all the graphs registered.
     * The registration is a one time event.
     *
     * @param out get supported graphs by name
     */
    @utf8InCpp String[] getGraphList();

    /**
     * Returns the graph runner for a specific graph
     *
     * Once the client has found the graph it is interested in using
     * getGraphList(), it will use this to retrieve the runner for that graph.
     * It is possible that between the GraphList retrieval and the invocation of
     * this method the runner for this graph has gone down. In which case appropriate binder
     * status will be used to report such an event.
     *
     * @param: graphId graph name for which corresponding runner is sought.
     * @param: info retrieve client information for match purposes when needed
     * @return handle to interact with specific graph
     */
    IPipeRunner getPipeRunner(in @utf8InCpp String graphName, in IClientInfo info);
}
