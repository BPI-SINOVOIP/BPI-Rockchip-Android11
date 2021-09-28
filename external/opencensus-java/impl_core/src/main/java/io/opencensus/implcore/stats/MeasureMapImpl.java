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

package io.opencensus.implcore.stats;

import io.opencensus.stats.Measure.MeasureDouble;
import io.opencensus.stats.Measure.MeasureLong;
import io.opencensus.stats.MeasureMap;
import io.opencensus.tags.TagContext;
import io.opencensus.tags.unsafe.ContextUtils;

/** Implementation of {@link MeasureMap}. */
final class MeasureMapImpl extends MeasureMap {
  private final StatsManager statsManager;
  private final MeasureMapInternal.Builder builder = MeasureMapInternal.builder();

  static MeasureMapImpl create(StatsManager statsManager) {
    return new MeasureMapImpl(statsManager);
  }

  private MeasureMapImpl(StatsManager statsManager) {
    this.statsManager = statsManager;
  }

  @Override
  public MeasureMapImpl put(MeasureDouble measure, double value) {
    builder.put(measure, value);
    return this;
  }

  @Override
  public MeasureMapImpl put(MeasureLong measure, long value) {
    builder.put(measure, value);
    return this;
  }

  @Override
  public MeasureMap putAttachment(String key, String value) {
    builder.putAttachment(key, value);
    return this;
  }

  @Override
  public void record() {
    // Use the context key directly, to avoid depending on the tags implementation.
    record(ContextUtils.TAG_CONTEXT_KEY.get());
  }

  @Override
  public void record(TagContext tags) {
    statsManager.record(tags, builder.build());
  }
}
