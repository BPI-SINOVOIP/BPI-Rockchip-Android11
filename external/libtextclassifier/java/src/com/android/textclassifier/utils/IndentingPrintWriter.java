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

package com.android.textclassifier.utils;

import com.google.common.base.Preconditions;
import java.io.PrintWriter;

/**
 * A print writer that supports indentation.
 *
 * @see PrintWriter
 */
public final class IndentingPrintWriter {
  static final String SINGLE_INDENT = "  ";

  private final PrintWriter writer;
  private final StringBuilder indentBuilder = new StringBuilder();
  private String currentIndent = "";

  public IndentingPrintWriter(PrintWriter writer) {
    this.writer = Preconditions.checkNotNull(writer);
  }

  /** Prints a string. */
  public IndentingPrintWriter println(String string) {
    writer.print(currentIndent);
    writer.print(string);
    writer.println();
    return this;
  }

  /** Prints a empty line */
  public IndentingPrintWriter println() {
    writer.println();
    return this;
  }

  /** Increases indents for subsequent texts. */
  public IndentingPrintWriter increaseIndent() {
    indentBuilder.append(SINGLE_INDENT);
    currentIndent = indentBuilder.toString();
    return this;
  }

  /** Decreases indents for subsequent texts. */
  public IndentingPrintWriter decreaseIndent() {
    indentBuilder.delete(0, SINGLE_INDENT.length());
    currentIndent = indentBuilder.toString();
    return this;
  }

  /** Prints a key-valued pair. */
  public IndentingPrintWriter printPair(String key, Object value) {
    println(String.format("%s=%s", key, String.valueOf(value)));
    return this;
  }

  /** Flushes the stream. */
  public void flush() {
    writer.flush();
  }
}
