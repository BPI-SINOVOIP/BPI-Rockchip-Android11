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

package io.opencensus.common;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link Duration}. */
@RunWith(JUnit4.class)
public class DurationTest {
  @Rule public ExpectedException thrown = ExpectedException.none();

  @Test
  public void testDurationCreate() {
    assertThat(Duration.create(24, 42).getSeconds()).isEqualTo(24);
    assertThat(Duration.create(24, 42).getNanos()).isEqualTo(42);
    assertThat(Duration.create(-24, -42).getSeconds()).isEqualTo(-24);
    assertThat(Duration.create(-24, -42).getNanos()).isEqualTo(-42);
    assertThat(Duration.create(315576000000L, 999999999).getSeconds()).isEqualTo(315576000000L);
    assertThat(Duration.create(315576000000L, 999999999).getNanos()).isEqualTo(999999999);
    assertThat(Duration.create(-315576000000L, -999999999).getSeconds()).isEqualTo(-315576000000L);
    assertThat(Duration.create(-315576000000L, -999999999).getNanos()).isEqualTo(-999999999);
  }

  @Test
  public void create_SecondsTooLow() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' is less than minimum (-315576000000): -315576000001");
    Duration.create(-315576000001L, 0);
  }

  @Test
  public void create_SecondsTooHigh() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' is greater than maximum (315576000000): 315576000001");
    Duration.create(315576000001L, 0);
  }

  @Test
  public void create_NanosTooLow() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'nanos' is less than minimum (-999999999): -1000000000");
    Duration.create(0, -1000000000);
  }

  @Test
  public void create_NanosTooHigh() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'nanos' is greater than maximum (999999999): 1000000000");
    Duration.create(0, 1000000000);
  }

  @Test
  public void create_NegativeSecondsPositiveNanos() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' and 'nanos' have inconsistent sign: seconds=-1, nanos=1");
    Duration.create(-1, 1);
  }

  @Test
  public void create_PositiveSecondsNegativeNanos() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' and 'nanos' have inconsistent sign: seconds=1, nanos=-1");
    Duration.create(1, -1);
  }

  @Test
  public void testDurationFromMillis() {
    assertThat(Duration.fromMillis(0)).isEqualTo(Duration.create(0, 0));
    assertThat(Duration.fromMillis(987)).isEqualTo(Duration.create(0, 987000000));
    assertThat(Duration.fromMillis(3456)).isEqualTo(Duration.create(3, 456000000));
  }

  @Test
  public void testDurationFromMillisNegative() {
    assertThat(Duration.fromMillis(-1)).isEqualTo(Duration.create(0, -1000000));
    assertThat(Duration.fromMillis(-999)).isEqualTo(Duration.create(0, -999000000));
    assertThat(Duration.fromMillis(-1000)).isEqualTo(Duration.create(-1, 0));
    assertThat(Duration.fromMillis(-3456)).isEqualTo(Duration.create(-3, -456000000));
  }

  @Test
  public void fromMillis_TooLow() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' is less than minimum (-315576000000): -315576000001");
    Duration.fromMillis(-315576000001000L);
  }

  @Test
  public void fromMillis_TooHigh() {
    thrown.expect(IllegalArgumentException.class);
    thrown.expectMessage("'seconds' is greater than maximum (315576000000): 315576000001");
    Duration.fromMillis(315576000001000L);
  }

  @Test
  public void duration_CompareLength() {
    assertThat(Duration.create(0, 0).compareTo(Duration.create(0, 0))).isEqualTo(0);
    assertThat(Duration.create(24, 42).compareTo(Duration.create(24, 42))).isEqualTo(0);
    assertThat(Duration.create(-24, -42).compareTo(Duration.create(-24, -42))).isEqualTo(0);
    assertThat(Duration.create(25, 42).compareTo(Duration.create(24, 42))).isEqualTo(1);
    assertThat(Duration.create(24, 45).compareTo(Duration.create(24, 42))).isEqualTo(1);
    assertThat(Duration.create(24, 42).compareTo(Duration.create(25, 42))).isEqualTo(-1);
    assertThat(Duration.create(24, 42).compareTo(Duration.create(24, 45))).isEqualTo(-1);
    assertThat(Duration.create(-24, -45).compareTo(Duration.create(-24, -42))).isEqualTo(-1);
    assertThat(Duration.create(-24, -42).compareTo(Duration.create(-25, -42))).isEqualTo(1);
    assertThat(Duration.create(24, 42).compareTo(Duration.create(-24, -42))).isEqualTo(1);
  }

  @Test
  public void testDurationEqual() {
    // Positive tests.
    assertThat(Duration.create(0, 0)).isEqualTo(Duration.create(0, 0));
    assertThat(Duration.create(24, 42)).isEqualTo(Duration.create(24, 42));
    assertThat(Duration.create(-24, -42)).isEqualTo(Duration.create(-24, -42));
    // Negative tests.
    assertThat(Duration.create(25, 42)).isNotEqualTo(Duration.create(24, 42));
    assertThat(Duration.create(24, 43)).isNotEqualTo(Duration.create(24, 42));
    assertThat(Duration.create(-25, -42)).isNotEqualTo(Duration.create(-24, -42));
    assertThat(Duration.create(-24, -43)).isNotEqualTo(Duration.create(-24, -42));
  }

  @Test
  public void toMillis() {
    assertThat(Duration.create(10, 0).toMillis()).isEqualTo(10000L);
    assertThat(Duration.create(10, 1000).toMillis()).isEqualTo(10000L);
    assertThat(Duration.create(0, (int) 1e6).toMillis()).isEqualTo(1L);
    assertThat(Duration.create(0, 0).toMillis()).isEqualTo(0L);
    assertThat(Duration.create(-10, 0).toMillis()).isEqualTo(-10000L);
    assertThat(Duration.create(-10, -1000).toMillis()).isEqualTo(-10000L);
    assertThat(Duration.create(0, -(int) 1e6).toMillis()).isEqualTo(-1L);
  }
}
