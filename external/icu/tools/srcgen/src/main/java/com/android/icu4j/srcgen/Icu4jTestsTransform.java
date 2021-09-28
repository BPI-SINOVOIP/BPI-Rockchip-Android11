/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.icu4j.srcgen;

import com.google.common.collect.Lists;
import com.google.currysrc.Main;
import com.google.currysrc.api.RuleSet;
import com.google.currysrc.api.input.InputFileGenerator;
import com.google.currysrc.api.output.OutputSourceFileGenerator;
import com.google.currysrc.api.process.Rule;
import com.google.currysrc.processors.ReplaceTextCommentScanner;
import com.ibm.icu.text.Transliterator;
import com.ibm.icu.util.VersionInfo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static com.google.currysrc.api.process.Rules.createOptionalRule;

/**
 * Applies Android's ICU4J source code transformation rules to test code, adds @RunWith annotations
 * to test so that they can be run with JUnit and fixes up the jcite start/end tags.
 *
 * <p>Intended for use when transforming test code.
 */
public class Icu4jTestsTransform {

  private static final boolean DEBUG = false;

  private Icu4jTestsTransform() {
  }

  /**
   * Usage:
   * java com.android.icu4j.srcgen.Icu4jSampleTransform {source files/directories} {target dir}
   */
  public static void main(String[] args) throws Exception {
    new Main(DEBUG).execute(new Icu4jBasicRules(args));
    String outputDirName = args[args.length - 1];
    writeAndroidIcuVersionPropertyFile(outputDirName);
    writeExpectedTransliterationIdsFile(outputDirName);
  }

  private static class Icu4jBasicRules implements RuleSet {

    private final InputFileGenerator inputFileGenerator;

    private final List<Rule> rules;

    private final OutputSourceFileGenerator outputSourceFileGenerator;

    public Icu4jBasicRules(String[] args) {
      if (args.length < 2) {
        throw new IllegalArgumentException("At least 2 arguments required.");
      }

      String[] inputDirNames = new String[args.length - 1];
      System.arraycopy(args, 0, inputDirNames, 0, args.length - 1);
      inputFileGenerator = Icu4jTransformRules.createInputFileGenerator(inputDirNames);
      rules = createTransformRules();
      String outputDirName = args[args.length - 1];
      outputSourceFileGenerator = Icu4jTransformRules.createOutputFileGenerator(outputDirName);
    }



    @Override
    public List<Rule> getRuleList(File ignored) {
      return rules;
    }

    @Override
    public InputFileGenerator getInputFileGenerator() {
      return inputFileGenerator;
    }

    @Override
    public OutputSourceFileGenerator getOutputSourceFileGenerator() {
      return outputSourceFileGenerator;
    }

    private static List<Rule> createTransformRules() {
      List<Rule> rules =
              Lists.newArrayList(Icu4jTransform.Icu4jRules.getRepackagingRules());

      // Switch all embedded comment references from com.ibm.icu to android.icu.
      rules.add(
          createOptionalRule(new ReplaceTextCommentScanner(
              Icu4jTransform.ORIGINAL_ICU_PACKAGE, Icu4jTransform.ANDROID_ICU_PACKAGE)));

      // Change sample jcite begin / end tags ---XYZ to Androids 'BEGIN(XYZ)' / 'END(XYZ)'
      rules.add(createOptionalRule(new TranslateJcite.BeginEndTagsHandler()));

      // Add annotations to each test file so that they can be sharded across multiple processes.
      rules.add(createOptionalRule(new ShardingAnnotator()));

      return rules;
    }
  }

  /**
   * Generate a property file containing ICU version for AndroidICUVersionTest
   */
  private static void writeAndroidIcuVersionPropertyFile(String outputDirName) throws IOException {
    String FILE_NAME = "android_icu_version.properties";
    String VERSION_PROP_NAME = "version";
    File dir = new File(outputDirName, "android/icu/extratest/");

    dir.mkdirs();
    try (PrintWriter writer = new PrintWriter(new FileOutputStream(new File(dir, FILE_NAME)))) {
      // Use PrintWriter instead of Properties to avoid writing the current timestamp
      // which would cause the file content to change every time it is generated.
      writer.println("# Property file for AndroidICUVersionTest.");
      writer.println(VERSION_PROP_NAME + "=" + VersionInfo.ICU_VERSION.toString());
    }
  }

  /**
   * The list of transliteration IDs that ICU provides but Android does not require.
   *
   * <p>If a transliteration id provided by ICU should be considered optional then add it to this
   * list.
   *
   * <p>The list in expected_transliteration_id_list.txt should be reviewed during each ICU
   * upgrade and any new IDs that are not required should be added to the list below to stop them
   * being required by the android.icu.extratest.AndroidTransliteratorAvailableIdsTest.
   */
  private static final Set<String> EXCLUDED_TRANSLITERATION_IDS = new HashSet<>(Arrays.asList(
           /* Deliberately empty: No IDs are currently optional. */
        ));

  /**
   * Generate a list of translieration ids for AndroidTransliteratorTest
   */
  private static void writeExpectedTransliterationIdsFile(String outputDirName) throws IOException {
    String FILE_NAME = "expected_transliteration_id_list.txt";
    File dir = new File(outputDirName, "android/icu/extratest/");

    dir.mkdirs();
    try (PrintWriter writer = new PrintWriter(new FileOutputStream(new File(dir, FILE_NAME)))) {
      for(String id : Collections.list(Transliterator.getAvailableIDs())) {
        if (!EXCLUDED_TRANSLITERATION_IDS.contains(id)) {
          writer.println(id);
        }
      }
    }
  }
}
