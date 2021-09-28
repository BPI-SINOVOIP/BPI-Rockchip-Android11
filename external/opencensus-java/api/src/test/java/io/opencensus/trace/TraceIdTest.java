/*
 * Copyright 2016-17, OpenCensus Authors
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
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link TraceId}. */
@RunWith(JUnit4.class)
public class TraceIdTest {
  private static final byte[] firstBytes =
      new byte[] {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'a'};
  private static final byte[] secondBytes =
      new byte[] {(byte) 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'A'};
  private static final TraceId first = TraceId.fromBytes(firstBytes);
  private static final TraceId second = TraceId.fromBytes(secondBytes);

  @Test
  public void invalidTraceId() {
    assertThat(TraceId.INVALID.getBytes()).isEqualTo(new byte[16]);
  }

  @Test
  public void isValid() {
    assertThat(TraceId.INVALID.isValid()).isFalse();
    assertThat(first.isValid()).isTrue();
    assertThat(second.isValid()).isTrue();
  }

  @Test
  public void getBytes() {
    assertThat(first.getBytes()).isEqualTo(firstBytes);
    assertThat(second.getBytes()).isEqualTo(secondBytes);
  }

  @Test
  public void fromLowerBase16() {
    assertThat(TraceId.fromLowerBase16("00000000000000000000000000000000"))
        .isEqualTo(TraceId.INVALID);
    assertThat(TraceId.fromLowerBase16("00000000000000000000000000000061")).isEqualTo(first);
    assertThat(TraceId.fromLowerBase16("ff000000000000000000000000000041")).isEqualTo(second);
  }

  @Test
  public void toLowerBase16() {
    assertThat(TraceId.INVALID.toLowerBase16()).isEqualTo("00000000000000000000000000000000");
    assertThat(first.toLowerBase16()).isEqualTo("00000000000000000000000000000061");
    assertThat(second.toLowerBase16()).isEqualTo("ff000000000000000000000000000041");
  }

  @Test
  public void traceId_CompareTo() {
    assertThat(first.compareTo(second)).isGreaterThan(0);
    assertThat(second.compareTo(first)).isLessThan(0);
    assertThat(first.compareTo(TraceId.fromBytes(firstBytes))).isEqualTo(0);
  }

  @Test
  public void traceId_EqualsAndHashCode() {
    EqualsTester tester = new EqualsTester();
    tester.addEqualityGroup(TraceId.INVALID, TraceId.INVALID);
    tester.addEqualityGroup(first, TraceId.fromBytes(Arrays.copyOf(firstBytes, firstBytes.length)));
    tester.addEqualityGroup(
        second, TraceId.fromBytes(Arrays.copyOf(secondBytes, secondBytes.length)));
    tester.testEquals();
  }

  @Test
  public void traceId_ToString() {
    assertThat(TraceId.INVALID.toString()).contains("00000000000000000000000000000000");
    assertThat(first.toString()).contains("00000000000000000000000000000061");
    assertThat(second.toString()).contains("ff000000000000000000000000000041");
  }
}
