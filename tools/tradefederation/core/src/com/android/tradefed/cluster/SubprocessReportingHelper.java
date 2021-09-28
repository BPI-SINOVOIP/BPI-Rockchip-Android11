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
package com.android.tradefed.cluster;

import com.android.tradefed.log.LogUtil;
import com.android.tradefed.result.LegacySubprocessResultsReporter;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.QuotationAwareTokenizer;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.ZipUtil2;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;

/**
 * A class to build a wrapper configuration file to use subprocess results reporter for a cluster
 * command.
 */
public class SubprocessReportingHelper {
    private static final String REPORTER_JAR_NAME = "subprocess-results-reporter.jar";
    private static final String CLASS_FILTER =
            String.format(
                    "(^%s|^%s|^%s|^%s|^%s).*class$",
                    "LegacySubprocessResultsReporter",
                    "SubprocessTestResultsParser",
                    "SubprocessEventHelper",
                    "SubprocessResultsReporter",
                    "ISupportGranularResults");

    /**
     * Dynamically generate extract .class file from tradefed.jar and generate new subprocess
     * results reporter jar.
     *
     * @param parentDir parent directory of subprocess results reporter jar.
     * @return subprocess result reporter jar.
     * @throws IOException
     */
    public File createSubprocessReporterJar(File parentDir) throws IOException {
        File reporterJar = new File(parentDir, REPORTER_JAR_NAME);
        File tfJar =
                new File(
                        LegacySubprocessResultsReporter.class
                                .getProtectionDomain()
                                .getCodeSource()
                                .getLocation()
                                .getPath());
        // tfJar is directory of .class file when running JUnit test from Eclipse IDE
        if (tfJar.isDirectory()) {
            Set<File> classFiles = FileUtil.findFilesObject(tfJar, CLASS_FILTER);
            Manifest manifest = new Manifest();
            createJar(reporterJar, manifest, classFiles);
        }
        // tfJar is the tradefed.jar when running with tradefed.
        else {
            File extractedJar = ZipUtil2.extractZipToTemp(tfJar, "tmp-jar");
            try {
                Set<File> classFiles = FileUtil.findFilesObject(extractedJar, CLASS_FILTER);
                File mf = FileUtil.findFile(extractedJar, "MANIFEST.MF");
                Manifest manifest = new Manifest(new FileInputStream(mf));
                createJar(reporterJar, manifest, classFiles);
            } finally {
                FileUtil.recursiveDelete(extractedJar);
            }
        }
        return reporterJar;
    }

    /**
     * Create jar file.
     *
     * @param jar jar file to be created.
     * @param manifest manifest file.
     * @throws IOException
     */
    private void createJar(File jar, Manifest manifest, Set<File> classFiles) throws IOException {
        try (JarOutputStream jarOutput = new JarOutputStream(new FileOutputStream(jar), manifest)) {
            for (File file : classFiles) {
                try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(file))) {
                    String path = file.getPath();
                    JarEntry entry = new JarEntry(path.substring(path.indexOf("com")));
                    entry.setTime(file.lastModified());
                    jarOutput.putNextEntry(entry);
                    StreamUtil.copyStreams(in, jarOutput);
                    jarOutput.closeEntry();
                }
            }
        }
    }

    /**
     * Get a new command line whose configuration argument is replaced by a newly-created wrapper
     * configuration.
     *
     * <p>The resulting command line will reference a generate XML file in parentDir and needs to
     * run from parentDir.
     *
     * @param commandLine old command line that will be run by subprocess.
     * @param port port number that subprocess should use to report results.
     * @param parentDir parent directory of new wrapper configuration.
     * @return new command line, whose first argument is wrapper config.
     * @throws IOException
     */
    public String buildNewCommandConfig(String commandLine, String port, File parentDir)
            throws IOException {
        String[] tokens = QuotationAwareTokenizer.tokenizeLine(commandLine);
        SubprocessConfigBuilder builder = new SubprocessConfigBuilder();
        builder.setWorkingDir(parentDir).setOriginalConfig(tokens[0]).setPort(port);
        File f = builder.build();
        LogUtil.CLog.i("Generating new configuration:\n %s", FileUtil.readStringFromFile(f));
        tokens[0] = f.getName();
        return QuotationAwareTokenizer.combineTokens(tokens);
    }
}
