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

package android.security.cts;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.CpuFeatures;
import com.android.compatibility.common.util.PropertyUtil;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.lang.String;
import java.util.stream.Collectors;
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.zip.GZIPInputStream;

/**
 * Host-side kernel config tests.
 *
 * These tests analyze /proc/config.gz to verify that certain kernel config options are set.
 */
public class KernelConfigTest extends DeviceTestCase implements IBuildReceiver, IDeviceTest {

    private static final Map<ITestDevice, HashSet<String>> cachedConfigGzSet = new HashMap<>(1);

    private HashSet<String> configSet;

    private ITestDevice mDevice;
    private IBuildInfo mBuild;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuild(IBuildInfo build) {
        mBuild = build;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        super.setDevice(device);
        mDevice = device;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        configSet = getDeviceConfig(mDevice, cachedConfigGzSet);
    }

    /*
     * IMPLEMENTATION DETAILS: Cache the configurations from /proc/config.gz on per-device basis
     * in case CTS is being run against multiple devices at the same time. This speeds up testing
     * by avoiding pulling/parsing the config file for each individual test
     */
    private static HashSet<String> getDeviceConfig(ITestDevice device,
            Map<ITestDevice, HashSet<String>> cache) throws Exception {
        if (!device.doesFileExist("/proc/config.gz")){
            throw new Exception();
        }
        HashSet<String> set;
        synchronized (cache) {
            set = cache.get(device);
        }
        if (set != null) {
            return set;
        }
        File file = File.createTempFile("config.gz", ".tmp");
        file.deleteOnExit();
        device.pullFile("/proc/config.gz", file);

        BufferedReader reader = new BufferedReader(new InputStreamReader(new GZIPInputStream(new FileInputStream(file))));
        set = new HashSet<String>(reader.lines().collect(Collectors.toList()));

        synchronized (cache) {
            cache.put(device, set);
        }
        return set;
    }

