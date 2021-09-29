/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
package com.google.android.jacoco.reporter;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.jacoco.core.analysis.Analyzer;
import org.jacoco.core.analysis.CoverageBuilder;
import org.jacoco.core.analysis.IBundleCoverage;
import org.jacoco.core.data.ExecutionDataStore;
import org.jacoco.core.tools.ExecFileLoader;
import org.jacoco.report.DirectorySourceFileLocator;
import org.jacoco.report.FileMultiReportOutput;
import org.jacoco.report.IMultiReportOutput;
import org.jacoco.report.IReportVisitor;
import org.jacoco.report.MultiReportVisitor;
import org.jacoco.report.MultiSourceFileLocator;
import org.jacoco.report.html.HTMLFormatter;
import org.jacoco.report.xml.XMLFormatter;
import org.objectweb.asm.ClassReader;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarFile;
import java.util.stream.Collectors;

public class ReportGenerator {
    private static final String OPT_CLASSPATH = "classpath";
    private static final String OPT_REPORT_NAME = "name";
    private static final String OPT_EXEC_FILE = "exec-file";
    private static final String OPT_SOURCES = "srcs";
    private static final String OPT_SRCJARS = "srcjars";
    private static final String OPT_REPORT_DIR = "report-dir";
    private static final int TAB_WIDTH = 4;

    private final Config mConfig;

    private ReportGenerator(Config config) {
        mConfig = config;
    }

    private void execute() throws IOException {
        ExecFileLoader execFileLoader = new ExecFileLoader();
        execFileLoader.load(mConfig.mExecFileDir);
        IReportVisitor reportVisitor = new MultiReportVisitor(getVisitors());
        reportVisitor.visitInfo(execFileLoader.getSessionInfoStore().getInfos(),
                execFileLoader.getExecutionDataStore().getContents());
        MultiSourceFileLocator sourceFileLocator = new MultiSourceFileLocator(TAB_WIDTH);
        mConfig.mSourceDirs.stream().filter(File::isDirectory)
                .map(sourceDir -> new DirectorySourceFileLocator(sourceDir, null, TAB_WIDTH))
                .forEach(sourceFileLocator::add);
        mConfig.mSrcJars.stream()
                .map(srcJar -> new JarSourceFileLocator(srcJar, null, TAB_WIDTH))
                .forEach(sourceFileLocator::add);
        reportVisitor.visitBundle(createBundle(execFileLoader.getExecutionDataStore()),
                sourceFileLocator);
        reportVisitor.visitEnd();
    }

    private IBundleCoverage createBundle(ExecutionDataStore dataStore) throws IOException {
        CoverageBuilder coverageBuilder = new CoverageBuilder();
        Analyzer analyzer = new Analyzer(dataStore, coverageBuilder) {
            @Override
            public void analyzeClass(ClassReader reader) {
                if (weHaveSourceFor(reader.getClassName())) {
                    super.analyzeClass(reader);
                }
            }

            private boolean weHaveSourceFor(String asmClassName) {
                String fileName = asmClassName.replaceFirst("\\$.*", "") + ".java";
                return mConfig.mSourceDirs.stream().map(parent -> new File(parent, fileName))
                        .anyMatch(File::exists) ||
                        mConfig.mSrcJars.stream().anyMatch(srcJar -> srcJar.stream().anyMatch(
                                it -> it.getName().endsWith(fileName)));
            }
        };

        for (File file : mConfig.mClasspath) {
            analyzer.analyzeAll(file);
        }
        return coverageBuilder.getBundle(mConfig.mReportName);
    }

    private List<IReportVisitor> getVisitors() throws IOException {
        List<IReportVisitor> visitors = new ArrayList<>();
        visitors.add(new XMLFormatter().createVisitor(mConfig.getXmlOutputStream()));
        visitors.add(new HTMLFormatter().createVisitor(mConfig.getHtmlReportOutput()));
        return visitors;
    }

    private static class Config {
        final String mReportName;
        final List<File> mClasspath;
        final List<File> mSourceDirs;
        final List<JarFile> mSrcJars;
        final File mReportDir;
        final File mExecFileDir;

        Config(String reportName, List<File> classpath, List<File> sourceDirs,
                List<JarFile> srcJars, File reportDir, File execFileDir) {
            mReportName = reportName;
            mClasspath = classpath;
            mSourceDirs = sourceDirs;
            mSrcJars = srcJars;
            mReportDir = reportDir;
            mExecFileDir = execFileDir;
        }

