/*
 * Copyright 2014 The gRPC Authors
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

package io.grpc.okhttp;

import io.grpc.okhttp.internal.framed.Settings;

/**
 * A utility class help gRPC get/set the necessary fields of OkHttp's Settings.
 */
class OkHttpSettingsUtil {
  public static final int MAX_CONCURRENT_STREAMS = Settings.MAX_CONCURRENT_STREAMS;
  public static final int INITIAL_WINDOW_SIZE = Settings.INITIAL_WINDOW_SIZE;

  public static boolean isSet(Settings settings, int id) {
    return settings.isSet(id);
  }

  public static int get(Settings settings, int id) {
    return settings.get(id);
  }

  public static void set(Settings settings, int id, int value) {
    settings.set(id, 0, value);
  }
}
