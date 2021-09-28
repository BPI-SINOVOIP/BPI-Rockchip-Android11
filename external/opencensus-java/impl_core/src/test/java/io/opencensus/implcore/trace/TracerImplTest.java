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

package io.opencensus.implcore.trace;

import static com.google.common.truth.Truth.assertThat;

import io.opencensus.implcore.trace.RecordEventsSpanImpl.StartEndHandler;
import io.opencensus.implcore.trace.internal.RandomHandler.SecureRandomHandler;
import io.opencensus.testing.common.TestClock;
import io.opencensus.trace.BlankSpan;
import io.opencensus.trace.SpanBuilder;
import io.opencensus.trace.SpanContext;
import io.opencensus.trace.config.TraceConfig;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/** Unit tests for {@link TracerImpl}. */
@RunWith(JUnit4.class)
public class TracerImplTest {
  private static final String SPAN_NAME = "MySpanName";
  @Mock private StartEndHandler startEndHandler;
  @Mock private TraceConfig traceConfig;
  private TracerImpl tracer;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    tracer =
        new TracerImpl(new SecureRandomHandler(), startEndHandler, TestClock.create(), traceConfig);
  }

  @Test
  public void createSpanBuilder() {
    SpanBuilder spanBuilder = tracer.spanBuilderWithExplicitParent(SPAN_NAME, BlankSpan.INSTANCE);
    assertThat(spanBuilder).isInstanceOf(SpanBuilderImpl.class);
  }

  @Test
  public void createSpanBuilderWithRemoteParet() {
    SpanBuilder spanBuilder = tracer.spanBuilderWithRemoteParent(SPAN_NAME, SpanContext.INVALID);
    assertThat(spanBuilder).isInstanceOf(SpanBuilderImpl.class);
  }
}
