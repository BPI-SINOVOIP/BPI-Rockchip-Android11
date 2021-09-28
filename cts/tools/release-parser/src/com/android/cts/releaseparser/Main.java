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

package com.android.cts.releaseparser;

import com.android.cts.releaseparser.ReleaseProto.*;
import com.google.protobuf.TextFormat;

import java.io.FileOutputStream;
import java.nio.charset.Charset;
import java.nio.file.Paths;
import java.util.logging.Logger;

/** Main of release parser */
public class Main {
    private Main() {}

    private static final String USAGE_MESSAGE =
            "Usage: java -jar releaseparser.jar [-options <parameter>]...\n"
                    + "\tto prase a release, such as device build, test suite or app distribution package\n"
                    + "Options:\n"
                    + "\t-i PATH\t path to a release folder\n"
                    + "\t-o PATH\t path to output files\n";

    public static void main(final String[] args) {
        try {
            ArgumentParser argParser = new ArgumentParser(args);
            String relFolder = argParser.getParameterElement("i", 0);
            String outputPath = argParser.getParameterElement("o", 0);

            // parse a release folder
            ReleaseParser relParser = new ReleaseParser(relFolder);
            String relNameVer = relParser.getReleaseId();
            relParser.writeRelesaeContentCsvFile(
                    relNameVer, getPathString(outputPath, "%s-ReleaseContent.csv", relNameVer));

            // write release content JSON file
            JsonPrinter jPrinter =
                    new JsonPrinter(
                            relParser.getReleaseContent(),
                            getPathString(outputPath, "%s", relNameVer));
            jPrinter.write();

            // Write release content message to disk.
            ReleaseContent relContent = relParser.getReleaseContent();
            FileOutputStream output =
                    new FileOutputStream(
                            getPathString(outputPath, "%s-ReleaseContent.pb", relNameVer));
            relContent.writeTo(output);
            output.flush();
            output.close();

            FileOutputStream txtOutput =
                    new FileOutputStream(
                            getPathString(outputPath, "%s-ReleaseContent.txt", relNameVer));
            txtOutput.write(
                    TextFormat.printToString(relContent).getBytes(Charset.forName("UTF-8")));
            txtOutput.flush();
            txtOutput.close();

            // parse Test Suite
            TestSuiteParser tsParser = new TestSuiteParser(relContent, relFolder);
            if (tsParser.getTestSuite().getModulesList().size() == 0) {
                // skip if no test module
                return;
            }

            // write Known Failus & etc. CSV files
            relParser.writeKnownFailureCsvFile(
                    relNameVer, getPathString(outputPath, "%s-KnownFailure.csv", relNameVer));
            tsParser.writeCsvFile(
                    relNameVer, getPathString(outputPath, "%s-TestCase.csv", relNameVer));
            tsParser.writeModuleCsvFile(
                    relNameVer, getPathString(outputPath, "%s-TestModule.csv", relNameVer));

            // Write test suite content message to disk.
            TestSuite testSuite = tsParser.getTestSuite();
            FileOutputStream tsOutput =
                    new FileOutputStream(getPathString(outputPath, "%s-TestSuite.pb", relNameVer));
            testSuite.writeTo(tsOutput);
            tsOutput.flush();
            tsOutput.close();
        } catch (Exception ex) {
            System.out.println(USAGE_MESSAGE);
            ex.printStackTrace();
        }
    }

    public static String getPathString(String outputPath, String format, String id) {
        return Paths.get(outputPath, String.format(format, id)).toString();
    }

    private static Logger getLogger() {
        return Logger.getLogger(Main.class.getSimpleName());
    }
}