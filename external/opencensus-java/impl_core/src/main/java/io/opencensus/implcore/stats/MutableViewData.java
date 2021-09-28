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
import static io.opencensus.implcore.stats.RecordUtils.createAggregationMap;
import static io.opencensus.implcore.stats.RecordUtils.createMutableAggregation;
import static io.opencensus.implcore.stats.RecordUtils.getTagMap;
import static io.opencensus.implcore.stats.RecordUtils.getTagValues;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.LinkedHashMultimap;
import com.google.common.collect.Maps;
import com.google.common.collect.Multimap;
import io.opencensus.common.Duration;
import io.opencensus.common.Function;
import io.opencensus.common.Functions;
import io.opencensus.common.Timestamp;
import io.opencensus.implcore.internal.CheckerFrameworkUtils;
import io.opencensus.implcore.internal.CurrentState.State;
import io.opencensus.metrics.LabelValue;
import io.opencensus.metrics.export.Metric;
import io.opencensus.metrics.export.MetricDescriptor;
import io.opencensus.metrics.export.MetricDescriptor.Type;
import io.opencensus.metrics.export.Point;
import io.opencensus.metrics.export.TimeSeries;
import io.opencensus.stats.Aggregation;
import io.opencensus.stats.AggregationData;
import io.opencensus.stats.Measure;
import io.opencensus.stats.View;
import io.opencensus.stats.ViewData;
import io.opencensus.tags.TagContext;
import io.opencensus.tags.TagValue;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/*>>>
import org.checkerframework.checker.nullness.qual.Nullable;
*/

/** A mutable version of {@link ViewData}, used for recording stats and start/end time. */
@SuppressWarnings("deprecation")
abstract class MutableViewData {

  @VisibleForTesting static final Timestamp ZERO_TIMESTAMP = Timestamp.create(0, 0);

  private final View view;

  private MutableViewData(View view) {
    this.view = view;
  }

  /**
   * Constructs a new {@link MutableViewData}.
   *
   * @param view the {@code View} linked with this {@code MutableViewData}.
   * @param start the start {@code Timestamp}.
   * @return a {@code MutableViewData}.
   */
  static MutableViewData create(final View view, final Timestamp start) {
    return view.getWindow()
        .match(
            new CreateCumulative(view, start),
            new CreateInterval(view, start),
            Functions.<MutableViewData>throwAssertionError());
  }

  /** The {@link View} associated with this {@link ViewData}. */
  View getView() {
    return view;
  }

  @javax.annotation.Nullable
  abstract Metric toMetric(Timestamp now, State state);

  /** Record stats with the given tags. */
  abstract void record(
      TagContext context, double value, Timestamp timestamp, Map<String, String> attachments);

  /** Convert this {@link MutableViewData} to {@link ViewData}. */
  abstract ViewData toViewData(Timestamp now, State state);

  // Clear recorded stats.
  abstract void clearStats();

  // Resume stats collection, and reset Start Timestamp (for CumulativeMutableViewData), or refresh
  // bucket list (for InternalMutableViewData).
  abstract void resumeStatsCollection(Timestamp now);

  private static final class CumulativeMutableViewData extends MutableViewData {

    private Timestamp start;
    private final Map<List</*@Nullable*/ TagValue>, MutableAggregation> tagValueAggregationMap =
        Maps.newHashMap();
    // Cache a MetricDescriptor to avoid converting View to MetricDescriptor in the future.
    private final MetricDescriptor metricDescriptor;

    private CumulativeMutableViewData(View view, Timestamp start) {
      super(view);
      this.start = start;
      MetricDescriptor metricDescriptor = MetricUtils.viewToMetricDescriptor(view);
      if (metricDescriptor == null) {
        throw new AssertionError(
            "Cumulative view should be converted to a non-null MetricDescriptor.");
      } else {
        this.metricDescriptor = metricDescriptor;
      }
    }

    @javax.annotation.Nullable
    @Override
    Metric toMetric(Timestamp now, State state) {
      if (state == State.DISABLED) {
        return null;
      }
      Type type = metricDescriptor.getType();
      @javax.annotation.Nullable
      Timestamp startTime = type == Type.GAUGE_INT64 || type == Type.GAUGE_DOUBLE ? null : start;
      List<TimeSeries> timeSeriesList = new ArrayList<TimeSeries>();
      for (Entry<List</*@Nullable*/ TagValue>, MutableAggregation> entry :
          tagValueAggregationMap.entrySet()) {
        List<LabelValue> labelValues = MetricUtils.tagValuesToLabelValues(entry.getKey());
        Point point = entry.getValue().toPoint(now);
        timeSeriesList.add(TimeSeries.createWithOnePoint(labelValues, point, startTime));
      }
      return Metric.create(metricDescriptor, timeSeriesList);
    }

