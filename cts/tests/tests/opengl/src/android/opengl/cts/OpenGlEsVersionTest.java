/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.opengl.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ConfigurationInfo;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.util.Log;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

/**
 * Test for checking whether the ro.opengles.version property is set to the correct value.
 */
@LargeTest
@RunWith(AndroidJUnit4.class)
public class OpenGlEsVersionTest {

    private static final String TAG = OpenGlEsVersionTest.class.getSimpleName();

    // TODO: switch to android.opengl.EGL14/EGLExt and use the constants from there
    private static final int EGL_OPENGL_ES_BIT = 0x0001;
    private static final int EGL_OPENGL_ES2_BIT = 0x0004;
    private static final int EGL_OPENGL_ES3_BIT_KHR = 0x0040;

    private OpenGlEsVersionCtsActivity mActivity;

    @Rule
    public ActivityTestRule<OpenGlEsVersionCtsActivity> mActivityRule =
            new ActivityTestRule<>(OpenGlEsVersionCtsActivity.class);

    @Rule
    public ActivityTestRule<OpenGlEsVersionCtsActivity> mActivityRelaunchRule =
            new ActivityTestRule<>(OpenGlEsVersionCtsActivity.class, false, false);

    @Before
    public void setup() {
        mActivity = mActivityRule.getActivity();
    }

    @CddTest(requirement="7.1.4.1/C-0-1")
    @Test
    public void testOpenGlEsVersion() throws InterruptedException {
        Assume.assumeFalse(isRunningANGLE());

        int detectedMajorVersion = getDetectedMajorVersion();
        int reportedVersion = getVersionFromActivityManager(mActivity);

        assertEquals("Reported OpenGL ES version from ActivityManager differs from PackageManager",
                reportedVersion, getVersionFromPackageManager(mActivity));

        verifyGlVersionString(1, 1);
        if (detectedMajorVersion == 2) {
            restartActivityWithClientVersion(2);
            verifyGlVersionString(2, getMinorVersion(reportedVersion));
        } else if (detectedMajorVersion == 3) {
            restartActivityWithClientVersion(3);
            verifyGlVersionString(3, getMinorVersion(reportedVersion));
        }
    }

    @CddTest(requirement="7.1.4.1/C-2-2")
    @Test
    public void testRequiredExtensions() throws InterruptedException {
        Assume.assumeFalse(isRunningANGLE());

        int reportedVersion = getVersionFromActivityManager(mActivity);

        if (getMajorVersion(reportedVersion) < 3)
            return;

        restartActivityWithClientVersion(3);

        String extensions = mActivity.getExtensionsString();

        if (getMajorVersion(reportedVersion) != 3 || getMinorVersion(reportedVersion) < 1)
            return;

        final String es31RequiredList[] = {
            "EXT_texture_sRGB_decode",
            "KHR_blend_equation_advanced",
            "KHR_debug",
            "OES_shader_image_atomic",
            "OES_texture_stencil8",
            "OES_texture_storage_multisample_2d_array"
        };

        for (int i = 0; i < es31RequiredList.length; ++i) {
            assertTrue("OpenGL ES version 3.1+ is missing extension " + es31RequiredList[i],
                    hasExtension(extensions, es31RequiredList[i]));
        }
    }

    @CddTest(requirement="7.1.4.1/C-2-1,C-5-1,C-4-1")
    @Test
    public void testExtensionPack() throws InterruptedException {
        Assume.assumeFalse(isRunningANGLE());

        // Requirements:
        // 1. If the device claims support for the system feature, the extension must be available.
        // 2. If the extension is available, the device must claim support for it.
        // 3. If the extension is available, it must be correct:
        //    - ES 3.1+ must be supported
        //    - All included extensions must be available

        int reportedVersion = getVersionFromActivityManager(mActivity);
        boolean hasAepFeature = mActivity.getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_OPENGLES_EXTENSION_PACK);

