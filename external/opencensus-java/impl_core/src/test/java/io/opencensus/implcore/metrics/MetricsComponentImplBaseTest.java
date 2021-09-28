/*
 * Copyright 2018, OpenCensus Authors
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

package io.opencensus.implcore.metrics;

import static com.google.common.truth.Truth.assertThat;

import io.opencensus.implcore.metrics.export.ExportComponentImpl;
import io.opencensus.testing.common.TestClock;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link MetricsComponentImplBase}. */
@RunWith(JUnit4.class)
public class MetricsComponentImplBaseTest {
  private final MetricsComponentImplBase metricsComponentImplBase =
      new MetricsComponentImplBase(TestClock.create());

  @Test
  public void getExportComponent() {
    assertThat(metricsComponentImplBase.getExportComponent())
        .isInstanceOf(ExportComponentImpl.class);
  }

  @Test
  public void getMetricRegistry() {
    assertThat(metricsComponentImplBase.getMetricRegistry()).isInstanceOf(MetricRegistryImpl.class);
  }

  @Test
  public void metricRegistry_InstalledToMetricProducerManger() {
    assertThat(
            metricsComponentImplBase
                .getExportComponent()
                .getMetricProducerManager()
                .getAllMetricProducer())
        .containsExactly(metricsComponentImplBase.getMetricRegistry().getMetricProducer());
  }
}
