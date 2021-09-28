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

package io.opencensus.stats;

import static com.google.common.truth.Truth.assertThat;

import com.google.common.collect.Lists;
import com.google.common.testing.EqualsTester;
import io.opencensus.common.Function;
import io.opencensus.common.Functions;
import io.opencensus.stats.Measure.MeasureDouble;
import io.opencensus.stats.Measure.MeasureLong;
import java.util.Arrays;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for {@link Measure}. */
@RunWith(JUnit4.class)
public final class MeasureTest {

  @Rule public final ExpectedException thrown = ExpectedException.none();

  @Test
  public void testConstants() {
    assertThat(Measure.NAME_MAX_LENGTH).isEqualTo(255);
  }

  @Test
  public void preventTooLongMeasureName() {
    char[] chars = new char[Measure.NAME_MAX_LENGTH + 1];
    Arrays.fill(chars, 'a');
    String longName = String.valueOf(chars);
    thrown.expect(IllegalArgumentException.class);
    Measure.MeasureDouble.create(longName, "description", "1");
  }

  @Test
  public void preventNonPrintableMeasureName() {
    thrown.expect(IllegalArgumentException.class);
    Measure.MeasureDouble.create("\2", "description", "1");
  }

  @Test
  public void testMeasureDoubleComponents() {
    Measure measurement = Measure.MeasureDouble.create("Foo", "The description of Foo", "Mbit/s");
    assertThat(measurement.getName()).isEqualTo("Foo");
    assertThat(measurement.getDescription()).isEqualTo("The description of Foo");
    assertThat(measurement.getUnit()).isEqualTo("Mbit/s");
  }

  @Test
  public void testMeasureLongComponents() {
    Measure measurement = Measure.MeasureLong.create("Bar", "The description of Bar", "1");
    assertThat(measurement.getName()).isEqualTo("Bar");
    assertThat(measurement.getDescription()).isEqualTo("The description of Bar");
    assertThat(measurement.getUnit()).isEqualTo("1");
  }

  @Test
  public void testMeasureDoubleEquals() {
    new EqualsTester()
        .addEqualityGroup(
            Measure.MeasureDouble.create("name", "description", "bit/s"),
            Measure.MeasureDouble.create("name", "description", "bit/s"))
        .addEqualityGroup(Measure.MeasureDouble.create("name", "description 2", "bit/s"))
        .testEquals();
  }

  @Test
  public void testMeasureLongEquals() {
    new EqualsTester()
        .addEqualityGroup(
            Measure.MeasureLong.create("name", "description", "bit/s"),
            Measure.MeasureLong.create("name", "description", "bit/s"))
        .addEqualityGroup(Measure.MeasureLong.create("name", "description 2", "bit/s"))
        .testEquals();
  }

  @Test
  public void testMatch() {
    List<Measure> measures =
        Arrays.asList(
            MeasureDouble.create("measure1", "description", "1"),
            MeasureLong.create("measure2", "description", "1"));
    List<String> outputs = Lists.newArrayList();
    for (Measure measure : measures) {
      outputs.add(
          measure.match(
              new Function<MeasureDouble, String>() {
                @Override
                public String apply(MeasureDouble arg) {
                  return "double";
                }
              },
              new Function<MeasureLong, String>() {
                @Override
                public String apply(MeasureLong arg) {
                  return "long";
                }
              },
              Functions.<String>throwAssertionError()));
    }
    assertThat(outputs).containsExactly("double", "long").inOrder();
  }

  @Test
  public void testMeasureDoubleIsNotEqualToMeasureLong() {
    assertThat(Measure.MeasureDouble.create("name", "description", "bit/s"))
        .isNotEqualTo(Measure.MeasureLong.create("name", "description", "bit/s"));
  }
}
