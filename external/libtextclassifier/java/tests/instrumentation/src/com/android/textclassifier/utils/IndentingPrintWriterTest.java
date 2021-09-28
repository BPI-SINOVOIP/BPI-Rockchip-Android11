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

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import java.io.PrintWriter;
import java.io.StringWriter;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public final class IndentingPrintWriterTest {

  private static final String TEST_STRING = "sense";
  private static final String TEST_KEY = "key";
  private static final String TEST_VALUE = "value";

  private StringWriter stringWriter;
  private IndentingPrintWriter indentingPrintWriter;

  @Before
  public void setUp() {
    stringWriter = new StringWriter();
    indentingPrintWriter =
        new IndentingPrintWriter(new PrintWriter(stringWriter, /* autoFlush= */ true));
  }

  @Test
  public void println_printString_noIndent() throws Exception {
    indentingPrintWriter.println(TEST_STRING);

    assertThat(stringWriter.toString()).isEqualTo(TEST_STRING + "\n");
  }

  @Test
  public void println_printString_withIndent() throws Exception {
    indentingPrintWriter.increaseIndent().println(TEST_STRING);

    assertThat(stringWriter.toString())
        .isEqualTo(IndentingPrintWriter.SINGLE_INDENT + TEST_STRING + "\n");
  }

  @Test
  public void decreaseIndent_noIndent() throws Exception {
    indentingPrintWriter.decreaseIndent().println(TEST_STRING);

    assertThat(stringWriter.toString()).isEqualTo(TEST_STRING + "\n");
  }

  @Test
  public void decreaseIndent_withIndent() throws Exception {
    indentingPrintWriter.increaseIndent().decreaseIndent().println(TEST_STRING);

    assertThat(stringWriter.toString()).isEqualTo(TEST_STRING + "\n");
  }

  @Test
  public void printPair_singlePair() throws Exception {
    indentingPrintWriter.printPair(TEST_KEY, TEST_VALUE);

    assertThat(stringWriter.toString()).isEqualTo(TEST_KEY + "=" + TEST_VALUE + "\n");
  }
}
