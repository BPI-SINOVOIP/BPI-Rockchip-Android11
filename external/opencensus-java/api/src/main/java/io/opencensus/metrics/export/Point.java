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

import com.google.auto.value.AutoValue;
import io.opencensus.common.ExperimentalApi;
import io.opencensus.common.Timestamp;
import javax.annotation.concurrent.Immutable;

/**
 * A timestamped measurement of a {@code TimeSeries}.
 *
 * @since 0.17
 */
@ExperimentalApi
@AutoValue
@Immutable
public abstract class Point {

  Point() {}

  /**
   * Creates a {@link Point}.
   *
   * @param value the {@link Value} of this {@link Point}.
   * @param timestamp the {@link Timestamp} when this {@link Point} was recorded.
   * @return a {@code Point}.
   * @since 0.17
   */
  public static Point create(Value value, Timestamp timestamp) {
    return new AutoValue_Point(value, timestamp);
  }

  /**
   * Returns the {@link Value}.
   *
   * @return the {@code Value}.
   * @since 0.17
   */
  public abstract Value getValue();

  /**
   * Returns the {@link Timestamp} when this {@link Point} was recorded.
   *
   * @return the {@code Timestamp}.
   * @since 0.17
   */
  public abstract Timestamp getTimestamp();
}