        if (getMajorVersion(reportedVersion) != 3 || getMinorVersion(reportedVersion) < 1) {
            assertFalse("FEATURE_OPENGLES_EXTENSION_PACK is available without OpenGL ES 3.1+",
                    hasAepFeature);
            return;
        }

        restartActivityWithClientVersion(3);

        String extensions = mActivity.getExtensionsString();
        boolean hasAepExtension = hasExtension(extensions, "GL_ANDROID_extension_pack_es31a");
        assertEquals("System feature FEATURE_OPENGLES_EXTENSION_PACK is "
            + (hasAepFeature ? "" : "not ") + "available, but extension GL_ANDROID_extension_pack_es31a is "
            + (hasAepExtension ? "" : "not ") + "in the OpenGL ES extension list.",
            hasAepFeature, hasAepExtension);
    }
    @CddTest(requirement="7.9.2/C-1-4")
    @Test
    public void testOpenGlEsVersionForVrHighPerformance() throws InterruptedException {
        Assume.assumeFalse(isRunningANGLE());

        if (!supportsVrHighPerformance())
            return;
        restartActivityWithClientVersion(3);

        int reportedVersion = getVersionFromActivityManager(mActivity);
        int major = getMajorVersion(reportedVersion);
        int minor = getMinorVersion(reportedVersion);

        assertTrue("OpenGL ES version 3.2 or higher is required for VR high-performance devices " +
            " but this device supports only version " + major + "." + minor,
            (major == 3 && minor >= 2) || major > 3);
    }

    @CddTest(requirement="7.9.2/C-1-6,C-1-8")
    @Test
    public void testRequiredExtensionsForVrHighPerformance() throws InterruptedException {
        Assume.assumeFalse(isRunningANGLE());

        if (!supportsVrHighPerformance())
            return;
        restartActivityWithClientVersion(3);
        final boolean isVrHeadset = (mActivity.getResources().getConfiguration().uiMode
            & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_VR_HEADSET;

        String extensions = mActivity.getExtensionsString();
        final String requiredList[] = {
            "GL_EXT_multisampled_render_to_texture",
            "GL_EXT_protected_textures",
            "GL_OVR_multiview",
            "GL_OVR_multiview2",
            "GL_OVR_multiview_multisampled_render_to_texture",
        };
        final String vrHeadsetRequiredList[] = {
            "GL_EXT_EGL_image_array",
            "GL_EXT_external_buffer",
            "GL_EXT_multisampled_render_to_texture2",
        };

        for (String requiredExtension : requiredList) {
            assertTrue("Required extension for VR high-performance is missing: " + requiredExtension,
                    hasExtension(extensions, requiredExtension));
        }
        if (isVrHeadset) {
            for (String requiredExtension : vrHeadsetRequiredList) {
                assertTrue("Required extension for VR high-performance is missing: " + requiredExtension,
                        hasExtension(extensions, requiredExtension));
            }
        }

        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        extensions = egl.eglQueryString(display, EGL10.EGL_EXTENSIONS);
        final String requiredEglList[] = {
            "EGL_ANDROID_front_buffer_auto_refresh",
            "EGL_ANDROID_get_native_client_buffer",
            "EGL_EXT_protected_content",
            "EGL_IMG_context_priority",
            "EGL_KHR_fence_sync",
            "EGL_KHR_mutable_render_buffer",
            "EGL_KHR_wait_sync",
        };
        final String vrHeadsetRequiredEglList[] = {
            "EGL_EXT_image_gl_colorspace",
        };

        for (String requiredExtension : requiredEglList) {
            assertTrue("Required EGL extension for VR high-performance is missing: " + requiredExtension,
                    hasExtension(extensions, requiredExtension));
        }
        if (isVrHeadset) {
            for (String requiredExtension : vrHeadsetRequiredEglList) {
                assertTrue("Required EGL extension for VR high-performance is missing: " + requiredExtension,
                        hasExtension(extensions, requiredExtension));
            }
        }
    }
    @CddTest(requirement="7.1.4.1/C-6-1")
    @Test
    public void testRequiredEglExtensions() {
        // See CDD section 7.1.4
        final String requiredEglList[] = {
            "EGL_KHR_image",
            "EGL_KHR_image_base",
            "EGL_ANDROID_image_native_buffer",
            "EGL_ANDROID_get_native_client_buffer",
            "EGL_KHR_wait_sync",
            "EGL_KHR_get_all_proc_addresses",
            "EGL_ANDROID_presentation_time",
            "EGL_KHR_swap_buffers_with_damage"
        };

        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

        if (egl.eglInitialize(display, null)) {
            try {
                String eglExtensions = egl.eglQueryString(display, EGL10.EGL_EXTENSIONS);
                for (int i = 0; i < requiredEglList.length; ++i) {
                    assertTrue("EGL Extension required by CDD section 7.1.4 missing: " +
                               requiredEglList[i], hasExtension(eglExtensions, requiredEglList[i]));
                }
                if (hasExtension(eglExtensions, "EGL_KHR_mutable_render_buffer")) {
                    assertTrue("Devices exposes EGL_KHR_mutable_render_buffer but not EGL_ANDROID_front_buffer_auto_refresh", hasExtension(eglExtensions, "EGL_ANDROID_front_buffer_auto_refresh"));
                }
            } finally {
                egl.eglTerminate(display);
            }
        } else {
            Log.e(TAG, "Couldn't initialize EGL.");
        }
    }

    @CddTest(requirement="7.1.4.5/H-1-1")
    @Test
    public void testRequiredEglExtensionsForHdrCapableDisplay() {
        // See CDD section 7.1.4
        // This test covers the EGL portion of the CDD requirement. The VK portion of the
        // requirement is covered elsewhere.
        final String requiredEglList[] = {
            "EGL_EXT_gl_colorspace_bt2020_pq",
            "EGL_EXT_surface_SMPTE2086_metadata",
            "EGL_EXT_surface_CTA861_3_metadata",
        };

        Assume.assumeFalse(isRunningANGLE());

        // This requirement only applies if device is handheld and claims to be HDR capable.
        boolean isHdrCapable = mActivity.getResources().getConfiguration().isScreenHdr();
        if (!isHdrCapable || !isHandheld())
            return;

        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

        if (egl.eglInitialize(display, null)) {
            try {
                String eglExtensions = egl.eglQueryString(display, EGL10.EGL_EXTENSIONS);
                for (int i = 0; i < requiredEglList.length; ++i) {
                    assertTrue("EGL extension required by CDD section 7.1.4.5 missing: " +
                        requiredEglList[i], hasExtension(eglExtensions, requiredEglList[i]));
                }
            } finally {
                egl.eglTerminate(display);
            }
        } else {
            Log.e(TAG, "Couldn't initialize EGL.");
        }
    }

    @CddTest(requirement="7.1.4.5/C-1-4")
    @Test
    public void testRequiredGLESVersion() {
        // This requirement only applies if device claims to be wide color capable.
        boolean isWideColorCapable =
            mActivity.getResources().getConfiguration().isScreenWideColorGamut();
        if (!isWideColorCapable)
            return;

        int reportedVersion = getVersionFromActivityManager(mActivity);
        assertEquals("Reported OpenGL ES major version doesn't meet the requirement of" +
            " CDD 7.1.4.5/C-1-4", 3, getMajorVersion(reportedVersion));
        assertTrue("Reported OpenGL ES minor version doesn't meet the requirement of" +
            " CDD 7.1.4.5/C-1-4", 1 == getMinorVersion(reportedVersion) ||
                                  2 == getMinorVersion(reportedVersion));
    }

    @CddTest(requirement="7.1.4.5/C-1-5")
    @Test
    public void testRequiredEglExtensionsForWideColorDisplay() {
        Assume.assumeFalse(isRunningANGLE());

        // See CDD section 7.1.4.5
        // This test covers the EGL portion of the CDD requirement. The VK portion of the
        // requirement is covered elsewhere.
        final String requiredEglList[] = {
            "EGL_KHR_no_config_context",
            "EGL_EXT_pixel_format_float",
            "EGL_KHR_gl_colorspace",
            "EGL_EXT_gl_colorspace_scrgb",
            "EGL_EXT_gl_colorspace_scrgb_linear",
            "EGL_EXT_gl_colorspace_display_p3",
            "EGL_EXT_gl_colorspace_display_p3_linear",
            "EGL_EXT_gl_colorspace_display_p3_passthrough",
        };

        // This requirement only applies if device claims to be wide color capable.
        boolean isWideColorCapable = mActivity.getResources().getConfiguration().isScreenWideColorGamut();
        if (!isWideColorCapable)
            return;

        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);

        if (egl.eglInitialize(display, null)) {
            try {
                String eglExtensions = egl.eglQueryString(display, EGL10.EGL_EXTENSIONS);
                for (int i = 0; i < requiredEglList.length; ++i) {
                    assertTrue("EGL extension required by CDD section 7.1.4.5 missing: " +
                        requiredEglList[i], hasExtension(eglExtensions, requiredEglList[i]));
                }
            } finally {
                egl.eglTerminate(display);
            }
        } else {
            Log.e(TAG, "Couldn't initialize EGL.");
        }
    }

    private boolean isHandheld() {
        // handheld nature is not exposed to package manager, for now
        // we check for touchscreen and NOT watch and NOT tv
        PackageManager pm = mActivity.getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_TOUCHSCREEN)
                && !pm.hasSystemFeature(pm.FEATURE_WATCH)
                && !pm.hasSystemFeature(pm.FEATURE_TELEVISION);
    }

    private static boolean hasExtension(String extensions, String name) {
        return OpenGlEsVersionCtsActivity.hasExtension(extensions, name);
    }

    /** @return OpenGL ES major version 1, 2, or 3 or some non-positive number for error */
    private static int getDetectedMajorVersion() {
        /*
         * Get all the device configurations and check the EGL_RENDERABLE_TYPE attribute
         * to determine the highest ES version supported by any config. The
         * EGL_KHR_create_context extension is required to check for ES3 support; if the
         * extension is not present this test will fail to detect ES3 support. This
         * effectively makes the extension mandatory for ES3-capable devices.
         */

        EGL10 egl = (EGL10) EGLContext.getEGL();
        EGLDisplay display = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        int[] numConfigs = new int[1];

        if (egl.eglInitialize(display, null)) {
            try {
                boolean checkES3 = hasExtension(egl.eglQueryString(display, EGL10.EGL_EXTENSIONS),
                        "EGL_KHR_create_context");
                if (egl.eglGetConfigs(display, null, 0, numConfigs)) {
                    EGLConfig[] configs = new EGLConfig[numConfigs[0]];
                    if (egl.eglGetConfigs(display, configs, numConfigs[0], numConfigs)) {
                        int highestEsVersion = 0;
                        int[] value = new int[1];
                        for (int i = 0; i < numConfigs[0]; i++) {
                            if (egl.eglGetConfigAttrib(display, configs[i],
                                    EGL10.EGL_RENDERABLE_TYPE, value)) {
                                if (checkES3 && ((value[0] & EGL_OPENGL_ES3_BIT_KHR) ==
                                        EGL_OPENGL_ES3_BIT_KHR)) {
                                    if (highestEsVersion < 3) highestEsVersion = 3;
                                } else if ((value[0] & EGL_OPENGL_ES2_BIT) == EGL_OPENGL_ES2_BIT) {
                                    if (highestEsVersion < 2) highestEsVersion = 2;
                                } else if ((value[0] & EGL_OPENGL_ES_BIT) == EGL_OPENGL_ES_BIT) {
                                    if (highestEsVersion < 1) highestEsVersion = 1;
                                }
                            } else {
                                Log.w(TAG, "Getting config attribute with "
                                        + "EGL10#eglGetConfigAttrib failed "
                                        + "(" + i + "/" + numConfigs[0] + "): "
                                        + egl.eglGetError());
                            }
                        }
                        return highestEsVersion;
                    } else {
                        Log.e(TAG, "Getting configs with EGL10#eglGetConfigs failed: "
                                + egl.eglGetError());
                        return -1;
                    }
                } else {
                    Log.e(TAG, "Getting number of configs with EGL10#eglGetConfigs failed: "
                            + egl.eglGetError());
                    return -2;
                }
              } finally {
                  egl.eglTerminate(display);
              }
        } else {
            Log.e(TAG, "Couldn't initialize EGL.");
            return -3;
        }
    }

    private static int getVersionFromActivityManager(Context context) {
        ActivityManager activityManager =
            (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ConfigurationInfo configInfo = activityManager.getDeviceConfigurationInfo();
        if (configInfo.reqGlEsVersion != ConfigurationInfo.GL_ES_VERSION_UNDEFINED) {
            return configInfo.reqGlEsVersion;
        } else {
            return 1 << 16; // Lack of property means OpenGL ES version 1
        }
    }

    private static int getVersionFromPackageManager(Context context) {
        PackageManager packageManager = context.getPackageManager();
        FeatureInfo[] featureInfos = packageManager.getSystemAvailableFeatures();
        if (featureInfos != null && featureInfos.length > 0) {
            for (FeatureInfo featureInfo : featureInfos) {
                // Null feature name means this feature is the open gl es version feature.
                if (featureInfo.name == null) {
                    if (featureInfo.reqGlEsVersion != FeatureInfo.GL_ES_VERSION_UNDEFINED) {
                        return featureInfo.reqGlEsVersion;
                    } else {
                        return 1 << 16; // Lack of property means OpenGL ES version 1
                    }
                }
            }
        }
        return 1;
    }

    /** @see FeatureInfo#getGlEsVersion() */
    private static int getMajorVersion(int glEsVersion) {
        return ((glEsVersion & 0xffff0000) >> 16);
    }

    /** @see FeatureInfo#getGlEsVersion() */
    private static int getMinorVersion(int glEsVersion) {
        return glEsVersion & 0xffff;
    }

    /**
     * Check that the version string has the form "OpenGL ES(-CM)? (\d+)\.(\d+)", where the two
     * numbers match the major and minor parameters.
     */
    @CddTest(requirement="7.1.4.1/C-0-1")
    private void verifyGlVersionString(int major, int minor) throws InterruptedException {
        Matcher matcher = Pattern.compile("OpenGL ES(?:-CM)? (\\d+)\\.(\\d+).*")
                                 .matcher(mActivity.getVersionString());
        assertTrue("OpenGL ES version string is not of the required form "
            + "'OpenGL ES(-CM)? (\\d+)\\.(\\d+).*'",
            matcher.matches());
        int stringMajor = Integer.parseInt(matcher.group(1));
        int stringMinor = Integer.parseInt(matcher.group(2));
        assertEquals("GL_VERSION string doesn't match ActivityManager major version (check ro.opengles.version property)",
            major, stringMajor);
        assertEquals("GL_VERSION string doesn't match ActivityManager minor version (check ro.opengles.version property)",
            minor, stringMinor);
    }

    /** Restart {@link GLSurfaceViewCtsActivity} with a specific client version. */
    private void restartActivityWithClientVersion(int version) {
        mActivity.finish();

        Intent intent = OpenGlEsVersionCtsActivity.createIntent(version);
        mActivity = mActivityRelaunchRule.launchActivity(intent);
    }

    /**
     * Return whether the system supports FEATURE_VR_MODE_HIGH_PERFORMANCE.
     * This is used to skip some tests.
     */
    private boolean supportsVrHighPerformance() {
        PackageManager pm = mActivity.getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_VR_MODE_HIGH_PERFORMANCE);
    }

    private boolean isRunningANGLE() {
        try {
            // We expect to find something like: OpenGL ES 1.0 (ANGLE 2.1.0.310294adacdd)
            return mActivity.getVersionString().contains("ANGLE");
        } catch (Exception e) {
            Log.e(TAG, "Caught exception: " + e);
        }
        return false;
    }
}
