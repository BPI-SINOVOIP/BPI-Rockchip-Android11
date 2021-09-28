/*
 * Copyright (C) 2011 The Android Open Source Project
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

package util.build;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Main class to generate data from the test suite to later run from a shell
 * script. the project's home folder.<br>
 * <project-home>/src must contain the java sources<br>
 * <project-home>/src/<for-each-package>/Main_testN1.java will be generated<br>
 * (one Main class for each test method in the Test_... class
 */
public class BuildDalvikSuite extends BuildUtilBase {

    public static final String TARGET_MAIN_FILE = "mains.jar";

    // the folder for the generated junit-files for the cts host (which in turn
    // execute the real vm tests using adb push/shell etc)
    private String OUTPUT_FOLDER = "";
    private String COMPILED_CLASSES_FOLDER = "";

    private String JAVASRC_FOLDER;

    /**
     * @param args
     *            args 0 must be the project root folder (where src, lib etc.
     *            resides)
     * @throws IOException
     */
    public static void main(String[] args) throws IOException {
        BuildDalvikSuite cat = new BuildDalvikSuite();
        if (!cat.parseArgs(args)) {
          printUsage();
          System.exit(-1);
        }

        long start = System.currentTimeMillis();
        cat.run(null);
        long end = System.currentTimeMillis();

        System.out.println("elapsed seconds: " + (end - start) / 1000);
    }

    private boolean parseArgs(String[] args) {
      if (args.length == 3) {
          JAVASRC_FOLDER = args[0];
          OUTPUT_FOLDER = args[1];
          COMPILED_CLASSES_FOLDER = args[2];
          return true;
      } else {
          return false;
      }
    }

    private static void printUsage() {
        System.out.println("usage: java-src-folder output-folder classpath " +
                           "generated-main-files compiled_output");
    }

    class MyTestHandler implements TestHandler {
        public String datafileContent = "";
        Set<BuildStep> targets = new TreeSet<BuildStep>();

        @Override
        public void handleTest(String fqcn, List<String> methods) {
            int lastDotPos = fqcn.lastIndexOf('.');
            String pName = fqcn.substring(0, lastDotPos);
            String classOnlyName = fqcn.substring(lastDotPos + 1);

            Collections.sort(methods, new Comparator<String>() {
                @Override
                public int compare(String s1, String s2) {
                    // TODO sort according: test ... N, B, E, VFE
                    return s1.compareTo(s2);
                }
            });
            for (String method : methods) {
                // e.g. testN1
                if (!method.startsWith("test")) {
                    throw new RuntimeException("no test method: " + method);
                }

                // generate the Main_xx java class

                // a Main_testXXX.java contains:
                // package <packagenamehere>;
                // public class Main_testxxx {
                // public static void main(String[] args) {
                // new dxc.junit.opcodes.aaload.Test_aaload().testN1();
                // }
                // }
                MethodData md = parseTestMethod(pName, classOnlyName, method);
                String methodContent = md.methodBody;

                List<String> dependentTestClassNames = parseTestClassName(pName,
                        classOnlyName, methodContent);

                if (dependentTestClassNames.isEmpty()) {
                    continue;
                }

                // prepare the entry in the data file for the bash script.
                // e.g.
                // main class to execute; opcode/constraint; test purpose
                // dxc.junit.opcodes.aaload.Main_testN1;aaload;normal case test
                // (#1)

                char ca = method.charAt("test".length()); // either N,B,E,
                // or V (VFE)
                String comment;
                switch (ca) {
                case 'N':
                    comment = "Normal #" + method.substring(5);
                    break;
                case 'B':
                    comment = "Boundary #" + method.substring(5);
                    break;
                case 'E':
                    comment = "Exception #" + method.substring(5);
                    break;
                case 'V':
                    comment = "Verifier #" + method.substring(7);
                    break;
                default:
                    throw new RuntimeException("unknown test abbreviation:"
                            + method + " for " + fqcn);
                }

                String line = pName + ".Main_" + method + ";";
                for (String className : dependentTestClassNames) {
                    line += className + " ";
                }


                // test description
                String[] pparts = pName.split("\\.");
                // detail e.g. add_double
                String detail = pparts[pparts.length-1];
                // type := opcode | verify
                String type = pparts[pparts.length-2];

                String description;
                if ("format".equals(type)) {
                    description = "format";
                } else if ("opcodes".equals(type)) {
                    // Beautify name, so it matches the actual mnemonic
                    detail = detail.replaceAll("_", "-");
                    detail = detail.replace("-from16", "/from16");
                    detail = detail.replace("-high16", "/high16");
                    detail = detail.replace("-lit8", "/lit8");
                    detail = detail.replace("-lit16", "/lit16");
                    detail = detail.replace("-4", "/4");
                    detail = detail.replace("-16", "/16");
                    detail = detail.replace("-32", "/32");
                    detail = detail.replace("-jumbo", "/jumbo");
                    detail = detail.replace("-range", "/range");
                    detail = detail.replace("-2addr", "/2addr");

                    // Unescape reserved words
                    detail = detail.replace("opc-", "");

                    description = detail;
                } else if ("verify".equals(type)) {
                    description = "verifier";
                } else {
                    description = type + " " + detail;
                }

                String details = (md.title != null ? md.title : "");
                if (md.constraint != null) {
                    details = " Constraint " + md.constraint + ", " + details;
                }
                if (details.length() != 0) {
                    details = details.substring(0, 1).toUpperCase()
                            + details.substring(1);
                }

                line += ";" + description + ";" + comment + ";" + details;

                datafileContent += line + "\n";
                generateBuildStepFor(dependentTestClassNames, targets);
            }
        }
    }

