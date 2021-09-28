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
 * Class holder for all common constants (such as the version) for the OpenCensus Java library.
 *
 * @since 0.8
 */
@ExperimentalApi
public final class OpenCensusLibraryInformation {

  /**
   * The current version of the OpenCensus Java library.
   *
   * @since 0.8
   */
  public static final String VERSION = "0.17.0-SNAPSHOT"; // CURRENT_OPENCENSUS_VERSION

  private OpenCensusLibraryInformation() {}
}
