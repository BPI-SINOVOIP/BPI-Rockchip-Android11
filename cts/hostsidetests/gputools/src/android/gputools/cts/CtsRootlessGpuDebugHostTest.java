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
package android.gputools.cts;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import java.util.Scanner;

import org.junit.After;
import org.junit.Before;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that exercise Rootless GPU Debug functionality supported by the loader.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsRootlessGpuDebugHostTest extends BaseHostJUnit4Test {

    public static final String TAG = "RootlessGpuDebugDeviceActivity";

    // This test ensures that the Vulkan and GLES loaders can use Settings to load layers
    // from the base directory of debuggable applications.  Is also tests several
    // positive and negative scenarios we want to cover (listed below).
    //
    // There are three APKs; DEBUG and RELEASE are practically identical with one
    // being flagged as debuggable.  The LAYERS APK is mainly a conduit for getting
    // layers onto the device without affecting the other APKs.
    //
    // The RELEASE APK does contain one layer to ensure using Settings to enable
    // layers does not interfere with legacy methods using system properties.
    //
    // The layers themselves are practically null, only enough functionality to
    // satisfy loader enumerating and loading.  They don't actually chain together.
    //
    // Positive Vulkan tests
    // - Ensure we can toggle the Enable Setting on and off (testDebugLayerLoadVulkan)
    // - Ensure we can set the debuggable app (testDebugLayerLoadVulkan)
    // - Ensure we can set the layer list (testDebugLayerLoadVulkan)
    // - Ensure we can push a layer to debuggable app (testDebugLayerLoadVulkan)
    // - Ensure we can specify the app to load layers (testDebugLayerLoadVulkan)
    // - Ensure we can load a layer from app's data directory (testDebugLayerLoadVulkan)
    // - Ensure we can load multiple layers, in order, from app's data directory (testDebugLayerLoadVulkan)
    // - Ensure we can still use system properties if no layers loaded via Settings (testSystemPropertyEnableVulkan)
    // - Ensure we can find layers in separate specified app and load them in a debuggable app (testDebugLayerLoadExternalVulkan)
    // - Ensure we can find layers in separate specified app and load them in an injectLayers app (testInjectLayerLoadExternalVulkan)
    // - Ensure we can enumerate the instance extension advertised by implicitly enabled layer (testInstanceExtensionPropertiesFromImplicitLayerVulkanBasic)
    // - Ensure we can only enumerate first instance extension closest to application
    //   when multiple implicitly enabled layers advertise the same extension (testInstanceExtensionPropertiesFromImplicitLayerVulkanMultipleLayers)
    // Negative Vulkan tests
    // - Ensure we cannot push a layer to non-debuggable app (testReleaseLayerLoadVulkan)
    // - Ensure non-debuggable app ignores the new Settings (testReleaseLayerLoadVulkan)
    // - Ensure we cannot push a layer to an injectLayers app (testInjectLayerLoadVulkan)
    // - Ensure we cannot enumerate layers from debuggable app's data directory if Setting not specified (testDebugNoEnumerateVulkan)
    // - Ensure we cannot enumerate layers without specifying the debuggable app (testDebugNoEnumerateVulkan)
    // - Ensure we cannot use system properties when layer is found via Settings with debuggable app (testSystemPropertyIgnoreVulkan)
    //
    // Positive GLES tests
    // - Ensure we can toggle the Enable Setting on and off (testDebugLayerLoadGLES)
    // - Ensure we can set the debuggable app (testDebugLayerLoadGLES)
    // - Ensure we can set the layer list (testDebugLayerLoadGLES)
    // - Ensure we can push a layer to debuggable app (testDebugLayerLoadGLES)
    // - Ensure we can specify the app to load layers (testDebugLayerLoadGLES)
    // - Ensure we can load a layer from app's data directory (testDebugLayerLoadGLES)
    // - Ensure we can load multiple layers, in order, from app's data directory (testDebugLayerLoadGLES)
    // - Ensure we can find layers in separate specified app and load them in a debuggable app (testDebugLayerLoadExternalGLES)
    // - Ensure we can find layers in separate specified app and load them in an injectLayers app (testInjectLayerLoadExternalGLES)
    // Negative GLES tests
    // - Ensure we cannot push a layer to non-debuggable app (testReleaseLayerLoadGLES)
    // - Ensure non-debuggable app ignores the new Settings (testReleaseLayerLoadGLES)
    // - Ensure we cannot enumerate layers from debuggable app's data directory if Setting not specified (testDebugNoEnumerateGLES)
    // - Ensure we cannot enumerate layers without specifying the debuggable app (testDebugNoEnumerateGLES)
    //
    // Positive combined tests
    // - Ensure we can load Vulkan and GLES layers at the same time, from multiple external apps (testMultipleExternalApps)

    private static final String CLASS = "RootlessGpuDebugDeviceActivity";
    private static final String ACTIVITY = "android.rootlessgpudebug.app.RootlessGpuDebugDeviceActivity";
    private static final String VK_LAYER_LIB_PREFIX = "libVkLayer_nullLayer";
    private static final String VK_LAYER_A_LIB = VK_LAYER_LIB_PREFIX + "A.so";
    private static final String VK_LAYER_B_LIB = VK_LAYER_LIB_PREFIX + "B.so";
    private static final String VK_LAYER_C_LIB = VK_LAYER_LIB_PREFIX + "C.so";
    private static final String VK_LAYER_D_LIB = VK_LAYER_LIB_PREFIX + "D.so";
    private static final String VK_LAYER_E_LIB = VK_LAYER_LIB_PREFIX + "E.so";
    private static final String VK_LAYER_NAME_PREFIX = "VK_LAYER_ANDROID_nullLayer";
    private static final String VK_LAYER_A = VK_LAYER_NAME_PREFIX + "A";
    private static final String VK_LAYER_B = VK_LAYER_NAME_PREFIX + "B";
    private static final String VK_LAYER_C = VK_LAYER_NAME_PREFIX + "C";
    private static final String VK_LAYER_D = VK_LAYER_NAME_PREFIX + "D";
    private static final String VK_LAYER_E = VK_LAYER_NAME_PREFIX + "E";
    private static final String DEBUG_APP = "android.rootlessgpudebug.DEBUG.app";
    private static final String RELEASE_APP = "android.rootlessgpudebug.RELEASE.app";
    private static final String INJECT_APP = "android.rootlessgpudebug.INJECT.app";
    private static final String LAYERS_APP = "android.rootlessgpudebug.LAYERS.app";
    private static final String GLES_LAYERS_APP = "android.rootlessgpudebug.GLES_LAYERS.app";
    private static final String DEBUG_APK = "CtsGpuToolsRootlessGpuDebugApp-DEBUG.apk";
    private static final String RELEASE_APK = "CtsGpuToolsRootlessGpuDebugApp-RELEASE.apk";
    private static final String INJECT_APK = "CtsGpuToolsRootlessGpuDebugApp-INJECT.apk";
    private static final String LAYERS_APK = "CtsGpuToolsRootlessGpuDebugApp-LAYERS.apk";
    private static final String GLES_LAYERS_APK = "CtsGpuToolsRootlessGpuDebugApp-GLES_LAYERS.apk";
    private static final String GLES_LAYER_A = "glesLayerA";
    private static final String GLES_LAYER_B = "glesLayerB";
    private static final String GLES_LAYER_C = "glesLayerC";
    private static final String GLES_LAYER_A_LIB = "libGLES_" + GLES_LAYER_A + ".so";
    private static final String GLES_LAYER_B_LIB = "libGLES_" + GLES_LAYER_B + ".so";
    private static final String GLES_LAYER_C_LIB = "libGLES_" + GLES_LAYER_C + ".so";

    // This is how long we'll scan the log for a result before giving up. This limit will only
    // be reached if something has gone wrong
    private static final long LOG_SEARCH_TIMEOUT_MS = 5000;
    private static final long SETTING_APPLY_TIMEOUT_MS = 5000;

    private static boolean initialized = false;

    private String removeWhitespace(String input) {
        return input.replaceAll(System.getProperty("line.separator"), "").trim();
    }

    /**
     * Return current timestamp in format accepted by logcat
     */
    private String getTime() throws Exception {
        // logcat will accept "MM-DD hh:mm:ss.mmm"
        return getDevice().executeShellCommand("date +\"%m-%d %H:%M:%S.%3N\"");
    }

    /**
     * Apply a setting and refresh the platform's cache
     */
    private void applySetting(String setting, String value) throws Exception {
        getDevice().executeShellCommand("settings put global " + setting + " " + value);
        getDevice().executeShellCommand("am refresh-settings-cache");
    }

    /**
     * Delete a setting and refresh the platform's cache
     */
    private void deleteSetting(String setting) throws Exception {
        getDevice().executeShellCommand("settings delete global " + setting);
        getDevice().executeShellCommand("am refresh-settings-cache");
    }

    /**
     * Extract the requested layer from APK and copy to tmp
     */
    private void setupLayer(String layer, String layerApp) throws Exception {

        // We use the LAYERS apk to facilitate getting layers onto the device for mixing and matching
        String libPath = getDevice().executeAdbCommand("shell", "pm", "path", layerApp);
        libPath = libPath.replaceAll("package:", "");
        libPath = libPath.replaceAll("base.apk", "");
        libPath = removeWhitespace(libPath);
        libPath += "lib/";

        // Use find to get the .so so we can ignore ABI
        String layerPath = getDevice().executeAdbCommand("shell", "find", libPath + " -name " + layer);
        layerPath = removeWhitespace(layerPath);
        getDevice().executeAdbCommand("shell", "cp", layerPath + " /data/local/tmp");
    }

    /**
     * Check that the layer is loaded by only checking the log after startTime.
     */
    private void assertVkLayerLoading(String startTime, String layerName, boolean loaded) throws Exception {
        String searchString = "nullCreateInstance called in " + layerName;
        LogScanResult result = scanLog(TAG + "," + layerName, searchString, startTime);
        if (loaded) {
            Assert.assertTrue(layerName + " was not loaded", result.found);
        } else {
            Assert.assertFalse(layerName + " was loaded", result.found);
        }
    }

    /**
     * Check that the layer is enumerated by only checking the log after startTime.
     */
    private void assertVkLayerEnumeration(String startTime, String layerName, boolean enumerated) throws Exception {
        String searchString = layerName + " loaded";
        LogScanResult result = scanLog(TAG + "," + layerName, searchString, startTime);
        if (enumerated) {
            Assert.assertTrue(layerName + " was not enumerated", result.found);
        } else {
            Assert.assertFalse(layerName + " was enumerated", result.found);
        }
    }

    /**
     * Check whether an extension is properly advertised by only checking the log after startTime.
     */
    private void assertVkExtension(String startTime, String extensionName, int specVersion) throws Exception {
        String searchString = extensionName + ": " + specVersion;
        LogScanResult result = scanLog(TAG + ",RootlessGpuDebug", searchString, startTime);
        Assert.assertTrue(extensionName + "with spec version: " + specVersion + " was not advertised", result.found);
    }

    /**
     * Simple helper class for returning multiple results
     */
    public class LogScanResult {
        public boolean found;
        public int lineNumber;
    }

    private LogScanResult scanLog(String tag, String searchString, String appStartTime) throws Exception {
        return scanLog(tag, searchString, "", appStartTime);
    }

    /**
     * Scan the logcat for requested layer tag, returning if found and which line
     */
    private LogScanResult scanLog(String tag, String searchString, String endString, String appStartTime) throws Exception {

        LogScanResult result = new LogScanResult();
        result.found = false;
        result.lineNumber = -1;

        // Scan until output from app is found
        boolean scanComplete= false;

        // Let the test run a reasonable amount of time before moving on
        long hostStartTime = System.currentTimeMillis();

        while (!scanComplete && ((System.currentTimeMillis() - hostStartTime) < LOG_SEARCH_TIMEOUT_MS)) {

            // Give our activity a chance to run and fill the log
            Thread.sleep(1000);

            // Pull the logcat since the app started, filter for tags
            // This command should look something like this:
            // adb logcat -d -t '03-27 21:35:05.392' -s "RootlessGpuDebugDeviceActivity,nullLayerC"
            String logcat = getDevice().executeShellCommand(
                    "logcat -d " +
                    "-t '" + removeWhitespace(appStartTime) + "' " +
                    "-s \"" + tag + "\"");
            int lineNumber = 0;
            Scanner apkIn = new Scanner(logcat);
            while (apkIn.hasNextLine()) {
                lineNumber++;
                String line = apkIn.nextLine();
                if (line.contains(searchString) && line.endsWith(endString)) {
                    result.found = true;
                    result.lineNumber = lineNumber;
                }
                if (line.contains("RootlessGpuDebug activity complete")) {
                    // Once we've got output from the app, we've collected what we need
                    scanComplete= true;
                }
            }
            apkIn.close();
        }

        // If this assert fires , try increasing the timeout
        Assert.assertTrue("Log scanning did not complete before timout (" +
                LOG_SEARCH_TIMEOUT_MS + "ms)", scanComplete);

        return result;
    }

    /**
     * Remove any temporary files on the device, clear any settings, kill the apps after each test
     */
    @After
    public void cleanup() throws Exception {
        getDevice().executeAdbCommand("shell", "am", "force-stop", DEBUG_APP);
        getDevice().executeAdbCommand("shell", "am", "force-stop", RELEASE_APP);
        getDevice().executeAdbCommand("shell", "am", "force-stop", INJECT_APP);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + VK_LAYER_A_LIB);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + VK_LAYER_B_LIB);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + VK_LAYER_C_LIB);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + GLES_LAYER_A_LIB);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + GLES_LAYER_B_LIB);
        getDevice().executeAdbCommand("shell", "rm", "-f", "/data/local/tmp/" + GLES_LAYER_C_LIB);
        getDevice().executeAdbCommand("shell", "settings", "delete", "global", "enable_gpu_debug_layers");
        getDevice().executeAdbCommand("shell", "settings", "delete", "global", "gpu_debug_app");
        getDevice().executeAdbCommand("shell", "settings", "delete", "global", "gpu_debug_layers");
        getDevice().executeAdbCommand("shell", "settings", "delete", "global", "gpu_debug_layers_gles");
        getDevice().executeAdbCommand("shell", "settings", "delete", "global", "gpu_debug_layer_app");
        getDevice().executeAdbCommand("shell", "setprop", "debug.vulkan.layers", "\'\'");
        getDevice().executeAdbCommand("shell", "setprop", "debug.gles.layers", "\'\'");
    }

    /**
     * Clean up before starting any tests, and ensure supporting packages are installed
     */
    @Before
    public void init() throws Exception {
        installPackage(DEBUG_APK);
        installPackage(RELEASE_APK);
        installPackage(LAYERS_APK);
        installPackage(GLES_LAYERS_APK);
        if (!initialized) {
            cleanup();
            initialized = true;
        }
    }

    /**
     * This is the primary test of the feature. It pushes layers to our debuggable app and ensures they are
     * loaded in the correct order.
     */
    @Test
    public void testDebugLayerLoadVulkan() throws Exception {

        // Set up layers to be loaded
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers", VK_LAYER_A + ":" + VK_LAYER_B);

        // Copy the layers from our LAYERS APK to tmp
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);
        setupLayer(VK_LAYER_B_LIB, LAYERS_APP);


        // Copy them over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'");
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_B_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_B_LIB, ";", "chmod", "700", VK_LAYER_B_LIB + "\'");


        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Check that both layers were loaded, in the correct order
        String searchStringA = "nullCreateInstance called in " + VK_LAYER_A;
        LogScanResult resultA = scanLog(TAG + "," + VK_LAYER_A + "," + VK_LAYER_B, searchStringA, appStartTime);
        Assert.assertTrue("LayerA was not loaded", resultA.found);

        String searchStringB = "nullCreateInstance called in " + VK_LAYER_B;
        LogScanResult resultB = scanLog(TAG + "," + VK_LAYER_A + "," + VK_LAYER_B, searchStringB, appStartTime);
        Assert.assertTrue("LayerB was not loaded", resultB.found);

        Assert.assertTrue("LayerA should be loaded before LayerB", resultA.lineNumber < resultB.lineNumber);
    }

    public void testLayerNotLoadedVulkan(final String APP_NAME) throws Exception {
        // Set up a layers to be loaded for RELEASE or INJECT app
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", APP_NAME);
        applySetting("gpu_debug_layers", VK_LAYER_A + ":" + VK_LAYER_B);

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);

        // Attempt to copy them over to our RELEASE or INJECT app (this should fail)
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
            "run-as", APP_NAME, "--user", Integer.toString(getDevice().getCurrentUser()),
            "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'", "||", "echo", "run-as", "failed");

        // Kick off our RELEASE app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", APP_NAME + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        assertVkLayerEnumeration(appStartTime, VK_LAYER_A, false);
    }

    /**
     * This test ensures that we cannot push a layer to a release app
     * It also ensures non-debuggable apps ignore Settings and don't enumerate layers in the base directory.
     */
    @Test
    public void testReleaseLayerLoadVulkan() throws Exception {
        testLayerNotLoadedVulkan(RELEASE_APP);
    }

    /**
     * This test ensures that we cannot push a layer to an injectable app
     * It also ensures non-debuggable apps ignore Settings and don't enumerate layers in the base directory.
     */
    @Test
    public void testInjectLayerLoadVulkan() throws Exception {
        testLayerNotLoadedVulkan(INJECT_APP);
    }

    /**
     * This test ensures debuggable apps do not enumerate layers in base
     * directory if enable_gpu_debug_layers is not enabled.
     */
    @Test
    public void testDebugNotEnabledVulkan() throws Exception {

        // Ensure the global layer enable settings is NOT enabled
        applySetting("enable_gpu_debug_layers", "0");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers", VK_LAYER_A);

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        assertVkLayerEnumeration(appStartTime, VK_LAYER_A, false);
    }

    /**
     * This test ensures debuggable apps do not enumerate layers in base
     * directory if gpu_debug_app does not match.
     */
    @Test
    public void testDebugWrongAppVulkan() throws Exception {

        // Ensure the gpu_debug_app does not match what we launch
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", RELEASE_APP);
        applySetting("gpu_debug_layers", VK_LAYER_A);

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        assertVkLayerEnumeration(appStartTime, VK_LAYER_A, false);
    }

    /**
     * This test ensures debuggable apps do not enumerate layers in base
     * directory if gpu_debug_layers are not set.
     */
    @Test
    public void testDebugNoLayersEnabledVulkan() throws Exception {

        // Ensure the global layer enable settings is NOT enabled
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers", "foo");

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure layerA is not loaded
        assertVkLayerLoading(appStartTime, VK_LAYER_A, false);
    }

    /**
     * This test ensures we can still use properties if no layer specified via Settings
     */
    @Test
    public void testSystemPropertyEnableVulkan() throws Exception {

        // Don't enable any layers via settings
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", RELEASE_APP);
        deleteSetting("gpu_debug_layers");

        // Enable layerC (which is packaged with the RELEASE app) with system properties
        getDevice().executeAdbCommand("shell", "setprop", "debug.vulkan.layers " + VK_LAYER_C);

        // Kick off our RELEASE app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", RELEASE_APP + "/" + ACTIVITY);

        // Check that only layerC was loaded
        assertVkLayerEnumeration(appStartTime, VK_LAYER_A, false);
        assertVkLayerLoading(appStartTime, VK_LAYER_C, true);
    }

    /**
     * This test ensures system properties are ignored if Settings load a layer
     */
    @Test
    public void testSystemPropertyIgnoreVulkan() throws Exception {

        // Set up layerA to be loaded, but not layerB
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers", VK_LAYER_A);

        // Copy the layers from our LAYERS APK
        setupLayer(VK_LAYER_A_LIB, LAYERS_APP);
        setupLayer(VK_LAYER_B_LIB, LAYERS_APP);

        // Copy them over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_A_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_A_LIB, ";", "chmod", "700", VK_LAYER_A_LIB + "\'");
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + VK_LAYER_B_LIB, "|",
                "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
                "sh", "-c", "\'cat", ">", VK_LAYER_B_LIB, ";", "chmod", "700", VK_LAYER_B_LIB + "\'");

        // Enable layerB with system properties
        getDevice().executeAdbCommand("shell", "setprop", "debug.vulkan.layers " + VK_LAYER_B);

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure only layerA is loaded
        assertVkLayerLoading(appStartTime, VK_LAYER_A, true);
        assertVkLayerLoading(appStartTime, VK_LAYER_B, false);
    }

    /**
     * The common functionality to load layers from an external package.
     * Returns the app start time.
     */
    public String testLayerLoadExternalVulkan(final String APP_NAME, String layers) throws Exception {

        // Set up layers to be loaded
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", APP_NAME);
        applySetting("gpu_debug_layers", layers);

        // Specify the external app that hosts layers
        applySetting("gpu_debug_layer_app", LAYERS_APP);

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", APP_NAME + "/" + ACTIVITY);

        String[] layerNames = layers.split(":");
        for (String layerName : layerNames) {
            assertVkLayerLoading(appStartTime, layerName, true);
        }
        return appStartTime;
    }

    /**
     * This test ensures a debuggable app can load layers from an external package
     */
    @Test
    public void testDebugLayerLoadExternalVulkan() throws Exception {
        testLayerLoadExternalVulkan(DEBUG_APP, VK_LAYER_C);
    }

    /**
     * This test ensures an injectLayers app can load layers from an external package
     */
    @Test
    public void testInjectLayerLoadExternalVulkan() throws Exception {
        testLayerLoadExternalVulkan(INJECT_APP, VK_LAYER_C);
    }

    /**
     * Test that the instance extension is advertised properly from the implicitly enabled layer.
     */
    @Test
    public void testInstanceExtensionPropertiesFromImplicitLayerVulkanBasic() throws Exception {
        String appStartTime = testLayerLoadExternalVulkan(DEBUG_APP, VK_LAYER_D);
        assertVkExtension(appStartTime, "VK_EXT_debug_utils", 1);
    }

    /**
     * Test that when there are multiple implicit layers are enabled, if there are several instance
     * extensions with the same extension names advertised by multiple layers, only the extension
     * that is closer to the application is advertised by the loader.
     */
    @Test
    public void testInstanceExtensionPropertiesFromImplicitLayerVulkanMultipleLayers() throws Exception {
        String appStartTime = testLayerLoadExternalVulkan(DEBUG_APP, VK_LAYER_E + ":" + VK_LAYER_D);
        assertVkExtension(appStartTime, "VK_EXT_debug_utils", 2);
    }

    /**
     * This test pushes GLES layers to our debuggable app and ensures they are
     * loaded in the correct order.
     */
    @Test
    public void testDebugLayerLoadGLES() throws Exception {

        // Set up layers to be loaded
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_A_LIB + ":" + GLES_LAYER_B_LIB);

        // Copy the layers from our LAYERS APK to tmp
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);
        setupLayer(GLES_LAYER_B_LIB, GLES_LAYERS_APP);

        // Copy them over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|",
            "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
            "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'");
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_B_LIB, "|",
            "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
            "sh", "-c", "\'cat", ">", GLES_LAYER_B_LIB, ";", "chmod", "700", GLES_LAYER_B_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Check that both layers were loaded, in the correct order
        String searchStringA = "glesLayer_eglChooseConfig called in " + GLES_LAYER_A;
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A + "," + GLES_LAYER_B, searchStringA, appStartTime);
        Assert.assertTrue(GLES_LAYER_A + " was not loaded", resultA.found);

        String searchStringB = "glesLayer_eglChooseConfig called in " + GLES_LAYER_B;
        LogScanResult resultB = scanLog(TAG + "," + GLES_LAYER_A + "," + GLES_LAYER_B, searchStringB, appStartTime);
        Assert.assertTrue(GLES_LAYER_B + " was not loaded", resultB.found);

        Assert.assertTrue(GLES_LAYER_A + " should be loaded before " + GLES_LAYER_B, resultA.lineNumber < resultB.lineNumber);
    }

    /**
     * This test ensures that we cannot push a layer to a non-debuggable GLES app
     * It also ensures non-debuggable apps ignore Settings and don't enumerate layers in the base directory.
     */
    @Test
    public void testReleaseLayerLoadGLES() throws Exception {

        // Set up a layers to be loaded for RELEASE app
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", RELEASE_APP);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_A_LIB + ":" + GLES_LAYER_B_LIB);
        deleteSetting("gpu_debug_layer_app");

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);

        // Attempt to copy them over to our RELEASE app (this should fail)
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|", "run-as", RELEASE_APP,
                                   "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'", "||", "echo", "run-as", "failed");

        // Kick off our RELEASE app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", RELEASE_APP + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        String searchStringA = GLES_LAYER_A + " loaded";
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertFalse(GLES_LAYER_A + " was enumerated", resultA.found);
    }

    /**
     * This test ensures debuggable GLES apps do not enumerate layers in base
     * directory if enable_gpu_debug_layers is not enabled.
     */
    @Test
    public void testDebugNotEnabledGLES() throws Exception {

        // Ensure the global layer enable settings is NOT enabled
        applySetting("enable_gpu_debug_layers", "0");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_A_LIB);

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|", "run-as", DEBUG_APP,
                                  "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        String searchStringA = GLES_LAYER_A + " loaded";
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertFalse(GLES_LAYER_A + " was enumerated", resultA.found);
    }

    /**
     * This test ensures debuggable GLES apps do not enumerate layers in base
     * directory if gpu_debug_app does not match.
     */
    @Test
    public void testDebugWrongAppGLES() throws Exception {

        // Ensure the gpu_debug_app does not match what we launch
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", RELEASE_APP);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_A_LIB);

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|", "run-as", DEBUG_APP,
                                  "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure we don't load the layer in base dir
        String searchStringA = GLES_LAYER_A + " loaded";
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertFalse(GLES_LAYER_A + " was enumerated", resultA.found);
    }

    /**
     * This test ensures debuggable GLES apps do not enumerate layers in base
     * directory if gpu_debug_layers are not set.
     */
    @Test
    public void testDebugNoLayersEnabledGLES() throws Exception {

        // Ensure the global layer enable settings is NOT enabled
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers_gles", "foo");

        // Copy a layer from our LAYERS APK to tmp
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);

        // Copy it over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|", "run-as", DEBUG_APP,
                                  "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'");

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure layerA is not loaded
        String searchStringA = "glesLayer_eglChooseConfig called in " + GLES_LAYER_A;
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertFalse(GLES_LAYER_A + " was loaded", resultA.found);
    }

    /**
     * This test ensures we can still use properties if no GLES layers are specified
     */
    @Test
    public void testSystemPropertyEnableGLES() throws Exception {

        // Set up layerA to be loaded, but not layerB or layerC
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", RELEASE_APP);
        deleteSetting("gpu_debug_layers_gles");

        // Enable layerC (which is packaged with the RELEASE app) with system properties
        getDevice().executeAdbCommand("shell", "setprop", "debug.gles.layers " + GLES_LAYER_C_LIB);

        // Kick off our RELEASE app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", RELEASE_APP + "/" + ACTIVITY);

        // Check that both layers were loaded, in the correct order
        String searchStringA = GLES_LAYER_A + "loaded";
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertFalse(GLES_LAYER_A + " was enumerated", resultA.found);

        String searchStringC = "glesLayer_eglChooseConfig called in " + GLES_LAYER_C;
        LogScanResult resultC = scanLog(TAG + "," + GLES_LAYER_C, searchStringC, appStartTime);
        Assert.assertTrue(GLES_LAYER_C + " was not loaded", resultC.found);
    }

    /**
     * This test ensures system properties are ignored if Settings load a GLES layer
     */
    @Test
    public void testSystemPropertyIgnoreGLES() throws Exception {

        // Set up layerA to be loaded, but not layerB
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_A_LIB);

        // Copy the layers from our LAYERS APK
        setupLayer(GLES_LAYER_A_LIB, GLES_LAYERS_APP);
        setupLayer(GLES_LAYER_B_LIB, GLES_LAYERS_APP);

        // Copy them over to our DEBUG app
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_A_LIB, "|",
            "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
            "sh", "-c", "\'cat", ">", GLES_LAYER_A_LIB, ";", "chmod", "700", GLES_LAYER_A_LIB + "\'");
        getDevice().executeAdbCommand("shell", "cat", "/data/local/tmp/" + GLES_LAYER_B_LIB, "|",
            "run-as", DEBUG_APP, "--user", Integer.toString(getDevice().getCurrentUser()),
            "sh", "-c", "\'cat", ">", GLES_LAYER_B_LIB, ";", "chmod", "700", GLES_LAYER_B_LIB + "\'");

        // Enable layerB with system properties
        getDevice().executeAdbCommand("shell", "setprop", "debug.gles.layers " + GLES_LAYER_B_LIB);

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Ensure only layerA is loaded
        String searchStringA = "glesLayer_eglChooseConfig called in " + GLES_LAYER_A;
        LogScanResult resultA = scanLog(TAG + "," + GLES_LAYER_A, searchStringA, appStartTime);
        Assert.assertTrue(GLES_LAYER_A + " was not loaded", resultA.found);

        String searchStringB = "glesLayer_eglChooseConfig called in " + GLES_LAYER_B;
        LogScanResult resultB = scanLog(TAG + "," + GLES_LAYER_B, searchStringB, appStartTime);
        Assert.assertFalse(GLES_LAYER_B + " was loaded", resultB.found);
    }

    public void testLayerLoadExternalGLES(final String APP_NAME) throws Exception {
        // Set up layers to be loaded
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", APP_NAME);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_C_LIB);

        // Specify the external app that hosts layers
        applySetting("gpu_debug_layer_app", GLES_LAYERS_APP);

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", APP_NAME + "/" + ACTIVITY);

        // Check that our external layer was loaded
        String searchStringC = "glesLayer_eglChooseConfig called in " + GLES_LAYER_C;
        LogScanResult resultC = scanLog(TAG + "," + GLES_LAYER_C, searchStringC, appStartTime);
        Assert.assertTrue(GLES_LAYER_C + " was not loaded", resultC.found);
    }

    /**
     * This test ensures that external GLES layers can be loaded by a debuggable app
     */
    @Test
    public void testDebugLayerLoadExternalGLES() throws Exception {
        testLayerLoadExternalGLES(DEBUG_APP);
    }

    /**
     * This test ensures that external GLES layers can be loaded by an injectLayers app
     */
    @Test
    public void testInjectLayerLoadExternalGLES() throws Exception {
        testLayerLoadExternalGLES(INJECT_APP);
    }

    /**
     *
     */
    @Test
    public void testMultipleExternalApps() throws Exception {

        // Set up layers to be loaded
        applySetting("enable_gpu_debug_layers", "1");
        applySetting("gpu_debug_app", DEBUG_APP);
        applySetting("gpu_debug_layers", VK_LAYER_C);
        applySetting("gpu_debug_layers_gles", GLES_LAYER_C_LIB);

        // Specify multple external apps that host layers
        applySetting("gpu_debug_layer_app", LAYERS_APP + ":" + GLES_LAYERS_APP);

        // Kick off our DEBUG app
        String appStartTime = getTime();
        getDevice().executeAdbCommand("shell", "am", "start", "-n", DEBUG_APP + "/" + ACTIVITY);

        // Check that external layers were loaded from both apps
        assertVkLayerLoading(appStartTime, VK_LAYER_C, true);

        String glesString = "glesLayer_eglChooseConfig called in " + GLES_LAYER_C;
        LogScanResult glesResult = scanLog(TAG + "," + GLES_LAYER_C, glesString, appStartTime);
        Assert.assertTrue(GLES_LAYER_C + " was not loaded", glesResult.found);
    }
}
