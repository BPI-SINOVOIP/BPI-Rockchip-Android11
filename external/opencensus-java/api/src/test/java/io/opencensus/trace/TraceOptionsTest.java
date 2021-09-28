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

package io.opencensus.trace;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.testing.EqualsTester;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link TraceOptions}. */
@RunWith(JUnit4.class)
public class TraceOptionsTest {
  private static final byte FIRST_BYTE = (byte) 0xff;
  private static final byte SECOND_BYTE = 1;
  private static final byte THIRD_BYTE = 6;

  @Test
  public void getOptions() {
    assertThat(TraceOptions.DEFAULT.getOptions()).isEqualTo(0);
    assertThat(TraceOptions.builder().setIsSampled(false).build().getOptions()).isEqualTo(0);
    assertThat(TraceOptions.builder().setIsSampled(true).build().getOptions()).isEqualTo(1);
    assertThat(TraceOptions.builder().setIsSampled(true).setIsSampled(false).build().getOptions())
        .isEqualTo(0);
    assertThat(TraceOptions.fromByte(FIRST_BYTE).getOptions()).isEqualTo(-1);
    assertThat(TraceOptions.fromByte(SECOND_BYTE).getOptions()).isEqualTo(1);
    assertThat(TraceOptions.fromByte(THIRD_BYTE).getOptions()).isEqualTo(6);
  }

  @Test
  public void isSampled() {
    assertThat(TraceOptions.DEFAULT.isSampled()).isFalse();
    assertThat(TraceOptions.builder().setIsSampled(true).build().isSampled()).isTrue();
  }

  @Test
  public void toFromByte() {
    assertThat(TraceOptions.fromByte(FIRST_BYTE).getByte()).isEqualTo(FIRST_BYTE);
    assertThat(TraceOptions.fromByte(SECOND_BYTE).getByte()).isEqualTo(SECOND_BYTE);
    assertThat(TraceOptions.fromByte(THIRD_BYTE).getByte()).isEqualTo(THIRD_BYTE);
  }

  @Test
  @SuppressWarnings("deprecation")
  public void deprecated_fromBytes() {
    assertThat(TraceOptions.fromBytes(new byte[] {FIRST_BYTE}).getByte()).isEqualTo(FIRST_BYTE);
    assertThat(TraceOptions.fromBytes(new byte[] {1, FIRST_BYTE}, 1).getByte())
        .isEqualTo(FIRST_BYTE);
  }

  @Test
  @SuppressWarnings("deprecation")
  public void deprecated_getBytes() {
    assertThat(TraceOptions.fromByte(FIRST_BYTE).getBytes()).isEqualTo(new byte[] {FIRST_BYTE});
  }

  @Test
  public void builder_FromOptions() {
    assertThat(
            TraceOptions.builder(TraceOptions.fromByte(THIRD_BYTE))
                .setIsSampled(true)
                .build()
                .getOptions())
        .isEqualTo(6 | 1);
  }

  @Test
  public void traceOptions_EqualsAndHashCode() {
    EqualsTester tester = new EqualsTester();
    tester.addEqualityGroup(TraceOptions.DEFAULT);
    tester.addEqualityGroup(
        TraceOptions.fromByte(SECOND_BYTE), TraceOptions.builder().setIsSampled(true).build());
    tester.addEqualityGroup(TraceOptions.fromByte(FIRST_BYTE));
    tester.testEquals();
  }

  @Test
  public void traceOptions_ToString() {
    assertThat(TraceOptions.DEFAULT.toString()).contains("sampled=false");
    assertThat(TraceOptions.builder().setIsSampled(true).build().toString())
        .contains("sampled=true");
  }
}
