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

package io.opencensus.implcore.trace.propagation;

import static com.google.common.truth.Truth.assertThat;
import static io.opencensus.implcore.trace.propagation.B3Format.X_B3_FLAGS;
import static io.opencensus.implcore.trace.propagation.B3Format.X_B3_PARENT_SPAN_ID;
import static io.opencensus.implcore.trace.propagation.B3Format.X_B3_SAMPLED;
import static io.opencensus.implcore.trace.propagation.B3Format.X_B3_SPAN_ID;
import static io.opencensus.implcore.trace.propagation.B3Format.X_B3_TRACE_ID;

import io.opencensus.trace.SpanContext;
import io.opencensus.trace.SpanId;
import io.opencensus.trace.TraceId;
import io.opencensus.trace.TraceOptions;
import io.opencensus.trace.propagation.SpanContextParseException;
import io.opencensus.trace.propagation.TextFormat.Getter;
import io.opencensus.trace.propagation.TextFormat.Setter;
import java.util.HashMap;
import java.util.Map;
import javax.annotation.Nullable;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link B3Format}. */
@RunWith(JUnit4.class)
public class B3FormatTest {
  private static final String TRACE_ID_BASE16 = "ff000000000000000000000000000041";
  private static final TraceId TRACE_ID = TraceId.fromLowerBase16(TRACE_ID_BASE16);
  private static final String TRACE_ID_BASE16_EIGHT_BYTES = "0000000000000041";
  private static final TraceId TRACE_ID_EIGHT_BYTES =
      TraceId.fromLowerBase16("0000000000000000" + TRACE_ID_BASE16_EIGHT_BYTES);
  private static final String SPAN_ID_BASE16 = "ff00000000000041";
  private static final SpanId SPAN_ID = SpanId.fromLowerBase16(SPAN_ID_BASE16);
  private static final byte TRACE_OPTIONS_BYTE = 1;
  private static final TraceOptions TRACE_OPTIONS = TraceOptions.fromByte(TRACE_OPTIONS_BYTE);
  private static final Setter<Map<String, String>> setter =
      new Setter<Map<String, String>>() {
        @Override
        public void put(Map<String, String> carrier, String key, String value) {
          carrier.put(key, value);
        }
      };
  private static final Getter<Map<String, String>> getter =
      new Getter<Map<String, String>>() {
        @Nullable
        @Override
        public String get(Map<String, String> carrier, String key) {
          return carrier.get(key);
        }
      };
  private final B3Format b3Format = new B3Format();
  @Rule public ExpectedException thrown = ExpectedException.none();

  @Test
  public void serialize_SampledContext() {
    Map<String, String> carrier = new HashMap<String, String>();
    b3Format.inject(SpanContext.create(TRACE_ID, SPAN_ID, TRACE_OPTIONS), carrier, setter);
    assertThat(carrier)
        .containsExactly(
            X_B3_TRACE_ID, TRACE_ID_BASE16, X_B3_SPAN_ID, SPAN_ID_BASE16, X_B3_SAMPLED, "1");
  }

