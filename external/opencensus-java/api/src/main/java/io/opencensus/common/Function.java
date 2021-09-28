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

package io.opencensus.common;

/**
 * Used to specify matching functions for use encoding tagged unions (i.e. sum types) in Java. See
 * {@link io.opencensus.trace.AttributeValue#match} for an example of its use.
 *
 * <p>Note: This class is based on the java.util.Function class added in Java 1.8. We cannot use the
 * Function from Java 1.8 because this library is Java 1.6 compatible.
 *
 * @since 0.5
 */
public interface Function<A, B> {

  /**
   * Applies the function to the given argument.
   *
   * @param arg the argument to the function.
   * @return the result of the function.
   * @since 0.5
   */
  B apply(A arg);
}
