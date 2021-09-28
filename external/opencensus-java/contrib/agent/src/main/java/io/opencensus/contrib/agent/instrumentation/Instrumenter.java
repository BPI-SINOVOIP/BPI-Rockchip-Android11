/*
 * Copyright 2017, OpenCensus Authors
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

package io.opencensus.contrib.agent.instrumentation;

import io.opencensus.contrib.agent.Settings;
import net.bytebuddy.agent.builder.AgentBuilder;

/**
 * Interface for plug-ins that add bytecode instrumentation.
 *
 * @since 0.6
 */
public interface Instrumenter {

  /**
   * Adds bytecode instrumentation to the given {@link AgentBuilder}.
   *
   * @param agentBuilder an {@link AgentBuilder} object to which the additional instrumentation is
   *     added
   * @param settings the configuration settings
   * @return an {@link AgentBuilder} object having the additional instrumentation
   * @since 0.10
   */
  AgentBuilder instrument(AgentBuilder agentBuilder, Settings settings);
}
