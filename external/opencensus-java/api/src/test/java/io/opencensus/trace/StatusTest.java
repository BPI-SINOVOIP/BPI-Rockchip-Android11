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

/** Unit tests for {@link Status}. */
@RunWith(JUnit4.class)
public class StatusTest {
  @Test
  public void status_Ok() {
    assertThat(Status.OK.getCanonicalCode()).isEqualTo(Status.CanonicalCode.OK);
    assertThat(Status.OK.getDescription()).isNull();
    assertThat(Status.OK.isOk()).isTrue();
  }

  @Test
  public void createStatus_WithDescription() {
    Status status = Status.UNKNOWN.withDescription("This is an error.");
    assertThat(status.getCanonicalCode()).isEqualTo(Status.CanonicalCode.UNKNOWN);
    assertThat(status.getDescription()).isEqualTo("This is an error.");
    assertThat(status.isOk()).isFalse();
  }

  @Test
  public void status_EqualsAndHashCode() {
    EqualsTester tester = new EqualsTester();
    tester.addEqualityGroup(Status.OK, Status.OK.withDescription(null));
    tester.addEqualityGroup(
        Status.CANCELLED.withDescription("ThisIsAnError"),
        Status.CANCELLED.withDescription("ThisIsAnError"));
    tester.addEqualityGroup(Status.UNKNOWN.withDescription("This is an error."));
    tester.testEquals();
  }
}
