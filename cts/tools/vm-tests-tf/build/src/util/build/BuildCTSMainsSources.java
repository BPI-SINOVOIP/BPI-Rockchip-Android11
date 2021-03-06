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
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Main class to generate data from the test suite to later run from a shell
 * script. the project's home folder.<br>
 * <project-home>/src must contain the java sources<br>
 * <project-home>/src/<for-each-package>/Main_testN1.java will be generated<br>
 * (one Main class for each test method in the Test_... class
 */
public class BuildCTSMainsSources extends BuildUtilBase {

    private String MAIN_SRC_OUTPUT_FOLDER = "";

    /**
     * @param args
     *            args 0 must be the project root folder (where src, lib etc.
     *            resides)
     * @throws IOException
     */
    public static void main(String[] args) throws IOException {
        BuildCTSMainsSources cat = new BuildCTSMainsSources();
        if (!cat.parseArgs(args)) {
          printUsage();
          System.exit(-1);
        }

        long start = System.currentTimeMillis();
        cat.run(cat::handleTest);
        long end = System.currentTimeMillis();

        System.out.println("elapsed seconds: " + (end - start) / 1000);
    }

    private boolean parseArgs(String[] args) {
      if (args.length == 1) {
          MAIN_SRC_OUTPUT_FOLDER = args[0];
          return true;
      } else {
          return false;
      }
    }

    private static void printUsage() {
        System.out.println("usage: java-src-folder output-folder classpath " +
                           "generated-main-files compiled_output");
    }

    private String getWarningMessage() {
        return "//Autogenerated code by " + this.getClass().getName() + "; do not edit.\n";
    }

    private void handleTest(String fqcn, List<String> methods) {
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

            String content = getWarningMessage() +
                    "package " + pName + ";\n" +
                    "import " + pName + ".d.*;\n" +
                    "import dot.junit.*;\n" +
                    "public class Main_" + method + " extends DxAbstractMain {\n" +
                    "    public static void main(String[] args) throws Exception {" +
                    methodContent + "\n}\n";

            File sourceFile;
            try {
                sourceFile = getFileFromPackage(pName, method);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }

            writeToFile(sourceFile, content);
        }
    }

    private File getFileFromPackage(String pname, String methodName) throws IOException {
        // e.g. dxc.junit.argsreturns.pargsreturn
        String path = getFileName(pname, methodName, ".java");
        String absPath = MAIN_SRC_OUTPUT_FOLDER + "/" + path;
        File dirPath = new File(absPath);
        File parent = dirPath.getParentFile();
        if (!parent.exists() && !parent.mkdirs()) {
            throw new IOException("failed to create directory: " + absPath);
        }
        return dirPath;
    }

    private String getFileName(String pname, String methodName, String extension) {
        String path = pname.replaceAll("\\.", "/");
        return new File(path, "Main_" + methodName + extension).getPath();
    }
}
