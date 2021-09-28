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

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import com.google.common.collect.Maps;
import io.opencensus.common.Duration;
import io.opencensus.common.Timestamp;
import io.opencensus.stats.Aggregation;
import io.opencensus.stats.Measure;
import io.opencensus.tags.TagValue;
import java.util.List;
import java.util.Map;

/*>>>
import org.checkerframework.checker.nullness.qual.Nullable;
*/

/** The bucket with aggregated {@code MeasureValue}s used for {@code IntervalViewData}. */
final class IntervalBucket {

  private static final Duration ZERO = Duration.create(0, 0);

  private final Timestamp start;
  private final Duration duration;
  private final Aggregation aggregation;
  private final Measure measure;
  private final Map<List</*@Nullable*/ TagValue>, MutableAggregation> tagValueAggregationMap =
      Maps.newHashMap();

  IntervalBucket(Timestamp start, Duration duration, Aggregation aggregation, Measure measure) {
    this.start = checkNotNull(start, "Start");
    this.duration = checkNotNull(duration, "Duration");
    checkArgument(duration.compareTo(ZERO) > 0, "Duration must be positive");
    this.aggregation = checkNotNull(aggregation, "Aggregation");
    this.measure = checkNotNull(measure, "measure");
  }

  Map<List</*@Nullable*/ TagValue>, MutableAggregation> getTagValueAggregationMap() {
    return tagValueAggregationMap;
  }

  Timestamp getStart() {
    return start;
  }

  // Puts a new value into the internal MutableAggregations, based on the TagValues.
  void record(
      List</*@Nullable*/ TagValue> tagValues,
      double value,
      Map<String, String> attachments,
      Timestamp timestamp) {
    if (!tagValueAggregationMap.containsKey(tagValues)) {
      tagValueAggregationMap.put(
          tagValues, RecordUtils.createMutableAggregation(aggregation, measure));
    }
    tagValueAggregationMap.get(tagValues).add(value, attachments, timestamp);
  }

  /*
   * Returns how much fraction of duration has passed in this IntervalBucket. For example, if this
   * bucket starts at 10s and has a duration of 20s, and now is 15s, then getFraction() should
   * return (15 - 10) / 20 = 0.25.
   *
   * This IntervalBucket must be current, i.e. the current timestamp must be within
   * [this.start, this.start + this.duration).
   */
  double getFraction(Timestamp now) {
    Duration elapsedTime = now.subtractTimestamp(start);
    checkArgument(
        elapsedTime.compareTo(ZERO) >= 0 && elapsedTime.compareTo(duration) < 0,
        "This bucket must be current.");
    return ((double) elapsedTime.toMillis()) / duration.toMillis();
  }

  void clearStats() {
    tagValueAggregationMap.clear();
  }
}
