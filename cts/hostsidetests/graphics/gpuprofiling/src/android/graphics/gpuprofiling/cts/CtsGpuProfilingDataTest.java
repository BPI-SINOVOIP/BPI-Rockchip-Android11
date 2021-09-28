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
 * limitations under the License.
 */

package android.graphics.gpuprofiling.cts;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import perfetto.protos.PerfettoConfig.TracingServiceState;
import perfetto.protos.PerfettoConfig.TracingServiceState.DataSource;
import perfetto.protos.PerfettoConfig.DataSourceDescriptor;

import java.util.Base64;

import org.junit.After;
import org.junit.Before;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests that ensure Perfetto producers exist for GPU profiling when the device claims to support profilng.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsGpuProfilingDataTest extends BaseHostJUnit4Test {
    public static final String TAG = "GpuProfilingDataDeviceActivity";

    // This test ensures that if a device reports ro.hardware.gpu.profiler.support if reports the correct perfetto producers
    //
    // Positive tests
    // - Ensure the perfetto producers for render stages, counters, and ftrace gpu frequency are available

    private static final String BIN_NAME = "ctsgraphicsgpucountersinit";
    private static final String DEVICE_BIN_PATH = "/data/local/tmp/" + BIN_NAME;
    private static final String APP = "android.graphics.gpuprofiling.app";
    private static final String APK = "CtsGraphicsProfilingDataApp.apk";
    private static final String ACTIVITY = "GpuRenderStagesDeviceActivity";
    private static final String COUNTERS_SOURCE_NAME = "gpu.counters";
    private static final String STAGES_SOURCE_NAME = "gpu.renderstages";
    private static final String PROFILING_PROPERTY = "graphics.gpu.profiler.support";
    private static final String LAYER_PACKAGE_PROPERTY = "graphics.gpu.profiler.vulkan_layer_apk";
    private static final String LAYER_NAME = "VkRenderStagesProducer";
    private static int MAX_RETRIES = 5;

    private class ShellThread extends Thread {

        private String mCmd;

        public ShellThread(String cmd) throws Exception {
            super("ShellThread");
            mCmd = cmd;
        }

        @Override
        public void run() {
            try {
                getDevice().executeShellV2Command(mCmd);
            } catch (Exception e) {
                CLog.e("Failed to start counters producer" + e.getMessage());
            }
        }
    }

    /**
     * Kill the native process and remove the layer related settings after each test
     */
    @After
    public void cleanup() throws Exception {
        getDevice().executeShellV2Command("killall " + BIN_NAME);
        getDevice().executeShellV2Command("am force-stop " + APP);
        getDevice().executeShellV2Command("settings delete global gpu_debug_layers");
        getDevice().executeShellV2Command("settings delete global enable_gpu_debug_layers");
        getDevice().executeShellV2Command("settings delete global gpu_debug_app");
        getDevice().executeShellV2Command("settings delete global gpu_debug_layer_app");
    }

    /**
     * Clean up before starting any tests. Apply the necessary layer settings if we need them
     */
    @Before
    public void init() throws Exception {
        cleanup();
        String layerApp = getDevice().getProperty(LAYER_PACKAGE_PROPERTY);
        if (layerApp != null && !layerApp.isEmpty()) {
            getDevice().executeShellV2Command("settings put global enable_gpu_debug_layers 1");
            getDevice().executeShellV2Command("settings put global gpu_debug_app " + APP);
            getDevice().executeShellV2Command("settings put global gpu_debug_layer_app " + layerApp);
            getDevice().executeShellV2Command("settings put global gpu_debug_layers " + LAYER_NAME);
        }
        installPackage(APK);
    }

    /**
     * This is the primary test of the feature. We check that gpu.counters and gpu.renderstages sources are available.
     */
    @Test
    public void testProfilingDataProducersAvailable() throws Exception {
        String profilingSupport = getDevice().getProperty(PROFILING_PROPERTY);

        if (profilingSupport != null && profilingSupport.equals("true")) {
            // Spin up a new thread to avoid blocking the main thread while the native process waits to be killed.
            ShellThread shellThread = new ShellThread(DEVICE_BIN_PATH);
            shellThread.start();
            CommandResult activityStatus = getDevice().executeShellV2Command("am start -n " + APP + "/." + ACTIVITY);
            boolean countersSourceFound = false;
            boolean stagesSourceFound = false;
            for(int i = 0; i < MAX_RETRIES; i++) {
                CommandResult queryStatus = getDevice().executeShellV2Command("perfetto --query-raw | base64");
                Assert.assertEquals(CommandStatus.SUCCESS, queryStatus.getStatus());
                byte[] decodedBytes = Base64.getMimeDecoder().decode(queryStatus.getStdout());
                TracingServiceState state = TracingServiceState.parseFrom(decodedBytes);
                int count = state.getDataSourcesCount();
                Assert.assertTrue("No sources found", count > 0);
                for (int j = 0; j < count; j++) {
                    DataSource source = state.getDataSources(j);
                    DataSourceDescriptor descriptor = source.getDsDescriptor();
                    if (descriptor != null) {
                        if (descriptor.getName().equals(COUNTERS_SOURCE_NAME)) {
                            countersSourceFound = true;
                        }
                        if (descriptor.getName().equals(STAGES_SOURCE_NAME)) {
                            stagesSourceFound = true;
                        }
                        if (countersSourceFound && stagesSourceFound) {
                            break;
                        }
                    }
                }
                if (countersSourceFound && stagesSourceFound) {
                    break;
                }
                Thread.sleep(1000);
            }

            Assert.assertTrue("Producer " + STAGES_SOURCE_NAME + " not found", stagesSourceFound);
            Assert.assertTrue("Producer " + COUNTERS_SOURCE_NAME + " not found", countersSourceFound);
        }
    }
}
