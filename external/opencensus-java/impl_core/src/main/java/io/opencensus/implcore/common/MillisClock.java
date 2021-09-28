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

package io.opencensus.implcore.common;

import io.opencensus.common.Clock;
import io.opencensus.common.Timestamp;
import javax.annotation.concurrent.ThreadSafe;

/** A {@link Clock} that uses {@link System#currentTimeMillis()} and {@link System#nanoTime()}. */
@ThreadSafe
public final class MillisClock extends Clock {
  private static final MillisClock INSTANCE = new MillisClock();

  private MillisClock() {}

  /**
   * Returns a {@code MillisClock}.
   *
   * @return a {@code MillisClock}.
   */
  public static MillisClock getInstance() {
    return INSTANCE;
  }

  @Override
  public Timestamp now() {
    return Timestamp.fromMillis(System.currentTimeMillis());
  }

  @Override
  public long nowNanos() {
    return System.nanoTime();
  }
}
