/*
 * Copyright 2018, OpenCensus Authors
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

package io.opencensus.contrib.spring.sleuth.v1x;

import io.opencensus.common.ExperimentalApi;
import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * Sleuth annotation settings.
 *
 * @since 0.16
 */
@ExperimentalApi
@ConfigurationProperties("spring.opencensus.sleuth")
public class OpenCensusSleuthProperties {

  private boolean enabled = true;

  /** Returns whether OpenCensus trace propagation is enabled. */
  public boolean isEnabled() {
    return this.enabled;
  }

  /** Enables OpenCensus trace propagation. */
  public void setEnabled(boolean enabled) {
    this.enabled = enabled;
  }
}
