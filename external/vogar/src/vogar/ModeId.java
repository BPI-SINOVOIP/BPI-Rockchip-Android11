/*
 * Copyright (C) 2009 The Android Open Source Project
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

package vogar;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public enum ModeId {
    /** (Target) dalvikvm with "normal" jars */
    DEVICE,
    /** (Target) dalvikvm with -testdex jars */
    DEVICE_TESTDEX,
    /** (Host) dalvikvm with -hostdex jars */
    HOST,
    /** (Host) java */
    JVM,
    /** (Target), execution as an Android app with Zygote */
    ACTIVITY,
    /** (Target) app_process */
    APP_PROCESS;

    /**
     * $BOOTCLASSPATH for art+libcore only.
     * (Intended for use with dalvikvm only.)
     * See TARGET_TEST_CORE_JARS in android/art/build/Android.common_path.mk
     */
    private static final String[] DEVICE_JARS = new String[] {
            "core-oj",
            "core-libart",
            "core-icu4j",
            "conscrypt",
            "okhttp",
            "bouncycastle",
            "apache-xml",
    };

    /**
     * $BOOTCLASSPATH for art+libcore only (host version).
     * (Intended for use with dalvikvm only.)
     * See HOST_TEST_CORE_JARS in android/art/build/Android.common_path.mk
     */
    private static final String[] HOST_JARS = new String[] {
            "core-oj-hostdex",
            "core-libart-hostdex",
            "core-icu4j-hostdex",
            "conscrypt-hostdex",
            "okhttp-hostdex",
            "bouncycastle-hostdex",
            "apache-xml-hostdex",
    };

    /**
     * $BOOTCLASSPATH defined by init.environ.rc on device.
     *
     * {@link #DEVICE_JARS} are prepended automatically in {@link #getJarNames()} so do not need to
     * be listed below.
     *
     * (Intended for use with app_process and activities.)
     *
     * See also system/core/rootdir/init.environment.rc.in
     * and PRODUCT_BOOT_JARS in build/make/target/product/base_system.mk for the build system
     * generation.
     */
    private static final String[] APP_JARS = new String[] {
            "framework",
            "ext",
            "telephony-common",
            "voip-common",
            "ims-common",
            "android.test.base",
            // TODO: get this list programatically
    };

    public boolean acceptsVmArgs() {
        return this != ACTIVITY;
    }

    /**
     * Returns {@code true} if execution happens on the local machine. e.g. host-mode android or a
     * JVM.
     */
    public boolean isLocal() {
        return isHost() || this == ModeId.JVM;
    }

    /** Returns {@code true} if execution takes place with a host-mode Android runtime */
    public boolean isHost() {
        return this == HOST;
    }

    /** Returns {@code true} if execution takes place with a device-mode Android runtime */
    public boolean isDevice() {
        return this == ModeId.DEVICE || this == ModeId.DEVICE_TESTDEX || this == ModeId.APP_PROCESS;
    }

    public boolean requiresAndroidSdk() {
        return this != JVM;
    }

    public boolean supportsVariant(Variant variant) {
        if (variant == Variant.DEFAULT) {
            return true;
        } else if (variant == Variant.X64 || variant == Variant.X32) {
            return this == HOST || this == DEVICE || this == DEVICE_TESTDEX || this == APP_PROCESS;
        }
        // Unknown variant.
        return false;
    }

    /** Does this mode support chroot-based execution? */
    public boolean supportsChroot() {
        // We only support execution from a chroot directory in device mode for now.
        return this == ModeId.DEVICE || this == ModeId.DEVICE_TESTDEX;
    }

    public boolean supportsToolchain(Toolchain toolchain) {
        return (this == JVM && toolchain == Toolchain.JAVAC)
                || (this != JVM && toolchain != Toolchain.JAVAC);
    }

    /** The default command to use for the mode unless overridden by --vm-command */
    public String defaultVmCommand(Variant variant) {
        if (!supportsVariant(variant)) {
            throw new AssertionError("Unsupported variant: " + variant + " for " + this);
        }
        switch (this) {
            case DEVICE:
            case DEVICE_TESTDEX:
            case HOST:
                if (variant == Variant.DEFAULT) {
                    return "dalvikvm";
                } else if (variant == Variant.X32) {
                    return "dalvikvm32";
                } else if (variant == Variant.X64) {
                    return "dalvikvm64";
                }
                throw throwInvalidVariant(variant);
            case JVM:
                if (variant == Variant.DEFAULT) {
                    return "java";
                }
                throw throwInvalidVariant(variant);
            case APP_PROCESS:
                if (variant == Variant.DEFAULT) {
                    return "app_process";
                } else if (variant == Variant.X32) {
                    return "app_process32";
                } else if (variant == Variant.X64) {
                    return "app_process64";
                }
                throw throwInvalidVariant(variant);
            case ACTIVITY:
                if (variant == Variant.DEFAULT) {
                    return null;
                }
                throw throwInvalidVariant(variant);
            default:
                throw new IllegalArgumentException("Unknown mode: " + this);
        }
    }

    private IllegalArgumentException throwInvalidVariant(Variant variant) {
        throw new IllegalArgumentException(
                "Unknown variant " + variant + " for mode " + this);
    }

    /**
     * Return the names of jars required to compile in this mode when android.jar is not being used.
     * Also used to generated the bootclasspath in HOST* and DEVICE* modes.
     */
    public String[] getJarNames() {
        List<String> jarNames = new ArrayList<String>();
        switch (this) {
            case ACTIVITY:
            case APP_PROCESS:
                // Order matters. Add device-jars before app-jars.
                jarNames.addAll(Arrays.asList(DEVICE_JARS));
                jarNames.addAll(Arrays.asList(APP_JARS));
                break;
            case DEVICE:
                jarNames.addAll(Arrays.asList(DEVICE_JARS));
                break;
            case DEVICE_TESTDEX: {
                for (String deviceJarName : Arrays.asList(DEVICE_JARS)) {
                    jarNames.add(deviceJarName + "-testdex");
                }
                break;
            }
            case HOST:
                jarNames.addAll(Arrays.asList(HOST_JARS));
                break;
            default:
                throw new IllegalArgumentException("Unsupported mode: " + this);
        }
        return jarNames.toArray(new String[jarNames.size()]);
    }

    /** Returns the default toolchain to use with the mode if not overriden. */
    public Toolchain defaultToolchain() {
        switch (this) {
            case JVM:
                return Toolchain.JAVAC;
            default:
                return Toolchain.D8;
        }
    }
}
