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

package io.opencensus.stats;

import io.opencensus.common.Functions;
import io.opencensus.common.Timestamp;
import io.opencensus.internal.Utils;
import io.opencensus.stats.Measure.MeasureDouble;
import io.opencensus.stats.Measure.MeasureLong;
import io.opencensus.tags.TagContext;
import io.opencensus.tags.TagValue;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import javax.annotation.concurrent.GuardedBy;
import javax.annotation.concurrent.Immutable;
import javax.annotation.concurrent.ThreadSafe;

/*>>>
import org.checkerframework.checker.nullness.qual.Nullable;
*/

/** No-op implementations of stats classes. */
final class NoopStats {

  private NoopStats() {}

  /**
   * Returns a {@code StatsComponent} that has a no-op implementation for {@link StatsRecorder}.
   *
   * @return a {@code StatsComponent} that has a no-op implementation for {@code StatsRecorder}.
   */
  static StatsComponent newNoopStatsComponent() {
    return new NoopStatsComponent();
  }

  /**
   * Returns a {@code StatsRecorder} that does not record any data.
   *
   * @return a {@code StatsRecorder} that does not record any data.
   */
  static StatsRecorder getNoopStatsRecorder() {
    return NoopStatsRecorder.INSTANCE;
  }

  /**
   * Returns a {@code MeasureMap} that ignores all calls to {@link MeasureMap#put}.
   *
   * @return a {@code MeasureMap} that ignores all calls to {@code MeasureMap#put}.
   */
  static MeasureMap getNoopMeasureMap() {
    return NoopMeasureMap.INSTANCE;
  }

  /**
   * Returns a {@code ViewManager} that maintains a map of views, but always returns empty {@link
   * ViewData}s.
   *
   * @return a {@code ViewManager} that maintains a map of views, but always returns empty {@code
   *     ViewData}s.
   */
  static ViewManager newNoopViewManager() {
    return new NoopViewManager();
  }

  @ThreadSafe
  private static final class NoopStatsComponent extends StatsComponent {
    private final ViewManager viewManager = newNoopViewManager();
    private volatile boolean isRead;

    @Override
    public ViewManager getViewManager() {
      return viewManager;
    }

    @Override
    public StatsRecorder getStatsRecorder() {
      return getNoopStatsRecorder();
    }

    @Override
    public StatsCollectionState getState() {
      isRead = true;
      return StatsCollectionState.DISABLED;
    }

    @Override
    @Deprecated
    public void setState(StatsCollectionState state) {
      Utils.checkNotNull(state, "state");
      Utils.checkState(!isRead, "State was already read, cannot set state.");
    }
  }

  @Immutable
  private static final class NoopStatsRecorder extends StatsRecorder {
    static final StatsRecorder INSTANCE = new NoopStatsRecorder();

    @Override
    public MeasureMap newMeasureMap() {
      return getNoopMeasureMap();
    }
  }

  @Immutable
  private static final class NoopMeasureMap extends MeasureMap {
    static final MeasureMap INSTANCE = new NoopMeasureMap();

    @Override
    public MeasureMap put(MeasureDouble measure, double value) {
      return this;
    }

    @Override
    public MeasureMap put(MeasureLong measure, long value) {
      return this;
    }

    @Override
    public void record() {}

    @Override
    public void record(TagContext tags) {
      Utils.checkNotNull(tags, "tags");
    }
  }

  @ThreadSafe
  private static final class NoopViewManager extends ViewManager {
    private static final Timestamp ZERO_TIMESTAMP = Timestamp.create(0, 0);

    @GuardedBy("registeredViews")
    private final Map<View.Name, View> registeredViews = new HashMap<View.Name, View>();

    // Cached set of exported views. It must be set to null whenever a view is registered or
    // unregistered.
    @javax.annotation.Nullable private volatile Set<View> exportedViews;

    @Override
    public void registerView(View newView) {
      Utils.checkNotNull(newView, "newView");
      synchronized (registeredViews) {
        exportedViews = null;
        View existing = registeredViews.get(newView.getName());
        Utils.checkArgument(
            existing == null || newView.equals(existing),
            "A different view with the same name already exists.");
        if (existing == null) {
          registeredViews.put(newView.getName(), newView);
        }
      }
    }

    @Override
    @javax.annotation.Nullable
    @SuppressWarnings("deprecation")
    public ViewData getView(View.Name name) {
      Utils.checkNotNull(name, "name");
      synchronized (registeredViews) {
        View view = registeredViews.get(name);
        if (view == null) {
          return null;
        } else {
          return ViewData.create(
              view,
              Collections.<List</*@Nullable*/ TagValue>, AggregationData>emptyMap(),
              view.getWindow()
                  .match(
                      Functions.<ViewData.AggregationWindowData>returnConstant(
                          ViewData.AggregationWindowData.CumulativeData.create(
                              ZERO_TIMESTAMP, ZERO_TIMESTAMP)),
                      Functions.<ViewData.AggregationWindowData>returnConstant(
                          ViewData.AggregationWindowData.IntervalData.create(ZERO_TIMESTAMP)),
                      Functions.<ViewData.AggregationWindowData>throwAssertionError()));
        }
      }
    }

    @Override
    public Set<View> getAllExportedViews() {
      Set<View> views = exportedViews;
      if (views == null) {
        synchronized (registeredViews) {
          exportedViews = views = filterExportedViews(registeredViews.values());
        }
      }
      return views;
    }

    // Returns the subset of the given views that should be exported
    @SuppressWarnings("deprecation")
    private static Set<View> filterExportedViews(Collection<View> allViews) {
      Set<View> views = new HashSet<View>();
      for (View view : allViews) {
        if (view.getWindow() instanceof View.AggregationWindow.Interval) {
          continue;
        }
        views.add(view);
      }
      return Collections.unmodifiableSet(views);
    }
  }
}
