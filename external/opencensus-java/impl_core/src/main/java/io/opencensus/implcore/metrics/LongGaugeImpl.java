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

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import com.google.common.annotations.VisibleForTesting;
import io.opencensus.common.Clock;
import io.opencensus.implcore.internal.Utils;
import io.opencensus.metrics.LabelKey;
import io.opencensus.metrics.LabelValue;
import io.opencensus.metrics.LongGauge;
import io.opencensus.metrics.export.Metric;
import io.opencensus.metrics.export.MetricDescriptor;
import io.opencensus.metrics.export.MetricDescriptor.Type;
import io.opencensus.metrics.export.Point;
import io.opencensus.metrics.export.TimeSeries;
import io.opencensus.metrics.export.Value;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;
import javax.annotation.Nullable;

/** Implementation of {@link LongGauge}. */
public final class LongGaugeImpl extends LongGauge implements Meter {
  @VisibleForTesting static final LabelValue UNSET_VALUE = LabelValue.create(null);

  private final MetricDescriptor metricDescriptor;
  private volatile Map<List<LabelValue>, PointImpl> registeredPoints =
      Collections.<List<LabelValue>, PointImpl>emptyMap();
  private final int labelKeysSize;
  private final List<LabelValue> defaultLabelValues;

  LongGaugeImpl(String name, String description, String unit, List<LabelKey> labelKeys) {
    labelKeysSize = labelKeys.size();
    this.metricDescriptor =
        MetricDescriptor.create(name, description, unit, Type.GAUGE_INT64, labelKeys);

    // initialize defaultLabelValues
    defaultLabelValues = new ArrayList<LabelValue>(labelKeysSize);
    for (int i = 0; i < labelKeysSize; i++) {
      defaultLabelValues.add(UNSET_VALUE);
    }
  }

  @Override
  public LongPoint getOrCreateTimeSeries(List<LabelValue> labelValues) {
    // lock free point retrieval, if it is present
    PointImpl existingPoint = registeredPoints.get(labelValues);
    if (existingPoint != null) {
      return existingPoint;
    }

    List<LabelValue> labelValuesCopy =
        Collections.unmodifiableList(
            new ArrayList<LabelValue>(checkNotNull(labelValues, "labelValues")));
    return registerTimeSeries(labelValuesCopy);
  }

  @Override
  public LongPoint getDefaultTimeSeries() {
    // lock free default point retrieval, if it is present
    PointImpl existingPoint = registeredPoints.get(defaultLabelValues);
    if (existingPoint != null) {
      return existingPoint;
    }
    return registerTimeSeries(Collections.unmodifiableList(defaultLabelValues));
  }

  @Override
  public synchronized void removeTimeSeries(List<LabelValue> labelValues) {
    checkNotNull(labelValues, "labelValues");

    Map<List<LabelValue>, PointImpl> registeredPointsCopy =
        new LinkedHashMap<List<LabelValue>, PointImpl>(registeredPoints);
    if (registeredPointsCopy.remove(labelValues) == null) {
      // The element not present, no need to update the current map of points.
      return;
    }
    registeredPoints = Collections.unmodifiableMap(registeredPointsCopy);
  }

  @Override
  public synchronized void clear() {
    registeredPoints = Collections.<List<LabelValue>, PointImpl>emptyMap();
  }

  private synchronized LongPoint registerTimeSeries(List<LabelValue> labelValues) {
    PointImpl existingPoint = registeredPoints.get(labelValues);
    if (existingPoint != null) {
      // Return a Point that are already registered. This can happen if a multiple threads
      // concurrently try to register the same {@code TimeSeries}.
      return existingPoint;
    }

    checkArgument(labelKeysSize == labelValues.size(), "Incorrect number of labels.");
    Utils.checkListElementNotNull(labelValues, "labelValue element should not be null.");

    PointImpl newPoint = new PointImpl(labelValues);
    // Updating the map of points happens under a lock to avoid multiple add operations
    // to happen in the same time.
    Map<List<LabelValue>, PointImpl> registeredPointsCopy =
        new LinkedHashMap<List<LabelValue>, PointImpl>(registeredPoints);
    registeredPointsCopy.put(labelValues, newPoint);
    registeredPoints = Collections.unmodifiableMap(registeredPointsCopy);

    return newPoint;
  }

  @Nullable
  @Override
  public Metric getMetric(Clock clock) {
    Map<List<LabelValue>, PointImpl> currentRegisteredPoints = registeredPoints;
    if (currentRegisteredPoints.isEmpty()) {
      return null;
    }

    if (currentRegisteredPoints.size() == 1) {
      PointImpl point = currentRegisteredPoints.values().iterator().next();
      return Metric.createWithOneTimeSeries(metricDescriptor, point.getTimeSeries(clock));
    }

    List<TimeSeries> timeSeriesList = new ArrayList<TimeSeries>(currentRegisteredPoints.size());
    for (Map.Entry<List<LabelValue>, PointImpl> entry : currentRegisteredPoints.entrySet()) {
      timeSeriesList.add(entry.getValue().getTimeSeries(clock));
    }
    return Metric.create(metricDescriptor, timeSeriesList);
  }

  /** Implementation of {@link LongGauge.LongPoint}. */
  public static final class PointImpl extends LongPoint {

    // TODO(mayurkale): Consider to use LongAdder here, once we upgrade to Java8.
    private final AtomicLong value = new AtomicLong(0);
    private final List<LabelValue> labelValues;

    PointImpl(List<LabelValue> labelValues) {
      this.labelValues = labelValues;
    }

    @Override
    public void add(long amt) {
      value.addAndGet(amt);
    }

    @Override
    public void set(long val) {
      value.set(val);
    }

    private TimeSeries getTimeSeries(Clock clock) {
      return TimeSeries.createWithOnePoint(
          labelValues, Point.create(Value.longValue(value.get()), clock.now()), null);
    }
  }
}
