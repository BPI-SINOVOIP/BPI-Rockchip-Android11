/*
 * Copyright (C) 2020 The Android Open Source Project
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
 * limitations under the License
 */

package android.compat.sjp.cts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assume.assumeTrue;

import com.android.compatibility.common.util.ApiLevelUtil;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import com.google.common.collect.HashMultimap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Multimap;
import com.google.common.collect.Sets;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

import org.jf.dexlib2.DexFileFactory;
import org.jf.dexlib2.Opcodes;
import org.jf.dexlib2.iface.DexFile;
import org.jf.dexlib2.iface.ClassDef;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class StrictJavaPackagesTest extends BaseHostJUnit4Test  {
    private static final long ADB_TIMEOUT_MILLIS = 10000L;
    /**
     * This is the list of classes that are currently duplicated and should be addressed.
     *
     * <p> DO NOT ADD CLASSES TO THIS LIST!
     */
    private static final Set<String> BCP_AND_SSCP_OVERLAP_BURNDOWN_LIST =
        ImmutableSet.of(
            "Landroid/annotation/CallbackExecutor;",
            "Landroid/annotation/CheckResult;",
            "Landroid/annotation/CurrentTimeMillisLong;",
            "Landroid/annotation/Hide;",
            "Landroid/annotation/IntDef;",
            "Landroid/annotation/IntRange;",
            "Landroid/annotation/LongDef;",
            "Landroid/annotation/NonNull;",
            "Landroid/annotation/Nullable;",
            "Landroid/annotation/RequiresPermission;",
            "Landroid/annotation/RequiresPermission$Read;",
            "Landroid/annotation/RequiresPermission$Write;",
            "Landroid/annotation/SdkConstant;",
            "Landroid/annotation/SdkConstant$SdkConstantType;",
            "Landroid/annotation/StringDef;",
            "Landroid/annotation/SystemApi;",
            "Landroid/annotation/SystemApi$Client;",
            "Landroid/annotation/SystemApi$Container;",
            "Landroid/annotation/SystemService;",
            "Landroid/annotation/TestApi;",
            "Landroid/annotation/WorkerThread;",
            "Landroid/gsi/AvbPublicKey;",
            "Landroid/gsi/AvbPublicKey$1;",
            "Landroid/gsi/GsiProgress;",
            "Landroid/gsi/GsiProgress$1;",
            "Landroid/gsi/IGsiService;",
            "Landroid/gsi/IGsiService$Default;",
            "Landroid/gsi/IGsiService$Stub;",
            "Landroid/gsi/IGsiService$Stub$Proxy;",
            "Landroid/gsi/IGsiServiceCallback;",
            "Landroid/gsi/IGsiServiceCallback$Default;",
            "Landroid/gsi/IGsiServiceCallback$Stub;",
            "Landroid/gsi/IGsiServiceCallback$Stub$Proxy;",
            "Landroid/gsi/IImageService;",
            "Landroid/gsi/IImageService$Default;",
            "Landroid/gsi/IImageService$Stub;",
            "Landroid/gsi/IImageService$Stub$Proxy;",
            "Landroid/gsi/IProgressCallback;",
            "Landroid/gsi/IProgressCallback$Default;",
            "Landroid/gsi/IProgressCallback$Stub;",
            "Landroid/gsi/IProgressCallback$Stub$Proxy;",
            "Landroid/gsi/MappedImage;",
            "Landroid/gsi/MappedImage$1;",
            "Landroid/hardware/contexthub/V1_0/AsyncEventType;",
            "Landroid/hardware/contexthub/V1_0/ContextHub;",
            "Landroid/hardware/contexthub/V1_0/ContextHubMsg;",
            "Landroid/hardware/contexthub/V1_0/HostEndPoint;",
            "Landroid/hardware/contexthub/V1_0/HubAppInfo;",
            "Landroid/hardware/contexthub/V1_0/HubMemoryFlag;",
            "Landroid/hardware/contexthub/V1_0/HubMemoryType;",
            "Landroid/hardware/contexthub/V1_0/IContexthub;",
            "Landroid/hardware/contexthub/V1_0/IContexthub$Proxy;",
            "Landroid/hardware/contexthub/V1_0/IContexthub$Stub;",
            "Landroid/hardware/contexthub/V1_0/IContexthubCallback;",
            "Landroid/hardware/contexthub/V1_0/IContexthubCallback$Proxy;",
            "Landroid/hardware/contexthub/V1_0/IContexthubCallback$Stub;",
            "Landroid/hardware/contexthub/V1_0/MemRange;",
            "Landroid/hardware/contexthub/V1_0/NanoAppBinary;",
            "Landroid/hardware/contexthub/V1_0/NanoAppFlags;",
            "Landroid/hardware/contexthub/V1_0/PhysicalSensor;",
            "Landroid/hardware/contexthub/V1_0/Result;",
            "Landroid/hardware/contexthub/V1_0/SensorType;",
            "Landroid/hardware/contexthub/V1_0/TransactionResult;"
        );

    /**
     * Ensure that there are no duplicate classes among jars listed in BOOTCLASSPATH.
     */
    @Test
    public void testBootclasspath_nonDuplicateClasses() throws Exception {
        assumeTrue(ApiLevelUtil.isAfter(getDevice(), 29));
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> bcpJarFiles = pullJarsFromEnvVariable(tmpDir, "BOOTCLASSPATH");
            checkClassDuplicatesMatchWhitelist(bcpJarFiles, ImmutableSet.of());
        });
    }

    /**
     * Ensure that there are no duplicate classes among jars listed in SYSTEMSERVERCLASSPATH.
     */
    @Test
    public void testSystemServerClasspath_nonDuplicateClasses() throws Exception {
        assumeTrue(ApiLevelUtil.isAfter(getDevice(), 29));
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> sscpJarFiles =
                pullJarsFromEnvVariable(tmpDir, "SYSTEMSERVERCLASSPATH");
            checkClassDuplicatesMatchWhitelist(sscpJarFiles, ImmutableSet.of());
        });
    }

    /**
     * Ensure that there are no duplicate classes among jars listed in BOOTCLASSPATH and
     * SYSTEMSERVERCLASSPATH.
     */
    @Test
    public void testBootClassPathAndSystemServerClasspath_nonDuplicateClasses() throws Exception {
        assumeTrue(ApiLevelUtil.isAfter(getDevice(), 29));
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> allJarFiles = Sets.union(
                pullJarsFromEnvVariable(tmpDir, "BOOTCLASSPATH"),
                pullJarsFromEnvVariable(tmpDir, "SYSTEMSERVERCLASSPATH")
            );
            checkClassDuplicatesMatchWhitelist(allJarFiles, BCP_AND_SSCP_OVERLAP_BURNDOWN_LIST);
        });
    }

    /**
     * Ensure that there are no duplicate classes among APEX jars listed in BOOTCLASSPATH.
     */
    @Test
    public void testBootclasspath_nonDuplicateApexJarClasses() throws Exception {
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> bcpJarFiles = pullJarsFromEnvVariable(tmpDir, "BOOTCLASSPATH");
            checkClassDuplicatesNotInApexJars(bcpJarFiles);
        });
    }

    /**
     * Ensure that there are no duplicate classes among APEX jars listed in SYSTEMSERVERCLASSPATH.
     */
    @Test
    public void testSystemServerClasspath_nonDuplicateApexJarClasses() throws Exception {
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> sscpJarFiles =
                pullJarsFromEnvVariable(tmpDir, "SYSTEMSERVERCLASSPATH");
            checkClassDuplicatesNotInApexJars(sscpJarFiles);
        });
    }

    /**
     * Ensure that there are no duplicate classes among APEX jars listed in BOOTCLASSPATH and
     * SYSTEMSERVERCLASSPATH.
     */
    @Test
    public void testBootClassPathAndSystemServerClasspath_nonApexDuplicateClasses()
            throws Exception {
        runWithTempDir(tmpDir -> {
            final Set<DeviceFile> allJarFiles = Sets.union(
                pullJarsFromEnvVariable(tmpDir, "BOOTCLASSPATH"),
                pullJarsFromEnvVariable(tmpDir, "SYSTEMSERVERCLASSPATH")
            );
            checkClassDuplicatesNotInApexJars(allJarFiles);
        });
    }

    private String getEnvVariable(String var) {
        try {
            return getDevice().executeShellCommand("echo $" + var).trim();
        } catch(DeviceNotAvailableException e) {
            throw new RuntimeException(e);
        }
    }

    private DeviceFile pullFromDevice(String devicePath, File tmpDir) {
        try {
            final File hostFile = Paths.get(tmpDir.getAbsolutePath(), devicePath).toFile();
            // Ensure the destination directory structure exists.
            hostFile.getParentFile().mkdirs();
            final String hostPath = hostFile.getAbsolutePath();
            getDevice().executeAdbCommand(ADB_TIMEOUT_MILLIS, "pull", devicePath, hostPath);
            return new DeviceFile(devicePath, hostPath);
        } catch (Exception e) {
            throw new RuntimeException("Failed to pull " + devicePath, e);
        }
    }

    /**
     * Gets the duplicate classes within a list of jar files.
     * @param jars  A list of jar files.
     * @return  A multimap with the class name as a key and the jar files as a value.
     */
    private Multimap<String, DeviceFile> getDuplicateClasses(Set<DeviceFile> jars)
                throws Exception {
        final Multimap<String, DeviceFile> allClasses = HashMultimap.create();
        final Multimap<String, DeviceFile> duplicateClasses = HashMultimap.create();
        for (DeviceFile deviceFile : jars) {
            final DexFile dexFile =
                    DexFileFactory.loadDexFile(deviceFile.hostPath, Opcodes.getDefault());
            for (ClassDef classDef : dexFile.getClasses()) {
                allClasses.put(classDef.getType(), deviceFile);
            }
        }
        for (Entry<String, Collection<DeviceFile>> entry : allClasses.asMap().entrySet()) {
            if (entry.getValue().size() > 1) {
                duplicateClasses.putAll(entry.getKey(), entry.getValue());
            }
        }
        return duplicateClasses;
    }

    /**
     * Checks that the duplicate classes in a set of jars exactly match a given whitelist.
     */
    private void checkClassDuplicatesMatchWhitelist(Set<DeviceFile> jars, Set<String> whitelist)
            throws Exception {
        // Collect classes which appear in at least two distinct jar files.
        Set<String> duplicateClasses = getDuplicateClasses(jars).keySet();
        assertThat(duplicateClasses).isEqualTo(whitelist);
    }

    /**
     * Checks that the duplicate classes are not in APEX jars.
     */
    private void checkClassDuplicatesNotInApexJars(Set<DeviceFile> jars)
            throws Exception {
        final Multimap<String, DeviceFile> jarClasses = getDuplicateClasses(jars);
        for (Entry<String, Collection<DeviceFile>> entry : jarClasses.asMap().entrySet()) {
            final String className = entry.getKey();
            final Collection<DeviceFile> filesWithClass = entry.getValue();
            // Check that jars that define the same class are not part of apexes.
            for (DeviceFile jarFile : filesWithClass) {
                assertWithMessage("%s is available in: %s, of which %s is an APEX jar",
                                    className, filesWithClass, jarFile.devicePath)
                .that(jarFile.devicePath.startsWith("/apex/"))
                .isFalse();
            }
        }
    }

    /**
     * Retrieve jar files from the device, based on an env variable.
     * @param tmpDir    The temporary directory where the file will be dumped.
     * @param variable  The environment variable containing the colon separated jar files.
     * @return  A {@link java.util.Set} with the pulled {@link DeviceFile} instances.
    */
    private Set<DeviceFile> pullJarsFromEnvVariable(File tmpDir, String variable) {
        return Arrays.asList(getEnvVariable(variable).split(":")).stream()
            .map(fileName -> pullFromDevice(fileName, tmpDir))
            .collect(Collectors.toSet());
    }

    private void runWithTempDir(TempDirRunnable runnable) throws Exception {
        final File tmpDir = Files.createTempDirectory("strictjavapackages").toFile();
        try {
            runnable.runWithTempDir(tmpDir);
        } finally {
            tmpDir.delete();
        }
    }

    private interface TempDirRunnable {
        public void runWithTempDir(File tempDir) throws Exception;
    }

    /**
     * Class representing a device artifact that was pulled for a test method.
     *
     * <p> Contains the local and on-device paths.
     */
    private static final class DeviceFile {
        public final String devicePath;
        public final String hostPath;
        public DeviceFile(String devicePath, String hostPath) {
            this.devicePath = devicePath;
            this.hostPath = hostPath;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) {
                return true;
            }
            if (other == null) {
                return false;
            }
            if (!(other instanceof DeviceFile)) {
                return false;
            }
            DeviceFile that = (DeviceFile) other;
            return Objects.equals(this.devicePath, that.devicePath)
            && Objects.equals(this.hostPath, that.hostPath);
        }

        @Override
        public int hashCode() {
            return Objects.hash(devicePath, hostPath);
        }

        @Override
        public String toString() {
            return String.format("DeviceFile(devicePath=%s,hostPath=%s)", devicePath, hostPath);
        }
    }
}
