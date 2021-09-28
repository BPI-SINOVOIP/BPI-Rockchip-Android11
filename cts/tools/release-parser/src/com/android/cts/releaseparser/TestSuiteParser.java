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
import com.android.tradefed.testtype.IRemoteTest;

import junit.framework.Test;

import org.jf.dexlib2.AccessFlags;
import org.jf.dexlib2.DexFileFactory;
import org.jf.dexlib2.Opcodes;
import org.jf.dexlib2.iface.Annotation;
import org.jf.dexlib2.iface.AnnotationElement;
import org.jf.dexlib2.iface.ClassDef;
import org.jf.dexlib2.iface.DexFile;
import org.jf.dexlib2.iface.Method;
import org.jf.dexlib2.iface.value.TypeEncodedValue;
import org.junit.runner.RunWith;
import org.junit.runners.Suite.SuiteClasses;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collection;
import java.util.Enumeration;
import java.util.List;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

class TestSuiteParser {
    // JUNIT3 Test suffix
    private static final String TEST_TAG = "Test;";
    // Some may ends with Tests e.g. cts/tests/tests/accounts/src/android/accounts/cts/AbstractAuthenticatorTests.java
    private static final String TESTS_TAG = "Tests;";
    private static final String TEST_PREFIX_TAG = "test";
    private static final String DEPRECATED_ANNOTATION_TAG = "Ljava/lang/Deprecated;";
    private static final String RUN_WITH_ANNOTATION_TAG = "Lorg/junit/runner/RunWith;";
    private static final String TEST_ANNOTATION_TAG = "Lorg/junit/Test;";
    private static final String SUPPRESS_ANNOTATION_TAG = "/Suppress;";
    private static final String ANDROID_JUNIT4_TEST_TAG =
            "Landroid/support/test/runner/AndroidJUnit4;";
    private static final String PARAMETERIZED_TEST_TAG = "Lorg/junit/runners/Parameterized;";

    // configuration option
    private static final String NOT_SHARDABLE_TAG = "not-shardable";
    // test class option
    private static final String RUNTIME_HIT_TAG = "runtime-hint";
    // com.android.tradefed.testtype.AndroidJUnitTest option
    private static final String PACKAGE_TAG = "package";
    // com.android.compatibility.common.tradefed.testtype.JarHostTest option
    private static final String JAR_TAG = "jar";
    // com.android.tradefed.testtype.GTest option
    private static final String NATIVE_TEST_DEVICE_PATH_TAG = "native-test-device-path";
    private static final String MODULE_TAG = "module-name";
    private static final String TESTCASES_FOLDER_FORMAT = "testcases/%s";

    private static final String SUITE_API_INSTALLER_TAG =
            "com.android.tradefed.targetprep.suite.SuiteApkInstaller";
    private static final String HOST_TEST_CLASS_TAG =
            "com.android.compatibility.common.tradefed.testtype.JarHostTest";
    // com.android.tradefed.targetprep.suite.SuiteApkInstaller option
    private static final String TEST_FILE_NAME_TAG = "test-file-name";
    // com.android.compatibility.common.tradefed.targetprep.FilePusher option
    private static final String PUSH_TAG = "push";

    // test class
    private static final String ANDROID_JUNIT_TEST_TAG =
            "com.android.tradefed.testtype.AndroidJUnitTest";
    private static final String DEQP_TEST_TAG = "com.drawelements.deqp.runner.DeqpTestRunner";
    private static final String GTEST_TAG = "com.android.tradefed.testtype.GTest";
    private static final String LIBCORE_TEST_TAG = "com.android.compatibility.testtype.LibcoreTest";
    private static final String DALVIK_TEST_TAG = "com.android.compatibility.testtype.DalvikTest";

    // Target File Extensions
    private static final String CONFIG_EXT_TAG = ".config";
    private static final String CONFIG_REGEX = ".config$";
    private static final String JAR_EXT_TAG = ".jar";
    private static final String APK_EXT_TAG = ".apk";
    private static final String SO_EXT_TAG = ".so";

