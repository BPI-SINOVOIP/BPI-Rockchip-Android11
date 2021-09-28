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

package io.opencensus.common;

/**
 * Exception thrown when a {@link ServerStats} cannot be parsed.
 *
 * @since 0.16
 */
public final class ServerStatsDeserializationException extends Exception {
  private static final long serialVersionUID = 0L;

  /**
   * Constructs a new {@code ServerStatsDeserializationException} with the given message.
   *
   * @param message a message describing the error.
   * @since 0.16
   */
  public ServerStatsDeserializationException(String message) {
    super(message);
  }

  /**
   * Constructs a new {@code ServerStatsDeserializationException} with the given message and cause.
   *
   * @param message a message describing the error.
   * @param cause the cause of the error.
   * @since 0.16
   */
  public ServerStatsDeserializationException(String message, Throwable cause) {
    super(message, cause);
  }
}