    @Override
    void record(
        TagContext context, double value, Timestamp timestamp, Map<String, String> attachments) {
      List</*@Nullable*/ TagValue> tagValues =
          getTagValues(getTagMap(context), super.view.getColumns());
      if (!tagValueAggregationMap.containsKey(tagValues)) {
        tagValueAggregationMap.put(
            tagValues,
            createMutableAggregation(super.view.getAggregation(), super.getView().getMeasure()));
      }
      tagValueAggregationMap.get(tagValues).add(value, attachments, timestamp);
    }

    @Override
    ViewData toViewData(Timestamp now, State state) {
      if (state == State.ENABLED) {
        return ViewData.create(
            super.view,
            createAggregationMap(tagValueAggregationMap, super.view.getMeasure()),
            ViewData.AggregationWindowData.CumulativeData.create(start, now));
      } else {
        // If Stats state is DISABLED, return an empty ViewData.
        return ViewData.create(
            super.view,
            Collections.<List</*@Nullable*/ TagValue>, AggregationData>emptyMap(),
            ViewData.AggregationWindowData.CumulativeData.create(ZERO_TIMESTAMP, ZERO_TIMESTAMP));
      }
    }

    @Override
    void clearStats() {
      tagValueAggregationMap.clear();
    }

    @Override
    void resumeStatsCollection(Timestamp now) {
      start = now;
    }
  }

  /*
   * For each IntervalView, we always keep a queue of N + 1 buckets (by default N is 4).
   * Each bucket has a duration which is interval duration / N.
   * Ideally:
   * 1. the buckets should always be up-to-date,
   * 2. current time should always be within the latest bucket, currently recorded stats should fall
   *    into the latest bucket,
   * 3. there are always N buckets before the current one, which holds the stats in the past
   *    interval duration.
   *
   * When getView() is called, we will extract and combine the stats from the current and past
   * buckets (part of the stats from the oldest bucket could have expired).
   *
   * However, in reality, we couldn't track the status of buckets all the time (keep monitoring and
   * updating the bucket queue will be expensive). When we call record() or getView(), some or all
   * of the buckets might be outdated, and we will need to "pad" new buckets to the queue and remove
   * outdated ones. After refreshing buckets, the bucket queue will able to maintain the three
   * invariants in the ideal situation.
   *
   * For example:
   * 1. We have an IntervalView which has a duration of 8 seconds, we register this view at 10s.
   * 2. Initially there will be 5 buckets: [2.0, 4.0), [4.0, 6.0), ..., [10.0, 12.0).
   * 3. If users don't call record() or getView(), bucket queue will remain as it is, and some
   *    buckets could expire.
   * 4. Suppose record() is called at 15s, now we need to refresh the bucket queue. We need to add
   *    two new buckets [12.0, 14.0) and [14.0, 16.0), and remove two expired buckets [2.0, 4.0)
   *    and [4.0, 6.0)
   * 5. Suppose record() is called again at 30s, all the current buckets should have expired. We add
   *    5 new buckets [22.0, 24.0) ... [30.0, 32.0) and remove all the previous buckets.
   * 6. Suppose users call getView() at 35s, again we need to add two new buckets and remove two
   *    expired one, so that bucket queue is up-to-date. Now we combine stats from all buckets and
   *    return the combined IntervalViewData.
   */
  private static final class IntervalMutableViewData extends MutableViewData {

    // TODO(songya): allow customizable bucket size in the future.
    private static final int N = 4; // IntervalView has N + 1 buckets

    private final ArrayDeque<IntervalBucket> buckets = new ArrayDeque<IntervalBucket>();

    private final Duration totalDuration; // Duration of the whole interval.
    private final Duration bucketDuration; // Duration of a single bucket (totalDuration / N)

    private IntervalMutableViewData(View view, Timestamp start) {
      super(view);
      Duration totalDuration = ((View.AggregationWindow.Interval) view.getWindow()).getDuration();
      this.totalDuration = totalDuration;
      this.bucketDuration = Duration.fromMillis(totalDuration.toMillis() / N);

      // When initializing. add N empty buckets prior to the start timestamp of this
      // IntervalMutableViewData, so that the last bucket will be the current one in effect.
      shiftBucketList(N + 1, start);
    }