    // [module].[class]#[method]
    public static final String TESTCASE_NAME_FORMAT = "%s.%s#%s";

    private final String mFolderPath;
    private ReleaseContent mRelContent;
    private TestSuite.Builder mTSBuilder;

    TestSuiteParser(ReleaseContent relContent, String folder) {
        mFolderPath = folder;
        mRelContent = relContent;
    }

    public TestSuite getTestSuite() {
        if (mTSBuilder == null) {
            mTSBuilder = praseTestSuite();
        }
        return mTSBuilder.build();
    }

    private TestSuite.Builder praseTestSuite() {
        TestSuite.Builder tsBuilder = TestSuite.newBuilder();

        tsBuilder.setName(mRelContent.getName());
        tsBuilder.setVersion(mRelContent.getVersion());
        tsBuilder.setBuildNumber(mRelContent.getBuildNumber());

        // Iterates all file
        for (Entry entry : getFileEntriesList(mRelContent)) {
            // Only parses test module config files
            if (Entry.EntryType.TEST_MODULE_CONFIG == entry.getType()) {
                TestModuleConfig config = entry.getTestModuleConfig();
                TestSuite.Module.Builder moduleBuilder = praseModule(config);
                moduleBuilder.setConfigFile(entry.getRelativePath());
                tsBuilder.addModules(moduleBuilder);
            }
        }
        return tsBuilder;
    }

    private TestSuite.Module.Builder praseModule(TestModuleConfig config) {
        TestSuite.Module.Builder moduleBuilder = TestSuite.Module.newBuilder();
        // parse test package and class
        List<TestModuleConfig.TestClass> testClassesList = config.getTestClassesList();
        moduleBuilder.setName(config.getModuleName());
        for (TestModuleConfig.TestClass tClass : testClassesList) {
            String testClass = tClass.getTestClass();
            moduleBuilder.setTestClass(testClass);
            switch (testClass) {
                case ANDROID_JUNIT_TEST_TAG:
                    moduleBuilder.setTestType(TestSuite.TestType.ANDROIDJUNIT);
                    parseAndroidJUnitTest(moduleBuilder, config, tClass.getTestClass());
                    break;
                case HOST_TEST_CLASS_TAG:
                    moduleBuilder.setTestType(TestSuite.TestType.JAVAHOST);
                    parseJavaHostTest(moduleBuilder, config, tClass.getTestClass());
                    break;
                default:
                    //ToDo
                    moduleBuilder.setTestType(TestSuite.TestType.UNKNOWN);
                    ApiPackage.Builder pkgBuilder = ApiPackage.newBuilder();
                    moduleBuilder.addPackages(pkgBuilder);
                    System.err.printf(
                            "ToDo Test Type: %s %s\n", tClass.getTestClass(), tClass.getPackage());
            }
        }
        return moduleBuilder;
    }

    private void parseAndroidJUnitTest(
            TestSuite.Module.Builder moduleBuilder, TestModuleConfig config, String tClass) {
        // getting apk list from Test Module Configuration
        List<TestModuleConfig.TargetPreparer> tPrepList = config.getTargetPreparersList();
        for (TestModuleConfig.TargetPreparer tPrep : tPrepList) {
            for (Option opt : tPrep.getOptionsList()) {
                if (TEST_FILE_NAME_TAG.equalsIgnoreCase(opt.getName())) {
                    ApiPackage.Builder pkgBuilder = ApiPackage.newBuilder();
                    String testFileName = opt.getValue();
                    Entry tEntry = getFileEntry(testFileName);
                    pkgBuilder.setName(testFileName);
                    pkgBuilder.setPackageFile(tEntry.getRelativePath());
                    pkgBuilder.setContentId(tEntry.getContentId());
                    parseApkTestCase(pkgBuilder, config);
                    moduleBuilder.addPackages(pkgBuilder);
                }
            }
        }
    }

