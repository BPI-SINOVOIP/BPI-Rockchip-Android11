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

package android.jni.cts;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;

import androidx.test.InstrumentationRegistry;

import dalvik.system.PathClassLoader;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LinkerNamespacesHelper {
    private final static String PUBLIC_CONFIG_DIR = "/system/etc/";
    private final static String PRODUCT_CONFIG_DIR = "/product/etc/";
    private final static String SYSTEM_CONFIG_FILE = PUBLIC_CONFIG_DIR + "public.libraries.txt";
    private final static Pattern EXTENSION_CONFIG_FILE_PATTERN = Pattern.compile(
            "public\\.libraries-([A-Za-z0-9\\-_.]+)\\.txt");
    private final static String VENDOR_CONFIG_FILE = "/vendor/etc/public.libraries.txt";
    private final static String[] PUBLIC_SYSTEM_LIBRARIES = {
        "libaaudio.so",
        "libamidi.so",
        "libandroid.so",
        "libbinder_ndk.so",
        "libc.so",
        "libcamera2ndk.so",
        "libdl.so",
        "libEGL.so",
        "libGLESv1_CM.so",
        "libGLESv2.so",
        "libGLESv3.so",
        "libjnigraphics.so",
        "liblog.so",
        "libmediandk.so",
        "libm.so",
        "libnativewindow.so",
        "libneuralnetworks.so",
        "libOpenMAXAL.so",
        "libOpenSLES.so",
        "libRS.so",
        "libstdc++.so",
        "libsync.so",
        "libvulkan.so",
        "libz.so"
    };

    // System libraries that may exist in some types of builds.
    private final static String[] OPTIONAL_SYSTEM_LIBRARIES = {
      "libclang_rt.hwasan-aarch64-android.so"
    };

    // Libraries listed in public.libraries.android.txt, located in /apex/com.android.art/${LIB}
    private final static String[] PUBLIC_ART_LIBRARIES = {
        "libicui18n.so",
        "libicuuc.so",
    };

    // The grey-list.
    private final static String[] PRIVATE_SYSTEM_LIBRARIES = {
        "libandroid_runtime.so",
        "libbinder.so",
        "libcrypto.so",
        "libcutils.so",
        "libexpat.so",
        "libgui.so",
        "libmedia.so",
        "libnativehelper.so",
        "libskia.so",
        "libssl.so",
        "libstagefright.so",
        "libsqlite.so",
        "libui.so",
        "libutils.so",
        "libvorbisidec.so",
    };

    private final static String WEBVIEW_PLAT_SUPPORT_LIB = "libwebviewchromium_plat_support.so";

    static enum Bitness { ALL, ONLY_32, ONLY_64 }

    private static List<String> readPublicLibrariesFile(File file) throws IOException {
        List<String> libs = new ArrayList<>();
        if (file.exists()) {
            try (BufferedReader br = new BufferedReader(new FileReader(file))) {
                String line;
                final boolean is64Bit = android.os.Process.is64Bit();
                while ((line = br.readLine()) != null) {
                    line = line.trim();
                    if (line.isEmpty() || line.startsWith("#")) {
                        continue;
                    }
                    String[] tokens = line.split(" ");
                    if (tokens.length < 1 || tokens.length > 3) {
                        throw new RuntimeException("Malformed line: '" + line + "' in " + file);
                    }
                    String soname = tokens[0];
                    Bitness bitness = Bitness.ALL;
                    int i = tokens.length;
                    while(--i >= 1) {
                        if (tokens[i].equals("nopreload")) {
                            continue;
                        }
                        else if (tokens[i].equals("32") || tokens[i].equals("64")) {
                            if (bitness != Bitness.ALL) {
                                throw new RuntimeException("Malformed line: '" + line +
                                        "' in " + file + ". Bitness can be specified only once");
                            }
                            bitness = tokens[i].equals("32") ? Bitness.ONLY_32 : Bitness.ONLY_64;
                        } else {
                            throw new RuntimeException("Unrecognized token '" + tokens[i] +
                                  "' in " + file);
                        }
                    }
                    if ((is64Bit && bitness == Bitness.ONLY_32) ||
                        (!is64Bit && bitness == Bitness.ONLY_64)) {
                        // skip unsupported bitness
                        continue;
                    }
                    libs.add(soname);
                }
            }
        }
        return libs;
    }

    private static String readExtensionConfigFiles(String configDir, List<String> libs) throws IOException {
        File[] configFiles = new File(configDir).listFiles(
                new FilenameFilter() {
                    public boolean accept(File dir, String name) {
                        return EXTENSION_CONFIG_FILE_PATTERN.matcher(name).matches();
                    }
                });
        if (configFiles == null) return null;

        for (File configFile: configFiles) {
            String fileName = configFile.toPath().getFileName().toString();
            Matcher configMatcher = EXTENSION_CONFIG_FILE_PATTERN.matcher(fileName);
            if (configMatcher.matches()) {
                String companyName = configMatcher.group(1);
                // a lib in public.libraries-acme.txt should be
                // libFoo.acme.so
                List<String> libNames = readPublicLibrariesFile(configFile);
                for (String lib : libNames) {
                    if (lib.endsWith("." + companyName + ".so")) {
                        libs.add(lib);
                    } else {
                        return "Library \"" + lib + "\" in " + configFile.toString()
                                + " must have company name " + companyName + " as suffix.";
                    }
                }
            }
        }
        return null;
    }

    public static String runAccessibilityTest() throws IOException {
        List<String> systemLibs = new ArrayList<>();
        List<String> artApexLibs = new ArrayList<>();

        Collections.addAll(systemLibs, PUBLIC_SYSTEM_LIBRARIES);
        Collections.addAll(systemLibs, OPTIONAL_SYSTEM_LIBRARIES);
	// System path could contain public ART libraries on foreign arch. http://b/149852946
        if (isForeignArchitecture()) {
            Collections.addAll(systemLibs, PUBLIC_ART_LIBRARIES);
        }

        if (InstrumentationRegistry.getContext().getPackageManager().
                hasSystemFeature(PackageManager.FEATURE_WEBVIEW)) {
            systemLibs.add(WEBVIEW_PLAT_SUPPORT_LIB);
        }

        Collections.addAll(artApexLibs, PUBLIC_ART_LIBRARIES);

        // Check if public.libraries.txt contains libs other than the
        // public system libs (NDK libs).

        List<String> oemLibs = new ArrayList<>();
        String oemLibsError = readExtensionConfigFiles(PUBLIC_CONFIG_DIR, oemLibs);
        if (oemLibsError != null) return oemLibsError;
        // OEM libs that passed above tests are available to Android app via JNI
        systemLibs.addAll(oemLibs);

        // PRODUCT libs that passed are also available
        List<String> productLibs = new ArrayList<>();
        String productLibsError = readExtensionConfigFiles(PRODUCT_CONFIG_DIR, productLibs);
        if (productLibsError != null) return productLibsError;

        List<String> vendorLibs = readPublicLibrariesFile(new File(VENDOR_CONFIG_FILE));

        // Make sure that the libs in grey-list are not exposed to apps. In fact, it
        // would be better for us to run this check against all system libraries which
        // are not NDK libs, but grey-list libs are enough for now since they have been
        // the most popular violators.
        Set<String> greyListLibs = new HashSet<>();
        Collections.addAll(greyListLibs, PRIVATE_SYSTEM_LIBRARIES);
        // Note: check for systemLibs isn't needed since we already checked
        // /system/etc/public.libraries.txt against NDK and
        // /system/etc/public.libraries-<company>.txt against lib<name>.<company>.so.
        for (String lib : vendorLibs) {
            if (greyListLibs.contains(lib)) {
                return "Internal library \"" + lib + "\" must not be available to apps.";
            }
        }

        return runAccessibilityTestImpl(systemLibs.toArray(new String[systemLibs.size()]),
                                        artApexLibs.toArray(new String[artApexLibs.size()]),
                                        vendorLibs.toArray(new String[vendorLibs.size()]),
                                        productLibs.toArray(new String[productLibs.size()]));
    }

    private static native String runAccessibilityTestImpl(String[] publicSystemLibs,
                                                          String[] publicRuntimeLibs,
                                                          String[] publicVendorLibs,
                                                          String[] publicProductLibs);

    private static void invokeIncrementGlobal(Class<?> clazz) throws Exception {
        clazz.getMethod("incrementGlobal").invoke(null);
    }
    private static int invokeGetGlobal(Class<?> clazz) throws Exception  {
        return (Integer)clazz.getMethod("getGlobal").invoke(null);
    }

    private static ApplicationInfo getApplicationInfo(String packageName) {
        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        try {
            return pm.getApplicationInfo(packageName, 0);
        } catch (NameNotFoundException nnfe) {
            throw new RuntimeException(nnfe);
        }
    }

    private static String getSourcePath(String packageName) {
        String sourcePath = getApplicationInfo(packageName).sourceDir;
        if (sourcePath == null) {
            throw new IllegalStateException("No source path path found for " + packageName);
        }
        return sourcePath;
    }

    private static String getNativePath(String packageName) {
        String nativePath = getApplicationInfo(packageName).nativeLibraryDir;
        if (nativePath == null) {
            throw new IllegalStateException("No native path path found for " + packageName);
        }
        return nativePath;
    }

    private static boolean isAlreadyOpenedError(UnsatisfiedLinkError e, String libFilePath) {
        // If one of the public system libraries are already opened in the bootclassloader, consider
        // this try as success, because dlopen to the lib is successful.
        String baseName = new File(libFilePath).getName();
        return e.getMessage().contains("Shared library \"" + libFilePath +
            "\" already opened by ClassLoader") &&
            Arrays.asList(PUBLIC_SYSTEM_LIBRARIES).contains(baseName);
    }

    private static String loadWithSystemLoad(String libFilePath) {
        try {
            System.load(libFilePath);
        } catch (UnsatisfiedLinkError e) {
            // all other exceptions are just thrown
            if (!isAlreadyOpenedError(e, libFilePath)) {
                return "System.load() UnsatisfiedLinkError: " + e.getMessage();
            }
        }
        return "";
    }

    private static String loadWithSystemLoadLibrary(String libFileName) {
        // Drop 'lib' and '.so' from the base name
        String libName = libFileName.substring(3, libFileName.length()-3);
        try {
            System.loadLibrary(libName);
        } catch (UnsatisfiedLinkError e) {
            if (!isAlreadyOpenedError(e, libFileName)) {
                return "System.loadLibrary(\"" + libName + "\") UnsatisfiedLinkError: " +
                    e.getMessage();
            }
        }
        return "";
    }

    // Verify the behaviour of native library loading in class loaders.
    // In this test:
    //    - libjninamespacea1, libjninamespacea2 and libjninamespaceb depend on libjnicommon
    //    - loaderA will load ClassNamespaceA1 (loading libjninamespacea1)
    //    - loaderA will load ClassNamespaceA2 (loading libjninamespacea2)
    //    - loaderB will load ClassNamespaceB (loading libjninamespaceb)
    //    - incrementGlobal/getGlobal operate on a static global from libjnicommon
    //      and each class should get its own view on it.
    //
    // This is a test case for 2 different scenarios:
    //    - loading native libraries in different class loaders
    //    - loading native libraries in the same class loader
    // Ideally we would have 2 different tests but JNI doesn't allow loading the same library in
    // different class loaders. So to keep the number of native libraries manageable we just
    // re-use the same class loaders for the two tests.
    public static String runClassLoaderNamespaces() throws Exception {
        // Test for different class loaders.
        // Verify that common dependencies get a separate copy in each class loader.
        // libjnicommon should be loaded twice:
        // in the namespace for loaderA and the one for loaderB.
        String apkPath = getSourcePath("android.jni.cts");
        String nativePath = getNativePath("android.jni.cts");
        PathClassLoader loaderA = new PathClassLoader(
                apkPath, nativePath, ClassLoader.getSystemClassLoader());
        Class<?> testA1Class = loaderA.loadClass("android.jni.cts.ClassNamespaceA1");
        PathClassLoader loaderB = new PathClassLoader(
                apkPath, nativePath, ClassLoader.getSystemClassLoader());
        Class<?> testBClass = loaderB.loadClass("android.jni.cts.ClassNamespaceB");

        int globalA1 = invokeGetGlobal(testA1Class);
        int globalB = invokeGetGlobal(testBClass);
        if (globalA1 != 0 || globalB != 0) {
            return "Expected globals to be 0/0: globalA1=" + globalA1 + " globalB=" + globalB;
        }

        invokeIncrementGlobal(testA1Class);
        globalA1 = invokeGetGlobal(testA1Class);
        globalB = invokeGetGlobal(testBClass);
        if (globalA1 != 1 || globalB != 0) {
            return "Expected globals to be 1/0: globalA1=" + globalA1 + " globalB=" + globalB;
        }

        invokeIncrementGlobal(testBClass);
        globalA1 = invokeGetGlobal(testA1Class);
        globalB = invokeGetGlobal(testBClass);
        if (globalA1 != 1 || globalB != 1) {
            return "Expected globals to be 1/1: globalA1=" + globalA1 + " globalB=" + globalB;
        }

        // Test for the same class loaders.
        // Verify that if we load ClassNamespaceA2 into loaderA we get the same view on the
        // globals.
        Class<?> testA2Class = loaderA.loadClass("android.jni.cts.ClassNamespaceA2");

        int globalA2 = invokeGetGlobal(testA2Class);
        if (globalA1 != 1 || globalA2 !=1) {
            return "Expected globals to be 1/1: globalA1=" + globalA1 + " globalA2=" + globalA2;
        }

        invokeIncrementGlobal(testA1Class);
        globalA1 = invokeGetGlobal(testA1Class);
        globalA2 = invokeGetGlobal(testA2Class);
        if (globalA1 != 2 || globalA2 != 2) {
            return "Expected globals to be 2/2: globalA1=" + globalA1 + " globalA2=" + globalA2;
        }

        invokeIncrementGlobal(testA2Class);
        globalA1 = invokeGetGlobal(testA1Class);
        globalA2 = invokeGetGlobal(testA2Class);
        if (globalA1 != 3 || globalA2 != 3) {
            return "Expected globals to be 2/2: globalA1=" + globalA1 + " globalA2=" + globalA2;
        }
        // On success we return null.
        return null;
    }

    public static String runDlopenPublicLibraries() {
        String error;
        try {
            List<String> publicLibs = new ArrayList<>();
            Collections.addAll(publicLibs, PUBLIC_SYSTEM_LIBRARIES);
            Collections.addAll(publicLibs, PUBLIC_ART_LIBRARIES);
            error = readExtensionConfigFiles(PUBLIC_CONFIG_DIR, publicLibs);
            if (error != null) return error;
            error = readExtensionConfigFiles(PRODUCT_CONFIG_DIR, publicLibs);
            if (error != null) return error;
            publicLibs.addAll(readPublicLibrariesFile(new File(VENDOR_CONFIG_FILE)));
            for (String lib : publicLibs) {
                String result = LinkerNamespacesHelper.tryDlopen(lib);
                if (result != null) {
                    if (error == null) {
                        error = "";
                    }
                    error += result + "\n";
                }
            }
        } catch (IOException e) {
            return e.toString();
        }
        return error;
    }

    public static native String tryDlopen(String lib);

    private static boolean isForeignArchitecture() {
        int libAbi = getLibAbi();
        String cpuAbi = android.os.SystemProperties.get("ro.product.cpu.abi");
        if ((libAbi == 1 || libAbi == 2) && !cpuAbi.startsWith("arm")) {
            return true;
        } else if ((libAbi == 3 || libAbi == 4) && !cpuAbi.startsWith("x86")) {
            return true;
        }
        return false;
    }

    /**
     * @return ABI type of the JNI library. 1: ARM64, 2:ARM, 3: x86_64, 4: x86, 0: others
     */
    private static native int getLibAbi();
}

class ClassNamespaceA1 {
    static {
        System.loadLibrary("jninamespacea1");
    }

    public static native void incrementGlobal();
    public static native int getGlobal();
}

class ClassNamespaceA2 {
    static {
        System.loadLibrary("jninamespacea2");
    }

    public static native void incrementGlobal();
    public static native int getGlobal();
}

class ClassNamespaceB {
    static {
        System.loadLibrary("jninamespaceb");
    }

    public static native void incrementGlobal();
    public static native int getGlobal();
}
