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

package io.opencensus.contrib.http.util;

import io.opencensus.trace.propagation.TextFormat;

/**
 * Utility class to get all supported {@link TextFormat}.
 *
 * @since 0.11.0
 */
public class HttpPropagationUtil {

  private HttpPropagationUtil() {}

  /**
   * Returns the Stack Driver format implementation. The header specification for this format is
   * "X-Cloud-Trace-Context: &lt;TRACE_ID&gt;/&lt;SPAN_ID&gt;[;o=&lt;TRACE_TRUE&gt;]". See this <a
   * href="https://cloud.google.com/trace/docs/support">page</a> for more information.
   *
   * @since 0.11.0
   * @return the Stack Driver format.
   */
  public static TextFormat getCloudTraceFormat() {
    return new CloudTraceFormat();
  }
}