    private Entry getFileEntry(String name) {
        Entry fEntry = null;
        for (Entry et : getFileEntriesList(mRelContent)) {
            if (name.equals(et.getName())) {
                fEntry = et;
                break;
            }
        }
        return fEntry;
    }
    // Parses test case list from an APK
    private void parseApkTestCase(ApiPackage.Builder pkgBuilder, TestModuleConfig config) {
        DexFile dexFile = null;
        String apkPath = Paths.get(mFolderPath, pkgBuilder.getPackageFile()).toString();
        String moduleName = config.getModuleName();

        // Loads a Dex file
        try {
            dexFile = DexFileFactory.loadDexFile(apkPath, Opcodes.getDefault());

            // Iterates through all clesses in the Dex file
            for (ClassDef classDef : dexFile.getClasses()) {
                // adjust the format Lclass/y;
                String className = classDef.getType().replace('/', '.');
                // remove L...;
                if (className.length() > 2) {
                    className = className.substring(1, className.length() - 1);
                }

                // Parses test classes
                TestClassType cType = chkTestClassType(classDef);
                ApiClass.Builder tClassBuilder = ApiClass.newBuilder();
                switch (cType) {
                    case JUNIT3:
                        tClassBuilder.setTestClassType(cType);
                        tClassBuilder.setName(className);
                        // Checks all test method
                        for (Method method : classDef.getMethods()) {
                            // Only care about Public
                            if ((method.getAccessFlags() & AccessFlags.PUBLIC.getValue()) != 0) {
                                String mName = method.getName();
                                // Warn current test result accounting does not work well with Supress
                                if (hasAnnotationSuffix(
                                        method.getAnnotations(), SUPPRESS_ANNOTATION_TAG)) {
                                    System.err.printf("%s#%s with Suppress:\n", className, mName);
                                } else if (mName.startsWith(TEST_PREFIX_TAG)) {
                                    // Junit3 style test case name starts with test
                                    tClassBuilder.addMethods(
                                            newTestBuilder(
                                                    moduleName, className, method.getName()));
                                } else if (hasAnnotationSuffix(
                                        method.getAnnotations(), TEST_ANNOTATION_TAG)) {
                                    tClassBuilder.addMethods(
                                            newTestBuilder(
                                                    moduleName, className, method.getName()));
                                    System.err.printf(
                                            "%s#%s JUNIT3 mixes with %s annotation:\n",
                                            className, mName, TEST_ANNOTATION_TAG);
                                }
                            }
                        }
                        pkgBuilder.addClasses(tClassBuilder);
                        break;
                    case PARAMETERIZED:
                        // ToDo need to find a way to count Parameterized tests
                        System.err.printf("To count Parameterized tests: %s\n", className);
                        tClassBuilder.setTestClassType(cType);
                        tClassBuilder.setName(className);
                        for (Method method : classDef.getMethods()) {
                            // Junit4 style test case annotated with @Test
                            if (hasAnnotation(method.getAnnotations(), TEST_ANNOTATION_TAG)) {
                                tClassBuilder.addMethods(
                                        newTestBuilder(moduleName, className, method.getName()));
                            }
                        }
                        pkgBuilder.addClasses(tClassBuilder);
                        break;
                    case JUNIT4:
                        tClassBuilder.setTestClassType(cType);
                        tClassBuilder.setName(className);
                        for (Method method : classDef.getMethods()) {
                            // Junit4 style test case annotated with @Test
                            if (hasAnnotation(method.getAnnotations(), TEST_ANNOTATION_TAG)) {
                                tClassBuilder.addMethods(
                                        newTestBuilder(moduleName, className, method.getName()));
                            }
                        }
                        pkgBuilder.addClasses(tClassBuilder);
                        break;
                    default:
                        // Not a known test class
                }
            }
        } catch (IOException | DexFileFactory.DexFileNotFoundException ex) {
            System.err.println("Unable to load dex file: " + apkPath);
            // ex.printStackTrace();
        }
    }