    @Override
    protected void handleTests(JUnitTestCollector tests, TestHandler ignored) {
        MyTestHandler handler = new MyTestHandler();
        super.handleTests(tests, handler);

        File scriptDataDir = new File(OUTPUT_FOLDER + "/data/");
        scriptDataDir.mkdirs();
        writeToFile(new File(scriptDataDir, "scriptdata"), handler.datafileContent);

        for (BuildStep buildStep : handler.targets) {
            if (!buildStep.build()) {
                System.out.println("building failed. buildStep: " +
                        buildStep.getClass().getName() + ", " + buildStep);
                System.exit(1);
            }
        }
    }

    private void generateBuildStepFor(Collection<String> dependentTestClassNames,
            Set<BuildStep> targets) {
        for (String dependentTestClassName : dependentTestClassNames) {
            generateBuildStepForDependant(dependentTestClassName, targets);
        }
    }

    private void generateBuildStepForDependant(String dependentTestClassName,
            Set<BuildStep> targets) {

        File sourceFolder = new File(JAVASRC_FOLDER);
        String fileName = dependentTestClassName.replace('.', '/').trim();

        if (new File(sourceFolder, fileName + ".dfh").exists()) {
            // Handled in vmtests-dfh-dex-generated build rule.
            return;
        }

        if (new File(sourceFolder, fileName + ".d").exists()) {
            // Handled in vmtests-dasm-dex-generated build rule.
            return;
        }

        {
            // Build dex from a single '*.smali' file or a *.smalis' dir
            // containing multiple smali files.
            File dexFile = null;
            SmaliBuildStep buildStep = null;
            File smaliFile = new File(sourceFolder, fileName + ".smali");
            File smalisDir = new File(sourceFolder, fileName + ".smalis");

            if (smaliFile.exists()) {
                dexFile = new File(OUTPUT_FOLDER, fileName + ".dex");
                buildStep = new SmaliBuildStep(
                    Collections.singletonList(smaliFile.getAbsolutePath()), dexFile);
            } else if (smalisDir.exists() && smalisDir.isDirectory()) {
                List<String> inputFiles = new ArrayList<>();
                for (File f: smalisDir.listFiles()) {
                    inputFiles.add(f.getAbsolutePath());
                }
                dexFile = new File(OUTPUT_FOLDER, fileName + ".dex");
                buildStep = new SmaliBuildStep(inputFiles, dexFile);
            }

            if (buildStep != null) {
                BuildStep.BuildFile jarFile = new BuildStep.BuildFile(
                        OUTPUT_FOLDER, fileName + ".jar");
                JarBuildStep jarBuildStep = new JarBuildStep(new BuildStep.BuildFile(dexFile),
                        "classes.dex", jarFile, true);
                jarBuildStep.addChild(buildStep);
                targets.add(jarBuildStep);
                return;
            }
        }

        File srcFile = new File(sourceFolder, fileName + ".java");
        if (srcFile.exists()) {
            BuildStep dexBuildStep;
            dexBuildStep = generateDexBuildStep(
              COMPILED_CLASSES_FOLDER, fileName);
            targets.add(dexBuildStep);
            return;
        }

        try {
            if (Class.forName(dependentTestClassName) != null) {
                BuildStep dexBuildStep = generateDexBuildStep(
                    COMPILED_CLASSES_FOLDER, fileName);
                targets.add(dexBuildStep);
                return;
            }
        } catch (ClassNotFoundException e) {
            // do nothing
        }

        throw new RuntimeException("neither .dfh,.d,.java file of dependant test class found : " +
                dependentTestClassName + ";" + fileName);
    }

    private BuildStep generateDexBuildStep(String classFileFolder,
            String classFileName) {
        BuildStep.BuildFile classFile = new BuildStep.BuildFile(
                classFileFolder, classFileName + ".class");

        BuildStep.BuildFile tmpJarFile = new BuildStep.BuildFile(
                OUTPUT_FOLDER,
                classFileName + "_tmp.jar");

        JarBuildStep jarBuildStep = new JarBuildStep(classFile,
                classFileName + ".class", tmpJarFile, false);

        BuildStep.BuildFile outputFile = new BuildStep.BuildFile(
                OUTPUT_FOLDER,
                classFileName + ".jar");

        D8BuildStep dexBuildStep = new D8BuildStep(tmpJarFile,
                outputFile,
                true);

        dexBuildStep.addChild(jarBuildStep);
        return dexBuildStep;
    }
}
