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
package com.android.tradefed.invoker.shard;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.StubBuildProvider;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.keystore.IKeyStoreClient;

import java.util.Arrays;
import java.util.List;

/** Replicate a setup for one device to all other devices that will be part of sharding. */
public class ParentShardReplicate {

    /**
     * Clone the setup for one devices to as many devices as sharding will target.
     *
     * @param config The config that will carry the replicate.
     * @param client The keystore client.
     */
    public static void replicatedSetup(IConfiguration config, IKeyStoreClient client) {
        if (!config.getCommandOptions().shouldUseReplicateSetup()) {
            return;
        }
        if (config.getDeviceConfig().size() != 1) {
            return;
        }
        Integer shardCount = config.getCommandOptions().getShardCount();
        Integer shardIndex = config.getCommandOptions().getShardIndex();
        if (shardCount == null || shardIndex != null) {
            return;
        }
        CLog.logAndDisplay(LogLevel.DEBUG, "Using replicated setup.");
        try {
            List<IDeviceConfiguration> currentConfigs = config.getDeviceConfig();
            for (int i = 0; i < shardCount - 1; i++) {
                IConfiguration deepCopy =
                        config.partialDeepClone(Arrays.asList(Configuration.DEVICE_NAME), client);
                String newName = String.format("expanded-%s", i);
                IDeviceConfiguration newDeviceConfig =
                        deepCopy.getDeviceConfig().get(0).clone(newName);
                // Stub the build provider since it should never be called
                newDeviceConfig.addSpecificConfig(new StubBuildProvider());
                currentConfigs.add(newDeviceConfig);
            }
            config.setDeviceConfigList(currentConfigs);
        } catch (ConfigurationException e) {
            CLog.e("Failed replicated setup configuration:");
            CLog.e(e);
        }
    }
}
