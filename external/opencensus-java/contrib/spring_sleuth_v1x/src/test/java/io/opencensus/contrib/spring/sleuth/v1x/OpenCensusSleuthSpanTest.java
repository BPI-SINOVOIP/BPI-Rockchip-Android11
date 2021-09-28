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

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.springframework.cloud.sleuth.Span;

/** Unit tests for {@link OpenCensusSleuthSpan}. */
@RunWith(JUnit4.class)
public class OpenCensusSleuthSpanTest {
  @Test
  public void testFromSleuthSampled() {
    Span sleuthSpan =
        Span.builder()
            .name("name")
            .traceIdHigh(12L)
            .traceId(22L)
            .spanId(23L)
            .exportable(true)
            .build();
    assertSpanEquals(new OpenCensusSleuthSpan(sleuthSpan), sleuthSpan);
  }

  @Test
  public void testFromSleuthNotSampled() {
    Span sleuthSpan =
        Span.builder()
            .name("name")
            .traceIdHigh(12L)
            .traceId(22L)
            .spanId(23L)
            .exportable(false)
            .build();
    assertSpanEquals(new OpenCensusSleuthSpan(sleuthSpan), sleuthSpan);
  }

  private static final void assertSpanEquals(io.opencensus.trace.Span span, Span sleuthSpan) {
    assertThat(span.getContext().isValid()).isTrue();
    assertThat(Long.parseLong(span.getContext().getTraceId().toLowerBase16().substring(0, 16), 16))
        .isEqualTo(sleuthSpan.getTraceIdHigh());
    assertThat(Long.parseLong(span.getContext().getTraceId().toLowerBase16().substring(16, 32), 16))
        .isEqualTo(sleuthSpan.getTraceId());
    assertThat(Long.parseLong(span.getContext().getSpanId().toLowerBase16(), 16))
        .isEqualTo(sleuthSpan.getSpanId());
    assertThat(span.getContext().getTraceOptions().isSampled())
        .isEqualTo(sleuthSpan.isExportable());
  }
}