    private ApiMethod.Builder newTestBuilder(String moduleName, String className, String testName) {
        ApiMethod.Builder testBuilder = ApiMethod.newBuilder();
        testBuilder.setName(testName);
        // Check if it's an known failure
        String nfFilter = getKnownFailureFilter(moduleName, className, testName);
        if (null != nfFilter) {
            testBuilder.setKnownFailureFilter(nfFilter);
        }
        return testBuilder;
    }

    private void parseJavaHostTest(
            TestSuite.Module.Builder moduleBuilder, TestModuleConfig config, String tClass) {
        ApiPackage.Builder pkgBuilder = ApiPackage.newBuilder();
        //Assuming there is only one test Jar
        String testFileName = config.getTestJars(0);
        Entry tEntry = getFileEntry(testFileName);
        String jarPath = tEntry.getRelativePath();

        pkgBuilder.setName(testFileName);
        pkgBuilder.setPackageFile(jarPath);
        pkgBuilder.setContentId(tEntry.getContentId());
        Collection<Class<?>> classes =
                getJarTestClasses(
                        Paths.get(mFolderPath, jarPath).toFile(),
                        // Includes [x]-tradefed.jar for classes such as CompatibilityHostTestBase
                        Paths.get(mFolderPath, mRelContent.getTestSuiteTradefed()).toFile());

        for (Class<?> c : classes) {
            ApiClass.Builder tClassBuilder = ApiClass.newBuilder();
            tClassBuilder.setTestClassType(TestClassType.JAVAHOST);
            tClassBuilder.setName(c.getName());

            for (java.lang.reflect.Method m : c.getMethods()) {
                int mdf = m.getModifiers();
                if (Modifier.isPublic(mdf) || Modifier.isProtected(mdf)) {
                    if (m.getName().startsWith(TEST_PREFIX_TAG)) {
                        ApiMethod.Builder methodBuilder = ApiMethod.newBuilder();
                        methodBuilder.setName(m.getName());
                        // Check if it's an known failure
                        String nfFilter =
                                getKnownFailureFilter(
                                        config.getModuleName(), c.getName(), m.getName());
                        if (null != nfFilter) {
                            methodBuilder.setKnownFailureFilter(nfFilter);
                        }
                        tClassBuilder.addMethods(methodBuilder);
                    }
                }
            }
            pkgBuilder.addClasses(tClassBuilder);
        }
        moduleBuilder.addPackages(pkgBuilder);
    }

    private static boolean hasAnnotation(Set<? extends Annotation> annotations, String tag) {
        for (Annotation annotation : annotations) {
            if (annotation.getType().equals(tag)) {
                return true;
            }
        }
        return false;
    }

    private static boolean hasAnnotationSuffix(Set<? extends Annotation> annotations, String tag) {
        for (Annotation annotation : annotations) {
            if (annotation.getType().endsWith(tag)) {
                return true;
            }
        }
        return false;
    }

    private static TestClassType chkTestClassType(ClassDef classDef) {
        // Only care about Public Class
        if ((classDef.getAccessFlags() & AccessFlags.PUBLIC.getValue()) == 0) {
            return TestClassType.UNKNOWN;
        }

        for (Annotation annotation : classDef.getAnnotations()) {
            if (annotation.getType().equals(DEPRECATED_ANNOTATION_TAG)) {
                return TestClassType.UNKNOWN;
            }
            if (annotation.getType().equals(RUN_WITH_ANNOTATION_TAG)) {
                for (AnnotationElement annotationEle : annotation.getElements()) {
                    if ("value".equals(annotationEle.getName())) {
                        String aValue = ((TypeEncodedValue) annotationEle.getValue()).getValue();
                        if (ANDROID_JUNIT4_TEST_TAG.equals(aValue)) {
                            return TestClassType.JUNIT4;
                        } else if (PARAMETERIZED_TEST_TAG.equals(aValue)) {
                            return TestClassType.PARAMETERIZED;
                        }
                    }
                }
                System.err.printf("Unknown test class type: %s\n", classDef.getType());
                return TestClassType.JUNIT4;
            }
        }

        if (classDef.getType().endsWith(TEST_TAG) || classDef.getType().endsWith(TESTS_TAG)) {
            return TestClassType.JUNIT3;
        } else {
            return TestClassType.UNKNOWN;
        }
    }

