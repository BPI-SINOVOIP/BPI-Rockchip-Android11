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

package io.opencensus.metrics.export;

import io.opencensus.common.ExperimentalApi;
import io.opencensus.internal.Utils;
import java.util.Collections;
import java.util.Set;
import javax.annotation.concurrent.ThreadSafe;

/**
 * Keeps a set of {@link MetricProducer} that is used by exporters to determine the metrics that
 * need to be exported.
 *
 * @since 0.17
 */
@ExperimentalApi
@ThreadSafe
public abstract class MetricProducerManager {

  /**
   * Adds the {@link MetricProducer} to the manager if it is not already present.
   *
   * @param metricProducer the {@code MetricProducer} to be added to the manager.
   * @since 0.17
   */
  public abstract void add(MetricProducer metricProducer);

  /**
   * Removes the {@link MetricProducer} to the manager if it is present.
   *
   * @param metricProducer the {@code MetricProducer} to be removed from the manager.
   * @since 0.17
   */
  public abstract void remove(MetricProducer metricProducer);

  /**
   * Returns all registered {@link MetricProducer}s that should be exported.
   *
   * <p>This method should be used by any metrics exporter that automatically exports data for
   * {@code MetricProducer} registered with the {@code MetricProducerManager}.
   *
   * @return all registered {@code MetricProducer}s that should be exported.
   * @since 0.17
   */
  public abstract Set<MetricProducer> getAllMetricProducer();

  /**
   * Returns a no-op implementation for {@link MetricProducerManager}.
   *
   * @return a no-op implementation for {@code MetricProducerManager}.
   */
  static MetricProducerManager newNoopMetricProducerManager() {
    return new NoopMetricProducerManager();
  }

  private static final class NoopMetricProducerManager extends MetricProducerManager {

    @Override
    public void add(MetricProducer metricProducer) {
      Utils.checkNotNull(metricProducer, "metricProducer");
    }

    @Override
    public void remove(MetricProducer metricProducer) {
      Utils.checkNotNull(metricProducer, "metricProducer");
    }

    @Override
    public Set<MetricProducer> getAllMetricProducer() {
      return Collections.emptySet();
    }
  }
}
