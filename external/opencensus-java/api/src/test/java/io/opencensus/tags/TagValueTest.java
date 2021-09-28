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

package io.opencensus.tags;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.testing.EqualsTester;
import java.util.Arrays;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for {@link TagValue}. */
@RunWith(JUnit4.class)
public final class TagValueTest {
  @Rule public final ExpectedException thrown = ExpectedException.none();

  @Test
  public void testMaxLength() {
    assertThat(TagValue.MAX_LENGTH).isEqualTo(255);
  }

  @Test
  public void testAsString() {
    assertThat(TagValue.create("foo").asString()).isEqualTo("foo");
  }

  @Test
  public void create_AllowTagValueWithMaxLength() {
    char[] chars = new char[TagValue.MAX_LENGTH];
    Arrays.fill(chars, 'v');
    String value = new String(chars);
    assertThat(TagValue.create(value).asString()).isEqualTo(value);
  }

  @Test
  public void create_DisallowTagValueOverMaxLength() {
    char[] chars = new char[TagValue.MAX_LENGTH + 1];
    Arrays.fill(chars, 'v');
    String value = new String(chars);
    thrown.expect(IllegalArgumentException.class);
    TagValue.create(value);
  }

  @Test
  public void disallowTagValueWithUnprintableChars() {
    String value = "\2ab\3cd";
    thrown.expect(IllegalArgumentException.class);
    TagValue.create(value);
  }

  @Test
  public void testTagValueEquals() {
    new EqualsTester()
        .addEqualityGroup(TagValue.create("foo"), TagValue.create("foo"))
        .addEqualityGroup(TagValue.create("bar"))
        .testEquals();
  }
}
