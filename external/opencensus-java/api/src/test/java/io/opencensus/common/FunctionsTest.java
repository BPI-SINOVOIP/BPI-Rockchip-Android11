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

/** Tests for {@link Functions}. */
@RunWith(JUnit4.class)
public class FunctionsTest {
  @Rule public ExpectedException thrown = ExpectedException.none();

  @Test
  public void testReturnNull() {
    assertThat(Functions.returnNull().apply("ignored")).isNull();
  }

  @Test
  public void testReturnConstant() {
    assertThat(Functions.returnConstant(123).apply("ignored")).isEqualTo(123);
  }

  @Test
  public void testReturnToString() {
    assertThat(Functions.returnToString().apply("input")).isEqualTo("input");
    assertThat(Functions.returnToString().apply(Boolean.FALSE)).isEqualTo("false");
    assertThat(Functions.returnToString().apply(Double.valueOf(123.45))).isEqualTo("123.45");
    assertThat(Functions.returnToString().apply(null)).isEqualTo(null);
  }

  @Test
  public void testThrowIllegalArgumentException() {
    Function<Object, Void> f = Functions.throwIllegalArgumentException();
    thrown.expect(IllegalArgumentException.class);
    f.apply("ignored");
  }

  @Test
  public void testThrowAssertionError() {
    Function<Object, Void> f = Functions.throwAssertionError();
    thrown.handleAssertionErrors();
    thrown.expect(AssertionError.class);
    f.apply("ignored");
  }
}