        FileOutputStream getXmlOutputStream() throws FileNotFoundException {
            File xmlFile = new File(mReportDir, "coverage.xml");
            return new FileOutputStream(xmlFile);
        }

        static Config from(CommandLine commandLine) {
            List<File> classpaths = parse(commandLine.getOptionValue(OPT_CLASSPATH),
                    "WARN: Classpath entry [%s] does not exist or is not a directory");
            List<File> sources = parse(commandLine.getOptionValue(OPT_SOURCES),
                    "WARN: Source entry [%s] does not exist or is not a directory");
            List<File> srcJars = parse(commandLine.getOptionValue(OPT_SRCJARS),
                    "WARN: srcjars entry [%s] does not exist");

            ensure(!sources.isEmpty() || !srcJars.isEmpty(),
                    "--%s or --%s argument is required", OPT_SOURCES, OPT_SRCJARS);

            List<JarFile> srcJarFiles = srcJars.stream().filter(File::exists)
                    .map(srcJar -> {
                        try {
                            return new JarFile(srcJar);
                        } catch (IOException e) {
                            System.out.printf("WARN: Failed to open srcjars file [%s]", srcJar);
                            return null;
                        }
                    }).filter(jarFile -> jarFile != null).collect(Collectors.toList());

            File execFileDir = new File(commandLine.getOptionValue(OPT_EXEC_FILE));
            ensure(execFileDir.exists() && execFileDir.canRead() && execFileDir.isFile(),
                    "execFile: [%s] does not exist or could not be read.", execFileDir);

            File reportDir = new File(commandLine.getOptionValue(OPT_REPORT_DIR));
            ensure(reportDir.exists() || reportDir.mkdirs(),
                    "Unable to create report dir [%s]", reportDir);

            return new Config(commandLine.getOptionValue(OPT_REPORT_NAME), classpaths, sources,
                    srcJarFiles, reportDir, execFileDir);
        }

        IMultiReportOutput getHtmlReportOutput() {
            return new FileMultiReportOutput(mReportDir);
        }
    }

    private static void ensure(boolean condition, String errorMessage, Object... args) {
        if (!condition) {
            System.err.printf(errorMessage, args);
            System.exit(0);
        }
    }

    private static List<File> parse(String value, String warningMessage) {
        List<File> files = new ArrayList<>(0);
        if (value != null) {
            for (String classpath : value.split(System.getProperty("path.separator"))) {
                File file = new File(classpath);
                if (file.exists()) {
                    files.add(file);
                } else {
                    System.out.println(String.format(warningMessage, classpath));
                }
            }
        }
        return files;
    }

    private static void printHelp(ParseException e, Options options) {
        System.out.println(e.getMessage());
        HelpFormatter helpFormatter = new HelpFormatter();
        helpFormatter.printHelp("java -cp ${report.jar} " + ReportGenerator.class.getName(),
                "Generates jacoco reports in XML and HTML format.", options, "", true);
    }

    private static void addOption(Options options, String longName, String description,
            boolean required) {
        Option option = new Option(null, longName, true, description);
        option.setArgs(1);
        option.setRequired(required);
        options.addOption(option);
    }

    private static void addOption(Options options, String longName, String description) {
        addOption(options, longName, description, true);
    }

    public static void main(String[] args) {
        Options options = new Options();
        try {
            // TODO: Support multiple exec-files
            addOption(options, OPT_CLASSPATH, "Classpath used during testing");
            addOption(options, OPT_EXEC_FILE, "File generated by jacoco during testing");
            addOption(options, OPT_REPORT_NAME, "Name of the project tested");
            addOption(options, OPT_REPORT_DIR, "Directory into which reports will be generated");
            addOption(options, OPT_SOURCES, "List of source directories", false);
            addOption(options, OPT_SRCJARS, "List of jars containing source files", false);
            CommandLine commandLine = new PosixParser().parse(options, args);
            new ReportGenerator(Config.from(commandLine))
                    .execute();
        } catch (ParseException e) {
            printHelp(e, options);
            System.exit(1);
        } catch (IOException e) {
            e.printStackTrace();
            System.exit(1);
        }
    }
}