  @Test
  public void serialize_NotSampledContext() {
    Map<String, String> carrier = new HashMap<String, String>();
    b3Format.inject(SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.DEFAULT), carrier, setter);
    assertThat(carrier)
        .containsExactly(X_B3_TRACE_ID, TRACE_ID_BASE16, X_B3_SPAN_ID, SPAN_ID_BASE16);
  }

  @Test
  public void parseMissingSampledAndMissingFlag() throws SpanContextParseException {
    Map<String, String> headersNotSampled = new HashMap<String, String>();
    headersNotSampled.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    headersNotSampled.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    SpanContext spanContext = SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.DEFAULT);
    assertThat(b3Format.extract(headersNotSampled, getter)).isEqualTo(spanContext);
  }

  @Test
  public void parseSampled() throws SpanContextParseException {
    Map<String, String> headersSampled = new HashMap<String, String>();
    headersSampled.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    headersSampled.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    headersSampled.put(X_B3_SAMPLED, "1");
    assertThat(b3Format.extract(headersSampled, getter))
        .isEqualTo(SpanContext.create(TRACE_ID, SPAN_ID, TRACE_OPTIONS));
  }

  @Test
  public void parseZeroSampled() throws SpanContextParseException {
    Map<String, String> headersNotSampled = new HashMap<String, String>();
    headersNotSampled.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    headersNotSampled.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    headersNotSampled.put(X_B3_SAMPLED, "0");
    assertThat(b3Format.extract(headersNotSampled, getter))
        .isEqualTo(SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.DEFAULT));
  }

  @Test
  public void parseFlag() throws SpanContextParseException {
    Map<String, String> headersFlagSampled = new HashMap<String, String>();
    headersFlagSampled.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    headersFlagSampled.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    headersFlagSampled.put(X_B3_FLAGS, "1");
    assertThat(b3Format.extract(headersFlagSampled, getter))
        .isEqualTo(SpanContext.create(TRACE_ID, SPAN_ID, TRACE_OPTIONS));
  }

  @Test
  public void parseZeroFlag() throws SpanContextParseException {
    Map<String, String> headersFlagNotSampled = new HashMap<String, String>();
    headersFlagNotSampled.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    headersFlagNotSampled.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    headersFlagNotSampled.put(X_B3_FLAGS, "0");
    assertThat(b3Format.extract(headersFlagNotSampled, getter))
        .isEqualTo(SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.DEFAULT));
  }

  @Test
  public void parseEightBytesTraceId() throws SpanContextParseException {
    Map<String, String> headersEightBytes = new HashMap<String, String>();
    headersEightBytes.put(X_B3_TRACE_ID, TRACE_ID_BASE16_EIGHT_BYTES);
    headersEightBytes.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    headersEightBytes.put(X_B3_SAMPLED, "1");
    assertThat(b3Format.extract(headersEightBytes, getter))
        .isEqualTo(SpanContext.create(TRACE_ID_EIGHT_BYTES, SPAN_ID, TRACE_OPTIONS));
  }

  @Test
  public void parseEightBytesTraceId_NotSampledSpanContext() throws SpanContextParseException {
    Map<String, String> headersEightBytes = new HashMap<String, String>();
    headersEightBytes.put(X_B3_TRACE_ID, TRACE_ID_BASE16_EIGHT_BYTES);
    headersEightBytes.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    assertThat(b3Format.extract(headersEightBytes, getter))
        .isEqualTo(SpanContext.create(TRACE_ID_EIGHT_BYTES, SPAN_ID, TraceOptions.DEFAULT));
  }

  @Test
  public void parseInvalidTraceId() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_TRACE_ID, "abcdefghijklmnop");
    invalidHeaders.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Invalid input.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void parseInvalidTraceId_Size() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_TRACE_ID, "0123456789abcdef00");
    invalidHeaders.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Invalid input.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void parseMissingTraceId() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_SPAN_ID, SPAN_ID_BASE16);
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Missing X_B3_TRACE_ID.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void parseInvalidSpanId() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    invalidHeaders.put(X_B3_SPAN_ID, "abcdefghijklmnop");
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Invalid input.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void parseInvalidSpanId_Size() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    invalidHeaders.put(X_B3_SPAN_ID, "0123456789abcdef00");
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Invalid input.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void parseMissingSpanId() throws SpanContextParseException {
    Map<String, String> invalidHeaders = new HashMap<String, String>();
    invalidHeaders.put(X_B3_TRACE_ID, TRACE_ID_BASE16);
    thrown.expect(SpanContextParseException.class);
    thrown.expectMessage("Missing X_B3_SPAN_ID.");
    b3Format.extract(invalidHeaders, getter);
  }

  @Test
  public void fields_list() {
    assertThat(b3Format.fields())
        .containsExactly(
            X_B3_TRACE_ID, X_B3_SPAN_ID, X_B3_PARENT_SPAN_ID, X_B3_SAMPLED, X_B3_FLAGS);
  }
}
