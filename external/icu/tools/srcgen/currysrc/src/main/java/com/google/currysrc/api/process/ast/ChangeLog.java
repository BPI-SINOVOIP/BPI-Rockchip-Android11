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
package com.google.currysrc.api.process.ast;

import com.google.currysrc.processors.AddAnnotation;
import com.google.currysrc.processors.AddDefaultConstructor;
import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.Locale;

/**
 * Records information about changes that were made to allow the changes to be verified.
 *
 * <p>This is intended to record information necessary for the caller of this application to
 * verify that the changes that were made were expected.
 */
public class ChangeLog implements AutoCloseable {

  private final Path path;
  private final BufferedWriter writer;

  public static ChangeLog createChangeLog(Path path) throws IOException {
    if (path == null) {
      path = Files.createTempFile("currysrc", "changelog");
    }
    return new ChangeLog(path);
  }

  private ChangeLog(Path path) throws IOException {
    this.path = path;
    // Append entries to the change log.
    writer = Files.newBufferedWriter(path, StandardOpenOption.APPEND);
  }

  private void changed(String tag, String message) {
    // Append the entry to the change log.
    try {
      writer.write(tag);
      writer.write(":");
      writer.write(message);
      writer.newLine();
    } catch (IOException e) {
      throw new IllegalStateException(
          String.format("Could not write change log entry %s:%s to file %s", tag, message, path));
    }
  }

  public AddDefaultConstructor.Listener asAddDefaultConstructorListener() {
    return (locator, typeDeclaration) -> changed("AddDefaultConstructor",
        locator.getStringFormTarget());
  }

  public AddAnnotation.Listener asAddAnnotationListener() {
    return (annotationInfo, locator, bodyDeclaration) -> changed(
        "@" + annotationInfo.getQualifiedName(),
        String.format(Locale.US, "%s:%s", locator.getStringFormType(),
            locator.getStringFormTarget()));
  }

  @Override
  public void close() throws IOException {
    writer.close();
  }
}
