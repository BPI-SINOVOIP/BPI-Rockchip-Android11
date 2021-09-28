/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.google.currysrc.api.process;

import com.google.currysrc.api.match.SourceMatchers;

/**
 * Basic utility methods for working with {@link Rule} instances.
 */
public class Rules {

  private Rules() {
  }

  /**
   * Create a mandatory rule that applies the {@link Processor} to all source files.
   *
   * <p>A mandatory rule is one which must result in a change to every file to which it applies and
   * will fail the transformation otherwise.
   */
  public static Rule createMandatoryRule(Processor processor) {
    return new DefaultRule(processor, SourceMatchers.all(), true /* mustModify */);
  }

  /**
   * Create an optional rule that applies the {@link Processor} to all the source files.
   *
   * <p>An optional rule does not require that it modifies every file to which it is applied.</p>
   */
  public static Rule createOptionalRule(Processor processor) {
    return new DefaultRule(processor, SourceMatchers.all(), false /* mustModify */);
  }
}
