/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.setupcompat.util;

import java.util.Arrays;

/** The util for {@link java.util.Objects} method to suitable in all sdk version. */
public final class ObjectUtils {

  private ObjectUtils() {}

  /** Copied from {@link java.util.Objects#hash(Object...)} */
  public static int hashCode(Object... args) {
    return Arrays.hashCode(args);
  }

  /** Copied from {@link java.util.Objects#equals(Object, Object)} */
  public static boolean equals(Object a, Object b) {
    return (a == b) || (a != null && a.equals(b));
  }
}