    private static boolean isTargetClass(List<String> pkgList, String className) {
        boolean found = false;
        for (String pkg : pkgList) {
            if (className.startsWith(pkg)) {
                found = true;
                break;
            }
        }
        return found;
    }

    private static Collection<Class<?>> getJarTestClasses(File jarTestFile, File tfFile)
            throws IllegalArgumentException {
        List<Class<?>> classes = new ArrayList<>();

        try (JarFile jarFile = new JarFile(jarTestFile)) {
            Enumeration<JarEntry> e = jarFile.entries();

            URL[] urls = {
                new URL(String.format("jar:file:%s!/", jarTestFile.getAbsolutePath())),
                new URL(String.format("jar:file:%s!/", tfFile.getAbsolutePath()))
            };
            URLClassLoader cl =
                    URLClassLoader.newInstance(urls, JarTestFinder.class.getClassLoader());

            while (e.hasMoreElements()) {
                JarEntry je = e.nextElement();
                if (je.isDirectory()
                        || !je.getName().endsWith(".class")
                        || je.getName().contains("$")
                        || je.getName().contains("junit/")) {
                    continue;
                }
                String className = getClassName(je.getName());

                /*if (!className.endsWith("Test")) {
                    continue;
                }*/
                try {
                    Class<?> cls = cl.loadClass(className);

                    if (IRemoteTest.class.isAssignableFrom(cls)
                            || Test.class.isAssignableFrom(cls)) {
                        classes.add(cls);
                    } else if (!Modifier.isAbstract(cls.getModifiers())
                            && hasJUnit4Annotation(cls)) {
                        classes.add(cls);
                    }
                } catch (ClassNotFoundException | Error x) {
                    System.err.println(
                            String.format(
                                    "Cannot find test class %s from %s",
                                    className, jarTestFile.getName()));
                    x.printStackTrace();
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        return classes;
    }

    /** Helper to determine if we are dealing with a Test class with Junit4 annotations. */
    protected static boolean hasJUnit4Annotation(Class<?> classObj) {
        if (classObj.isAnnotationPresent(SuiteClasses.class)) {
            return true;
        }
        if (classObj.isAnnotationPresent(RunWith.class)) {
            return true;
        }
        /*for (Method m : classObj.getMethods()) {
            if (m.isAnnotationPresent(org.junit.Test.class)) {
                return true;
            }
        }*/
        return false;
    }

    private static String getClassName(String name) {
        // -6 because of .class
        return name.substring(0, name.length() - 6).replace('/', '.');
    }

    private String getKnownFailureFilter(String tModule, String tClass, String tMethod) {
        List<String> knownFailures = mRelContent.getKnownFailuresList();
        String tsName = String.format(TESTCASE_NAME_FORMAT, tModule, tClass, tMethod);
        for (String kf : knownFailures) {
            if (tsName.startsWith(kf)) {
                return kf;
            }
        }
        return null;
    }

    // Iterates though all test suite content and prints them.
    public void writeCsvFile(String relNameVer, String csvFile) {
        TestSuite ts = getTestSuite();
        try {
            FileWriter fWriter = new FileWriter(csvFile);
            PrintWriter pWriter = new PrintWriter(fWriter);
            //Header
            pWriter.println(
                    "release,module,test_class,test,test_package,test_type,known_failure_filter,package_content_id");
            for (TestSuite.Module module : ts.getModulesList()) {
                for (ApiPackage pkg : module.getPackagesList()) {
                    for (ApiClass cls : pkg.getClassesList()) {
                        for (ApiMethod mtd : cls.getMethodsList()) {
                            pWriter.printf(
                                    "%s,%s,%s,%s,%s,%s,%s,%s\n",
                                    relNameVer,
                                    module.getName(),
                                    cls.getName(),
                                    mtd.getName(),
                                    pkg.getPackageFile(),
                                    cls.getTestClassType(),
                                    mtd.getKnownFailureFilter(),
                                    getTestTargetContentId(pkg.getPackageFile()));
                        }
                    }
                }
            }
            pWriter.flush();
            pWriter.close();
        } catch (IOException e) {
            System.err.println("IOException:" + e.getMessage());
        }
    }

    // Iterates though all test module and prints them.
    public void writeModuleCsvFile(String relNameVer, String csvFile) {
        TestSuite ts = getTestSuite();
        try {
            FileWriter fWriter = new FileWriter(csvFile);
            PrintWriter pWriter = new PrintWriter(fWriter);

            //Header
            pWriter.print(
                    "release,module,test_no,known_failure_no,test_type,test_class,component,description,test_config_file,test_file_names,test_jars,module_content_id\n");

            for (TestSuite.Module module : ts.getModulesList()) {
                int classCnt = 0;
                int methodCnt = 0;
                int kfCnt = 0;
                for (ApiPackage pkg : module.getPackagesList()) {
                    for (ApiClass cls : pkg.getClassesList()) {
                        for (ApiMethod mtd : cls.getMethodsList()) {
                            // Filter out known failures
                            if (mtd.getKnownFailureFilter().isEmpty()) {
                                methodCnt++;
                            } else {
                                kfCnt++;
                            }
                        }
                        classCnt++;
                    }
                }
                String config = module.getConfigFile();
                Entry entry = mRelContent.getEntries().get(config);
                TestModuleConfig tmConfig = entry.getTestModuleConfig();
                pWriter.printf(
                        "%s,%s,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s\n",
                        relNameVer,
                        module.getName(),
                        methodCnt,
                        kfCnt,
                        module.getTestType(),
                        module.getTestClass(),
                        tmConfig.getComponent(),
                        tmConfig.getDescription(),
                        config,
                        String.join(" ", tmConfig.getTestFileNamesList()),
                        String.join(" ", tmConfig.getTestJarsList()),
                        getTestModuleContentId(
                                entry,
                                tmConfig.getTestFileNamesList(),
                                tmConfig.getTestJarsList()));
            }
            pWriter.flush();
            pWriter.close();
        } catch (IOException e) {
            System.err.println("IOException:" + e.getMessage());
        }
    }

    public static Collection<Entry> getFileEntriesList(ReleaseContent relContent) {
        return relContent.getEntries().values();
    }

    // get Test Module Content Id = config cid + apk cids + jar cids
    private String getTestModuleContentId(Entry config, List<String> apks, List<String> jars) {
        String id = null;
        //Starts with config file content_id
        String idStr = config.getContentId();
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            //Add all apk content_id
            for (String apk : apks) {
                idStr += getTestTargetContentId(String.format(TESTCASES_FOLDER_FORMAT, apk));
            }
            //Add all jar content_id
            for (String jar : jars) {
                idStr += getTestTargetContentId(String.format(TESTCASES_FOLDER_FORMAT, jar));
            }
            md.update(idStr.getBytes(StandardCharsets.UTF_8));
            // Converts to Base64 String
            id = Base64.getEncoder().encodeToString(md.digest());
            // System.out.println("getTestModuleContentId: " + idStr);
        } catch (NoSuchAlgorithmException e) {
            System.err.println("NoSuchAlgorithmException:" + e.getMessage());
        }
        return id;
    }

    private String getTestTargetContentId(String targetFile) {
        Entry entry = mRelContent.getEntries().get(targetFile);
        if (entry != null) {
            return entry.getContentId();
        } else {
            System.err.println("No getTestTargetContentId: " + targetFile);
            return "";
        }
    }

}
