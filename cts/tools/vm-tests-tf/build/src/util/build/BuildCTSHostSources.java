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
import java.util.Collection;
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
public class BuildCTSHostSources extends BuildUtilBase {

    public static final String TARGET_MAIN_FILE = "mains.jar";

    // the folder for the generated junit-files for the cts host (which in turn
    // execute the real vm tests using adb push/shell etc)
    private static String HOSTJUNIT_SRC_OUTPUT_FOLDER = "";

    private static final String TARGET_JAR_ROOT_PATH = "/data/local/tmp/vm-tests";

    /**
     * @param args
     *            args 0 must be the project root folder (where src, lib etc.
     *            resides)
     * @throws IOException
     */
    public static void main(String[] args) throws IOException {
        BuildCTSHostSources cat = new BuildCTSHostSources();

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
          HOSTJUNIT_SRC_OUTPUT_FOLDER = args[0];
          return true;
      } else {
          return false;
      }
    }

    private static void printUsage() {
        System.out.println("usage: java-src-folder output-folder classpath " +
                           "generated-main-files compiled_output generated-main-files " +
                           "[restrict-to-opcode]");
    }

    private static class HostState {
        private String fileName;
        private StringBuilder fileData;

        public HostState(String fileName) {
            this.fileName = fileName;
            fileData = new StringBuilder();
        }

        public void append(String s) {
            fileData.append(s);
        }

        private void addCTSHostMethod(String pName, String method,
                Collection<String> dependentTestClassNames) {
            fileData.append("public void " + method + "() throws Exception {\n");
            final String targetCoreJarPath = String.format("%s/dot/junit/dexcore.jar",
                    TARGET_JAR_ROOT_PATH);

            String mainsJar = String.format("%s/%s", TARGET_JAR_ROOT_PATH, TARGET_MAIN_FILE);

            String cp = String.format("%s:%s", targetCoreJarPath, mainsJar);
            for (String depFqcn : dependentTestClassNames) {
                String sourceName = depFqcn.replaceAll("\\.", "/") + ".jar";
                String targetName= String.format("%s/%s", TARGET_JAR_ROOT_PATH,
                        sourceName);
                cp += ":" + targetName;
                // dot.junit.opcodes.invoke_interface_range.ITest
                // -> dot/junit/opcodes/invoke_interface_range/ITest.jar
            }

            //"dot.junit.opcodes.add_double_2addr.Main_testN2";
            String mainclass = pName + ".Main_" + method;
            fileData.append(getShellExecJavaLine(cp, mainclass));
            fileData.append("\n}\n\n");
        }

        public void end() {
            fileData.append("\n}\n");
        }

        public File getFileToWrite() {
            return new File(fileName);
        }
        public String getData() {
            return fileData.toString();
        }
    }

    private void flushHostState(HostState state) {
        state.end();

        File toWrite = state.getFileToWrite();
        writeToFileMkdir(toWrite, state.getData());
    }

    private HostState openCTSHostFileFor(String pName, String classOnlyName) {
        String sourceName = classOnlyName;

        String modPackage = pName;
        {
            // Given a class name of "Test_zzz" and a package of "xxx.yyy.zzz," strip
            // "zzz" from the package to reduce duplication (and dashboard clutter).
            int lastDot = modPackage.lastIndexOf('.');
            if (lastDot > 0) {
                String lastPackageComponent = modPackage.substring(lastDot + 1);
                if (classOnlyName.equals("Test_" + lastPackageComponent)) {
                    // Drop the duplication.
                    modPackage = modPackage.substring(0, lastDot);
                }
            }
        }

        String fileName = HOSTJUNIT_SRC_OUTPUT_FOLDER + "/" + modPackage.replaceAll("\\.", "/")
                + "/" + sourceName + ".java";

        HostState newState = new HostState(fileName);

        newState.append(getWarningMessage());
        newState.append("package " + modPackage + ";\n");
        newState.append("import java.io.IOException;\n" +
                "import java.util.concurrent.TimeUnit;\n\n" +
                "import com.android.tradefed.device.CollectingOutputReceiver;\n" +
                "import com.android.tradefed.testtype.IAbi;\n" +
                "import com.android.tradefed.testtype.IAbiReceiver;\n" +
                "import com.android.tradefed.testtype.DeviceTestCase;\n" +
                "import com.android.tradefed.util.AbiFormatter;\n" +
                "\n");
        newState.append("public class " + sourceName + " extends DeviceTestCase implements " +
                "IAbiReceiver {\n");

        newState.append("\n" +
                "protected IAbi mAbi;\n" +
                "@Override\n" +
                "public void setAbi(IAbi abi) {\n" +
                "    mAbi = abi;\n" +
                "}\n\n");

        return newState;
    }

    private static String getShellExecJavaLine(String classpath, String mainclass) {
      String cmd = String.format("ANDROID_DATA=%s dalvikvm|#ABI#| -Xmx512M -Xss32K " +
              "-Djava.io.tmpdir=%s -classpath %s %s", TARGET_JAR_ROOT_PATH, TARGET_JAR_ROOT_PATH,
              classpath, mainclass);
      StringBuilder code = new StringBuilder();
      code.append("    String cmd = AbiFormatter.formatCmdForAbi(\"")
          .append(cmd)
          .append("\", mAbi.getBitness());\n")
          .append("    CollectingOutputReceiver receiver = new CollectingOutputReceiver();\n")
          .append("    getDevice().executeShellCommand(cmd, receiver, 6, TimeUnit.MINUTES, 1);\n")
          .append("    // A sucessful adb shell command returns an empty string.\n")
          .append("    assertEquals(cmd, \"\", receiver.getOutput());");
      return code.toString();
    }

    private String getWarningMessage() {
        return "//Autogenerated code by " + this.getClass().getName() + "; do not edit.\n";
    }

    private void handleTest(String fqcn, List<String> methods) {
        int lastDotPos = fqcn.lastIndexOf('.');
        String pName = fqcn.substring(0, lastDotPos);
        String classOnlyName = fqcn.substring(lastDotPos + 1);

        HostState hostState = openCTSHostFileFor(pName, classOnlyName);

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

            hostState.addCTSHostMethod(pName, method, dependentTestClassNames);
        }

        flushHostState(hostState);
    }
}
