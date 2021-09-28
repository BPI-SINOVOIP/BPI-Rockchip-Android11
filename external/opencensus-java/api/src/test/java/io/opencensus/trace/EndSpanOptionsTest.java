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

/** Unit tests for {@link EndSpanOptions}. */
@RunWith(JUnit4.class)
public class EndSpanOptionsTest {
  @Test
  public void endSpanOptions_DefaultOptions() {
    assertThat(EndSpanOptions.DEFAULT.getStatus()).isNull();
    assertThat(EndSpanOptions.DEFAULT.getSampleToLocalSpanStore()).isFalse();
  }

  @Test
  public void setStatus_Ok() {
    EndSpanOptions endSpanOptions = EndSpanOptions.builder().setStatus(Status.OK).build();
    assertThat(endSpanOptions.getStatus()).isEqualTo(Status.OK);
  }

  @Test
  public void setStatus_Error() {
    EndSpanOptions endSpanOptions =
        EndSpanOptions.builder()
            .setStatus(Status.CANCELLED.withDescription("ThisIsAnError"))
            .build();
    assertThat(endSpanOptions.getStatus())
        .isEqualTo(Status.CANCELLED.withDescription("ThisIsAnError"));
  }

  @Test
  public void setSampleToLocalSpanStore() {
    EndSpanOptions endSpanOptions =
        EndSpanOptions.builder().setSampleToLocalSpanStore(true).build();
    assertThat(endSpanOptions.getSampleToLocalSpanStore()).isTrue();
  }

  @Test
  public void endSpanOptions_EqualsAndHashCode() {
    EqualsTester tester = new EqualsTester();
    tester.addEqualityGroup(
        EndSpanOptions.builder()
            .setStatus(Status.CANCELLED.withDescription("ThisIsAnError"))
            .build(),
        EndSpanOptions.builder()
            .setStatus(Status.CANCELLED.withDescription("ThisIsAnError"))
            .build());
    tester.addEqualityGroup(EndSpanOptions.builder().build(), EndSpanOptions.DEFAULT);
    tester.testEquals();
  }

  @Test
  public void endSpanOptions_ToString() {
    EndSpanOptions endSpanOptions =
        EndSpanOptions.builder()
            .setStatus(Status.CANCELLED.withDescription("ThisIsAnError"))
            .build();
    assertThat(endSpanOptions.toString()).contains("ThisIsAnError");
  }
}
