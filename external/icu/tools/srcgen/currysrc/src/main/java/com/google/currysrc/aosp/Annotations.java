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
package com.google.currysrc.aosp;

import com.google.currysrc.api.process.Processor;
import com.google.currysrc.api.process.ast.BodyDeclarationLocators;
import com.google.currysrc.processors.AddAnnotation;
import com.google.currysrc.processors.AnnotationInfo.AnnotationClass;
import java.io.IOException;
import java.nio.file.Path;

/**
 * Utility methods for manipulating AOSP annotations.
 */
public final class Annotations {

  private Annotations() {
  }

  /**
   * Add android.compat.annotation.UnsupportedAppUsage annotations to the body declarations
   * specified in the supplied file.
   *
   * <p>The supplied file is a JSON file consisting of a top level array containing objects of the
   * following structure:
   * <pre>{@code
   * {
   *  "@location": "<body declaration location>",
   *  "maxTargetSdk": <int>|<placeholder>,
   *  "trackingBug": <long>|<placeholder>,
   *  "expectedSignature": <string>,
   *  "publicAlternatives": <string>,
   * }
   * }</pre>
   *
   * <p>Where:
   * <ul>
   * <li>{@code <body declaration location>} is in the format expected by
   * {@link BodyDeclarationLocators#fromStringForm(String)}.</li>
   * <li>{@code <int>} and {@code <long>} are numbers that are inserted into the source as literal
   * primitive values.</li>
   * <li>{@code <string>} is a quoted JSON string that is inserted into the source as a literal
   * string.</li>
   * <li>{@code <placeholder>} is a quoted JSON string that is inserted into the source as if it
   * was a constant expression. It is used to reference constants in annotation values, e.g.
   * {@code android.compat.annotation.UnsupportedAppUsage.VERSION_CODES.P}.</li>
   * </ul>
   *
   * <p>See external/icu/tools/srcgen/unsupported-app-usage.json for an example.
   *
   * @param unsupportedAppUsagePath the location of the file.
   * @return the {@link Processor}.
   */
  public static AddAnnotation addUnsupportedAppUsage(Path unsupportedAppUsagePath) {
    AnnotationClass annotationClass = new AnnotationClass(
        "android.compat.annotation.UnsupportedAppUsage")
        .addProperty("maxTargetSdk", int.class)
        .addProperty("trackingBug", long.class)
        .addProperty("implicitMember", String.class)
        .addProperty("expectedSignature", String.class)
        .addProperty("publicAlternatives", String.class);
    try {
      return AddAnnotation.fromJsonFile(annotationClass, unsupportedAppUsagePath);
    } catch (IOException e) {
      throw new IllegalStateException("Could not read JSON from " + unsupportedAppUsagePath, e);
    }
  }
}
