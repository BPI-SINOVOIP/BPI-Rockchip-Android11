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
package com.android.icu4j.srcgen.checker;

import com.android.icu4j.srcgen.Icu4jTransformRules;
import com.google.currysrc.Main;
import com.google.currysrc.api.input.InputFileGenerator;

import java.io.File;
import java.io.PrintStream;
import java.util.List;

public class GeneratePublicApiReport {
  private static final boolean DEBUG = false;

  private GeneratePublicApiReport() {
  }

  /**
   * Usage:
   * java com.android.icu4j.srcgen.checker.GeneratePublicApiReport
   *   {android_icu4j src main directories} {report output file path}
   */
  public static void main(String[] args) throws Exception {
    if (args.length < 2) {
      throw new IllegalArgumentException("At least 2 argument required.");
    }

    Main main = new Main(DEBUG);

    // We assume we only need to look at ICU4J code for this for both passes.
    String[] inputDirs = new String[args.length - 1];
    System.arraycopy(args, 0, inputDirs, 0, inputDirs.length);
    InputFileGenerator inputFileGenerator = Icu4jTransformRules.createInputFileGenerator(
            inputDirs);

    System.out.println("Establishing Android public ICU4J API");
    RecordPublicApiRules recordPublicApiRulesRules = new RecordPublicApiRules(
            inputFileGenerator);
    main.execute(recordPublicApiRulesRules);
    List<String> publicMemberLocatorStrings = recordPublicApiRulesRules.publicMembers();
    File outputReportFile = new File(args[args.length - 1]);
    try (PrintStream report = new PrintStream(outputReportFile)) {
      for (String publicMemberLocatorString : publicMemberLocatorStrings) {
        report.println(publicMemberLocatorString);
      }
    }

    System.out.println("Report file: " + outputReportFile);
  }
}