    @javax.annotation.Nullable
    @Override
    Metric toMetric(Timestamp now, State state) {
      return null;
    }

    @Override
    void record(
        TagContext context, double value, Timestamp timestamp, Map<String, String> attachments) {
      List</*@Nullable*/ TagValue> tagValues =
          getTagValues(getTagMap(context), super.view.getColumns());
      refreshBucketList(timestamp);
      // It is always the last bucket that does the recording.
      CheckerFrameworkUtils.castNonNull(buckets.peekLast())
          .record(tagValues, value, attachments, timestamp);
    }

    @Override
    ViewData toViewData(Timestamp now, State state) {
      refreshBucketList(now);
      if (state == State.ENABLED) {
        return ViewData.create(
            super.view,
            combineBucketsAndGetAggregationMap(now),
            ViewData.AggregationWindowData.IntervalData.create(now));
      } else {
        // If Stats state is DISABLED, return an empty ViewData.
        return ViewData.create(
            super.view,
            Collections.<List</*@Nullable*/ TagValue>, AggregationData>emptyMap(),
            ViewData.AggregationWindowData.IntervalData.create(ZERO_TIMESTAMP));
      }
    }

    @Override
    void clearStats() {
      for (IntervalBucket bucket : buckets) {
        bucket.clearStats();
      }
    }

    @Override
    void resumeStatsCollection(Timestamp now) {
      // Refresh bucket list to be ready for stats recording, so that if record() is called right
      // after stats state is turned back on, record() will be faster.
      refreshBucketList(now);
    }

    // Add new buckets and remove expired buckets by comparing the current timestamp with
    // timestamp of the last bucket.
    private void refreshBucketList(Timestamp now) {
      if (buckets.size() != N + 1) {
        throw new AssertionError("Bucket list must have exactly " + (N + 1) + " buckets.");
      }
      Timestamp startOfLastBucket =
          CheckerFrameworkUtils.castNonNull(buckets.peekLast()).getStart();
      // TODO(songya): decide what to do when time goes backwards
      checkArgument(
          now.compareTo(startOfLastBucket) >= 0,
          "Current time must be within or after the last bucket.");
      long elapsedTimeMillis = now.subtractTimestamp(startOfLastBucket).toMillis();
      long numOfPadBuckets = elapsedTimeMillis / bucketDuration.toMillis();

      shiftBucketList(numOfPadBuckets, now);
    }

    // Add specified number of new buckets, and remove expired buckets
    private void shiftBucketList(long numOfPadBuckets, Timestamp now) {
      Timestamp startOfNewBucket;

      if (!buckets.isEmpty()) {
        startOfNewBucket =
            CheckerFrameworkUtils.castNonNull(buckets.peekLast())
                .getStart()
                .addDuration(bucketDuration);
      } else {
        // Initialize bucket list. Should only enter this block once.
        startOfNewBucket = subtractDuration(now, totalDuration);
      }

      if (numOfPadBuckets > N + 1) {
        // All current buckets expired, need to add N + 1 new buckets. The start time of the latest
        // bucket will be current time.
        startOfNewBucket = subtractDuration(now, totalDuration);
        numOfPadBuckets = N + 1;
      }

      for (int i = 0; i < numOfPadBuckets; i++) {
        buckets.add(
            new IntervalBucket(
                startOfNewBucket,
                bucketDuration,
                super.view.getAggregation(),
                super.view.getMeasure()));
        startOfNewBucket = startOfNewBucket.addDuration(bucketDuration);
      }

      // removed expired buckets
      while (buckets.size() > N + 1) {
        buckets.pollFirst();
      }
    }

    // Combine stats within each bucket, aggregate stats by tag values, and return the mapping from
    // tag values to aggregation data.
    private Map<List</*@Nullable*/ TagValue>, AggregationData> combineBucketsAndGetAggregationMap(
        Timestamp now) {
      // Need to maintain the order of inserted MutableAggregations (inserted based on time order).
      Multimap<List</*@Nullable*/ TagValue>, MutableAggregation> multimap =
          LinkedHashMultimap.create();

      ArrayDeque<IntervalBucket> shallowCopy = new ArrayDeque<IntervalBucket>(buckets);

      Aggregation aggregation = super.view.getAggregation();
      Measure measure = super.view.getMeasure();
      putBucketsIntoMultiMap(shallowCopy, multimap, aggregation, measure, now);
      Map<List</*@Nullable*/ TagValue>, MutableAggregation> singleMap =
          aggregateOnEachTagValueList(multimap, aggregation, measure);
      return createAggregationMap(singleMap, super.getView().getMeasure());
    }

