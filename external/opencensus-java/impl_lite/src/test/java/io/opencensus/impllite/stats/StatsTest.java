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

package io.opencensus.impllite.stats;

import static com.google.common.truth.Truth.assertThat;

import io.opencensus.implcore.stats.StatsRecorderImpl;
import io.opencensus.implcore.stats.ViewManagerImpl;
import io.opencensus.stats.Stats;
import io.opencensus.stats.StatsComponent;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test for accessing the {@link StatsComponent} through the {@link Stats} class. */
@RunWith(JUnit4.class)
public final class StatsTest {
  @Test
  public void getStatsRecorder() {
    assertThat(Stats.getStatsRecorder()).isInstanceOf(StatsRecorderImpl.class);
  }

  @Test
  public void getViewManager() {
    assertThat(Stats.getViewManager()).isInstanceOf(ViewManagerImpl.class);
  }
}