    /**
     * Test that the kernel has Stack Protector Strong enabled.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigStackProtectorStrong() throws Exception {
        assertTrue("Linux kernel must have Stack Protector enabled: " +
                "CONFIG_STACKPROTECTOR_STRONG=y or CONFIG_CC_STACKPROTECTOR_STRONG=y",
                configSet.contains("CONFIG_STACKPROTECTOR_STRONG=y") ||
                configSet.contains("CONFIG_CC_STACKPROTECTOR_STRONG=y"));
    }

    /**
     * Test that the kernel's executable code is read-only, read-only data is non-executable and
     * non-writable, and writable data is non-executable.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigROData() throws Exception {
        if (configSet.contains("CONFIG_UH_RKP=y"))
            return;

        assertTrue("Linux kernel must have RO data enabled: " +
                "CONFIG_DEBUG_RODATA=y or CONFIG_STRICT_KERNEL_RWX=y",
                configSet.contains("CONFIG_DEBUG_RODATA=y") ||
                configSet.contains("CONFIG_STRICT_KERNEL_RWX=y"));

        if (configSet.contains("CONFIG_MODULES=y")) {
            assertTrue("Linux kernel modules must also have RO data enabled: " +
                    "CONFIG_DEBUG_SET_MODULE_RONX=y or CONFIG_STRICT_MODULE_RWX=y",
                    configSet.contains("CONFIG_DEBUG_SET_MODULE_RONX=y") ||
                    configSet.contains("CONFIG_STRICT_MODULE_RWX=y"));
        }
    }

    /**
     * Test that the kernel implements static and dynamic object size bounds checking of copies
     * between user-space and kernel-space.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigHardenedUsercopy() throws Exception {
        if (PropertyUtil.getFirstApiLevel(mDevice) < 28) {
            return;
        }

        assertTrue("Linux kernel must have Hardened Usercopy enabled: CONFIG_HARDENED_USERCOPY=y",
                configSet.contains("CONFIG_HARDENED_USERCOPY=y"));
    }

    /**
     * Test that the kernel has PAN emulation enabled from architectures that support it.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigPAN() throws Exception {
        if (PropertyUtil.getFirstApiLevel(mDevice) < 28) {
            return;
        }

        if (CpuFeatures.isArm64(mDevice)) {
            assertTrue("Linux kernel must have PAN emulation enabled: " +
                    "CONFIG_ARM64_SW_TTBR0_PAN=y or CONFIG_ARM64_PAN=y",
                    (configSet.contains("CONFIG_ARM64_SW_TTBR0_PAN=y") ||
                    configSet.contains("CONFIG_ARM64_PAN=y")));
        } else if (CpuFeatures.isArm32(mDevice)) {
            assertTrue("Linux kernel must have PAN emulation enabled: CONFIG_CPU_SW_DOMAIN_PAN=y",
                    configSet.contains("CONFIG_CPU_SW_DOMAIN_PAN=y"));
        }
    }

    private String getHardware() throws Exception {
        String hardware = "DEFAULT";
        String[] pathList = new String[]{"/proc/cpuinfo", "/sys/devices/soc0/soc_id"};

        for (String nodeInfo : pathList) {
            if (!mDevice.doesFileExist(nodeInfo))
                continue;

            String nodeContent = mDevice.pullFileContents(nodeInfo);
            if (nodeContent == null)
                continue;

            for (String line : nodeContent.split("\n")) {
                /* Qualcomm SoCs */
                if (line.startsWith("Hardware")) {
                    String[] hardwareLine = line.split(" ");
                    hardware = hardwareLine[hardwareLine.length - 1];
                    break;
                }
                /* Samsung Exynos SoCs */
                else if (line.startsWith("EXYNOS")) {
                    hardware = line;
                    break;
                }
            }
        }
        /* TODO lookup other hardware as we get exemption requests. */
        return hardwareMitigations.containsKey(hardware) ? hardware : "DEFAULT";
    }

    private boolean doesFileExist(String filePath) throws Exception {
        String lsGrep = mDevice.executeShellCommand(String.format("ls \"%s\"", filePath));
        return lsGrep.trim().equals(filePath);
    }

    private Map<String, String[]> hardwareMitigations = new HashMap<String, String[]>() {
    {
        put("EXYNOS990", null);
        put("EXYNOS980", null);
        put("EXYNOS850", null);
        put("EXYNOS3830", null);
        put("EXYNOS9630", null);
        put("EXYNOS9830", null);
        put("EXYNOS7870", null);
        put("EXYNOS7880", null);
        put("EXYNOS7570", null);
        put("EXYNOS7872", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("EXYNOS7885", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("EXYNOS9610", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("Kirin980", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("Kirin970", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("Kirin810", null);
        put("Kirin710", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("SDMMAGPIE", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("SM6150", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("SM7150", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("LITO", null);
        put("SM8150", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("SM8150P", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("KONA", null);
        put("SDM429", null);
        put("SDM439", null);
        put("QM215", null);
        put("BENGAL", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y"});
        put("DEFAULT", new String[]{"CONFIG_HARDEN_BRANCH_PREDICTOR=y",
            "CONFIG_UNMAP_KERNEL_AT_EL0=y"});
    }};

    private String[] lookupMitigations() throws Exception {
        return hardwareMitigations.get(getHardware());
    }

    /**
     * Test that the kernel has Spectre/Meltdown mitigations for architectures and kernel versions
     * that support it. Exempt platforms which are known to not be vulnerable.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigHardwareMitigations() throws Exception {
        String mitigations[];

        if (PropertyUtil.getFirstApiLevel(mDevice) < 28) {
            return;
        }

        if (CpuFeatures.isArm64(mDevice) && !CpuFeatures.kernelVersionLessThan(mDevice, 4, 4)) {
            mitigations = lookupMitigations();
            if (mitigations != null) {
                for (String mitigation : mitigations) {
                    assertTrue("Linux kernel must have " + mitigation + " enabled.",
                            configSet.contains(mitigation));
                }
            }
        } else if (CpuFeatures.isX86(mDevice)) {
            assertTrue("Linux kernel must have KPTI enabled: CONFIG_PAGE_TABLE_ISOLATION=y",
                    configSet.contains("CONFIG_PAGE_TABLE_ISOLATION=y"));
        }
    }

    /**
     * Test that the kernel enables static usermodehelper and sets
     * the path to a whitelisted path.
     *
     * @throws Exception
     */
    @CddTest(requirement="9.7")
    public void testConfigDisableUsermodehelper() throws Exception {
        if (PropertyUtil.getFirstApiLevel(mDevice) < 30) {
            return;
        }

        final String ENABLE_CONFIG = "CONFIG_STATIC_USERMODEHELPER=y";
        final String PATH_CONFIG = "CONFIG_STATIC_USERMODEHELPER_PATH=";

        final Set<String> ALLOWED_PATH_PREFIXES = new HashSet<String>();
        ALLOWED_PATH_PREFIXES.add("/vendor/");
        ALLOWED_PATH_PREFIXES.add("/system/");
        ALLOWED_PATH_PREFIXES.add("/system_ext/");

        assertTrue("Linux kernel must enable static usermodehelper: " + ENABLE_CONFIG,
            configSet.contains(ENABLE_CONFIG));

        String configPath = null;

        for (String option : configSet) {
            if (option.startsWith(PATH_CONFIG)) {
                configPath = option;
            }
        }

        int index = configPath.indexOf('=');
        String path = configPath.substring(index+1).replace("\"", "");

        assertTrue("Linux kernel must specify an absolute path for static usermodehelper path",
            configPath.contains("..") == false);

        boolean pathIsWhitelisted = false;

        for (String allowedPath : ALLOWED_PATH_PREFIXES) {
            if (path.startsWith(allowedPath)) {
                pathIsWhitelisted = true;
                break;
            }
        }

        // Specifying no path, which disables usermodehelper, is also
        // valid.
        pathIsWhitelisted |= path.isEmpty();

        String whitelistedPathPrefixExample = "'" +
            String.join("', '", ALLOWED_PATH_PREFIXES) + "'";

        assertTrue("Linux kernel must specify a whitelisted static usermodehelper path, "
                   + "and it must be empty or start with one of the following "
                   + "prefixes: " + whitelistedPathPrefixExample, pathIsWhitelisted);
    }

    /**
     * Test that the kernel enables fs-verity and its built-in signature support.
     */
    @CddTest(requirement="9.10")
    public void testConfigFsVerity() throws Exception {
        if (PropertyUtil.getFirstApiLevel(mDevice) < 30 &&
                PropertyUtil.getPropertyInt(mDevice, "ro.apk_verity.mode") != 2) {
            return;
        }
        assertTrue("Linux kernel must have fs-verity enabled: CONFIG_FS_VERITY=y",
                configSet.contains("CONFIG_FS_VERITY=y"));
        assertTrue("Linux kernel must have fs-verity's builtin signature enabled: "
                + "CONFIG_FS_VERITY_BUILTIN_SIGNATURES=y",
                configSet.contains("CONFIG_FS_VERITY_BUILTIN_SIGNATURES=y"));
    }
}