    // Put stats within each bucket to a multimap. Each tag value list (map key) could have multiple
    // mutable aggregations (map value) from different buckets.
    private static void putBucketsIntoMultiMap(
        ArrayDeque<IntervalBucket> buckets,
        Multimap<List</*@Nullable*/ TagValue>, MutableAggregation> multimap,
        Aggregation aggregation,
        Measure measure,
        Timestamp now) {
      // Put fractional stats of the head (oldest) bucket.
      IntervalBucket head = CheckerFrameworkUtils.castNonNull(buckets.peekFirst());
      IntervalBucket tail = CheckerFrameworkUtils.castNonNull(buckets.peekLast());
      double fractionTail = tail.getFraction(now);
      // TODO(songya): decide what to do when time goes backwards
      checkArgument(
          0.0 <= fractionTail && fractionTail <= 1.0,
          "Fraction " + fractionTail + " should be within [0.0, 1.0].");
      double fractionHead = 1.0 - fractionTail;
      putFractionalMutableAggregationsToMultiMap(
          head.getTagValueAggregationMap(), multimap, aggregation, measure, fractionHead);

      // Put whole data of other buckets.
      boolean shouldSkipFirst = true;
      for (IntervalBucket bucket : buckets) {
        if (shouldSkipFirst) {
          shouldSkipFirst = false;
          continue; // skip the first bucket
        }
        for (Entry<List</*@Nullable*/ TagValue>, MutableAggregation> entry :
            bucket.getTagValueAggregationMap().entrySet()) {
          multimap.put(entry.getKey(), entry.getValue());
        }
      }
    }

    // Put stats within one bucket into multimap, multiplied by a given fraction.
    private static <T> void putFractionalMutableAggregationsToMultiMap(
        Map<T, MutableAggregation> mutableAggrMap,
        Multimap<T, MutableAggregation> multimap,
        Aggregation aggregation,
        Measure measure,
        double fraction) {
      for (Entry<T, MutableAggregation> entry : mutableAggrMap.entrySet()) {
        // Initially empty MutableAggregations.
        MutableAggregation fractionalMutableAgg = createMutableAggregation(aggregation, measure);
        fractionalMutableAgg.combine(entry.getValue(), fraction);
        multimap.put(entry.getKey(), fractionalMutableAgg);
      }
    }

    // For each tag value list (key of AggregationMap), combine mutable aggregations into one
    // mutable aggregation, thus convert the multimap into a single map.
    private static <T> Map<T, MutableAggregation> aggregateOnEachTagValueList(
        Multimap<T, MutableAggregation> multimap, Aggregation aggregation, Measure measure) {
      Map<T, MutableAggregation> map = Maps.newHashMap();
      for (T tagValues : multimap.keySet()) {
        // Initially empty MutableAggregations.
        MutableAggregation combinedAggregation = createMutableAggregation(aggregation, measure);
        for (MutableAggregation mutableAggregation : multimap.get(tagValues)) {
          combinedAggregation.combine(mutableAggregation, 1.0);
        }
        map.put(tagValues, combinedAggregation);
      }
      return map;
    }

    // Subtract a Duration from a Timestamp, and return a new Timestamp.
    private static Timestamp subtractDuration(Timestamp timestamp, Duration duration) {
      return timestamp.addDuration(Duration.create(-duration.getSeconds(), -duration.getNanos()));
    }
  }

  private static final class CreateCumulative
      implements Function<View.AggregationWindow.Cumulative, MutableViewData> {
    @Override
    public MutableViewData apply(View.AggregationWindow.Cumulative arg) {
      return new CumulativeMutableViewData(view, start);
    }

    private final View view;
    private final Timestamp start;

    private CreateCumulative(View view, Timestamp start) {
      this.view = view;
      this.start = start;
    }
  }

  private static final class CreateInterval
      implements Function<View.AggregationWindow.Interval, MutableViewData> {
    @Override
    public MutableViewData apply(View.AggregationWindow.Interval arg) {
      return new IntervalMutableViewData(view, start);
    }

    private final View view;
    private final Timestamp start;

    private CreateInterval(View view, Timestamp start) {
      this.view = view;
      this.start = start;
    }
  }
}
