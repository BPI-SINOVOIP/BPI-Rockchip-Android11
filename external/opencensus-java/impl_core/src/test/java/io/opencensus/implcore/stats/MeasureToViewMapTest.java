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

package io.opencensus.implcore.stats;

import static com.google.common.truth.Truth.assertThat;

import io.opencensus.common.Timestamp;
import io.opencensus.implcore.internal.CurrentState.State;
import io.opencensus.stats.Aggregation.Mean;
import io.opencensus.stats.Measure;
import io.opencensus.stats.View;
import io.opencensus.stats.View.AggregationWindow.Cumulative;
import io.opencensus.stats.View.Name;
import io.opencensus.stats.ViewData;
import io.opencensus.stats.ViewData.AggregationWindowData.CumulativeData;
import io.opencensus.tags.TagKey;
import io.opencensus.testing.common.TestClock;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for {@link MeasureToViewMap}. */
@RunWith(JUnit4.class)
public class MeasureToViewMapTest {

  private static final Measure MEASURE =
      Measure.MeasureDouble.create("my measurement", "measurement description", "By");

  private static final Name VIEW_NAME = View.Name.create("my view");

  private static final Cumulative CUMULATIVE = Cumulative.create();

  private static final View VIEW =
      View.create(
          VIEW_NAME,
          "view description",
          MEASURE,
          Mean.create(),
          Arrays.asList(TagKey.create("my key")),
          CUMULATIVE);

  @Test
  public void testRegisterAndGetView() {
    MeasureToViewMap measureToViewMap = new MeasureToViewMap();
    TestClock clock = TestClock.create(Timestamp.create(10, 20));
    measureToViewMap.registerView(VIEW, clock);
    clock.setTime(Timestamp.create(30, 40));
    ViewData viewData = measureToViewMap.getView(VIEW_NAME, clock, State.ENABLED);
    assertThat(viewData.getView()).isEqualTo(VIEW);
    assertThat(viewData.getWindowData())
        .isEqualTo(CumulativeData.create(Timestamp.create(10, 20), Timestamp.create(30, 40)));
    assertThat(viewData.getAggregationMap()).isEmpty();
  }
}
