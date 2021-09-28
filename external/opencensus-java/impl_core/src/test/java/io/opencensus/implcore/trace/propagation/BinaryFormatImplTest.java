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
import static com.google.common.truth.Truth.assertWithMessage;

import io.opencensus.trace.SpanContext;
import io.opencensus.trace.SpanId;
import io.opencensus.trace.TraceId;
import io.opencensus.trace.TraceOptions;
import io.opencensus.trace.propagation.BinaryFormat;
import io.opencensus.trace.propagation.SpanContextParseException;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link BinaryFormatImpl}. */
@RunWith(JUnit4.class)
public class BinaryFormatImplTest {
  private static final byte[] TRACE_ID_BYTES =
      new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
  private static final TraceId TRACE_ID = TraceId.fromBytes(TRACE_ID_BYTES);
  private static final byte[] SPAN_ID_BYTES = new byte[] {97, 98, 99, 100, 101, 102, 103, 104};
  private static final SpanId SPAN_ID = SpanId.fromBytes(SPAN_ID_BYTES);
  private static final byte TRACE_OPTIONS_BYTES = 1;
  private static final TraceOptions TRACE_OPTIONS = TraceOptions.fromByte(TRACE_OPTIONS_BYTES);
  private static final byte[] EXAMPLE_BYTES =
      new byte[] {
        0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 1, 97, 98, 99, 100,
        101, 102, 103, 104, 2, 1
      };
  private static final SpanContext EXAMPLE_SPAN_CONTEXT =
      SpanContext.create(TRACE_ID, SPAN_ID, TRACE_OPTIONS);
  @Rule public ExpectedException expectedException = ExpectedException.none();
  private final BinaryFormat binaryFormat = new BinaryFormatImpl();

  private void testSpanContextConversion(SpanContext spanContext) throws SpanContextParseException {
    SpanContext propagatedBinarySpanContext =
        binaryFormat.fromByteArray(binaryFormat.toByteArray(spanContext));

    assertWithMessage("BinaryFormat propagated context is not equal with the initial context.")
        .that(propagatedBinarySpanContext)
        .isEqualTo(spanContext);
  }

  @Test
  public void propagate_SpanContextTracingEnabled() throws SpanContextParseException {
    testSpanContextConversion(
        SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.builder().setIsSampled(true).build()));
  }

  @Test
  public void propagate_SpanContextNoTracing() throws SpanContextParseException {
    testSpanContextConversion(SpanContext.create(TRACE_ID, SPAN_ID, TraceOptions.DEFAULT));
  }

  @Test(expected = NullPointerException.class)
  public void toBinaryValue_NullSpanContext() {
    binaryFormat.toByteArray(null);
  }

  @Test
  public void toBinaryValue_InvalidSpanContext() {
    assertThat(binaryFormat.toByteArray(SpanContext.INVALID))
        .isEqualTo(
            new byte[] {
              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0
            });
  }

  @Test
  public void fromBinaryValue_BinaryExampleValue() throws SpanContextParseException {
    assertThat(binaryFormat.fromByteArray(EXAMPLE_BYTES)).isEqualTo(EXAMPLE_SPAN_CONTEXT);
  }

  @Test(expected = NullPointerException.class)
  public void fromBinaryValue_NullInput() throws SpanContextParseException {
    binaryFormat.fromByteArray(null);
  }

  @Test
  public void fromBinaryValue_EmptyInput() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage("Unsupported version.");
    binaryFormat.fromByteArray(new byte[0]);
  }

  @Test
  public void fromBinaryValue_UnsupportedVersionId() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage("Unsupported version.");
    binaryFormat.fromByteArray(
        new byte[] {
          66, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 97, 98, 99, 100, 101,
          102, 103, 104, 1
        });
  }

  @Test
  public void fromBinaryValue_UnsupportedFieldIdFirst() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage(
        "Invalid input: expected trace ID at offset " + BinaryFormatImpl.TRACE_ID_FIELD_ID_OFFSET);
    binaryFormat.fromByteArray(
        new byte[] {
          0, 4, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 1, 97, 98, 99, 100,
          101, 102, 103, 104, 2, 1
        });
  }

  @Test
  public void fromBinaryValue_UnsupportedFieldIdSecond() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage(
        "Invalid input: expected span ID at offset " + BinaryFormatImpl.SPAN_ID_FIELD_ID_OFFSET);
    binaryFormat.fromByteArray(
        new byte[] {
          0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 3, 97, 98, 99, 100,
          101, 102, 103, 104, 2, 1
        });
  }

  @Test
  public void fromBinaryValue_UnsupportedFieldIdThird_skipped() throws SpanContextParseException {
    assertThat(
            binaryFormat
                .fromByteArray(
                    new byte[] {
                      0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 1, 97,
                      98, 99, 100, 101, 102, 103, 104, 0, 1
                    })
                .isValid())
        .isTrue();
  }

  @Test
  public void fromBinaryValue_ShorterTraceId() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage("Invalid input: truncated");
    binaryFormat.fromByteArray(
        new byte[] {0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76});
  }

  @Test
  public void fromBinaryValue_ShorterSpanId() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage("Invalid input: truncated");
    binaryFormat.fromByteArray(new byte[] {0, 1, 97, 98, 99, 100, 101, 102, 103});
  }

  @Test
  public void fromBinaryValue_ShorterTraceOptions() throws SpanContextParseException {
    expectedException.expect(SpanContextParseException.class);
    expectedException.expectMessage("Invalid input: truncated");
    binaryFormat.fromByteArray(
        new byte[] {
          0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 1, 97, 98, 99, 100,
          101, 102, 103, 104, 2
        });
  }

  @Test
  public void fromBinaryValue_MissingTraceOptionsOk() throws SpanContextParseException {
    SpanContext extracted =
        binaryFormat.fromByteArray(
            new byte[] {
              0, 0, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 1, 97, 98, 99,
              100, 101, 102, 103, 104
            });

    assertThat(extracted.isValid()).isTrue();
    assertThat(extracted.getTraceOptions()).isEqualTo(TraceOptions.DEFAULT);
  }
}
