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

package io.opencensus.trace.propagation;

import static com.google.common.truth.Truth.assertThat;

import io.opencensus.trace.SpanContext;
import java.text.ParseException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link BinaryFormat}. */
@RunWith(JUnit4.class)
public class BinaryFormatTest {
  private static final BinaryFormat binaryFormat = BinaryFormat.getNoopBinaryFormat();

  @Test(expected = NullPointerException.class)
  public void toBinaryValue_NullSpanContext() {
    binaryFormat.toBinaryValue(null);
  }

  @Test
  public void toBinaryValue_NotNullSpanContext() {
    assertThat(binaryFormat.toBinaryValue(SpanContext.INVALID)).isEqualTo(new byte[0]);
  }

  @Test(expected = NullPointerException.class)
  public void toByteArray_NullSpanContext() {
    binaryFormat.toByteArray(null);
  }

  @Test
  public void toByteArray_NotNullSpanContext() {
    assertThat(binaryFormat.toByteArray(SpanContext.INVALID)).isEqualTo(new byte[0]);
  }

  @Test(expected = NullPointerException.class)
  public void fromBinaryValue_NullInput() throws ParseException {
    binaryFormat.fromBinaryValue(null);
  }

  @Test
  public void fromBinaryValue_NotNullInput() throws ParseException {
    assertThat(binaryFormat.fromBinaryValue(new byte[0])).isEqualTo(SpanContext.INVALID);
  }

  @Test(expected = NullPointerException.class)
  public void fromByteArray_NullInput() throws SpanContextParseException {
    binaryFormat.fromByteArray(null);
  }

  @Test
  public void fromByteArray_NotNullInput() throws SpanContextParseException {
    assertThat(binaryFormat.fromByteArray(new byte[0])).isEqualTo(SpanContext.INVALID);
  }
}
