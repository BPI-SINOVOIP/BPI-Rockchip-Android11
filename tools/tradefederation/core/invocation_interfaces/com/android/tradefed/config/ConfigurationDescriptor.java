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
package com.android.tradefed.config;

import com.android.tradefed.build.BuildSerializedVersion;
import com.android.tradefed.config.proto.ConfigurationDescription;
import com.android.tradefed.config.proto.ConfigurationDescription.Descriptor;
import com.android.tradefed.config.proto.ConfigurationDescription.Metadata;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.util.MultiMap;

import com.google.common.annotations.VisibleForTesting;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * Configuration Object that describes some aspect of the configuration itself. Like a membership
 * test-suite-tag. This class cannot receive option values via command line. Only directly in the
 * xml.
 */
@OptionClass(alias = "config-descriptor")
public class ConfigurationDescriptor implements Serializable, Cloneable {

    /** Enum used to indicate local test runner. */
    public enum LocalTestRunner {
        NONE,
        ATEST;
    }

    private static final long serialVersionUID = BuildSerializedVersion.VERSION;

    /** Metadata key for a config to specify that it was sharded. */
    public static final String LOCAL_SHARDED_KEY = "sharded";
    /** Metadata key for a config parameterization, optional. */
    public static final String ACTIVE_PARAMETER_KEY = "active-parameter";

    @Option(name = "test-suite-tag", description = "A membership tag to suite. Can be repeated.")
    private List<String> mSuiteTags = new ArrayList<>();

    @Option(name = "metadata", description = "Metadata associated with this configuration, can be "
            + "free formed key value pairs, and a key may be associated with multiple values.")
    private MultiMap<String, String> mMetaData = new MultiMap<>();

    @Option(
        name = "not-shardable",
        description =
                "A metadata that allows a suite configuration to specify that it cannot be "
                        + "sharded. Not because it doesn't support it but because it doesn't make "
                        + "sense."
    )
    private boolean mNotShardable = false;

    @Option(
        name = "not-strict-shardable",
        description =
                "A metadata to allows a suite configuration to specify that it cannot be "
                        + "sharded in a strict context (independent shards). If a config is already "
                        + "not-shardable, it will be not-strict-shardable."
    )
    private boolean mNotStrictShardable = false;

    @Option(
        name = "use-sandboxing",
        description = "Option used to notify an invocation that it is running in a sandbox."
    )
    private boolean mUseSandboxing = false;

    /** Optional Abi information the configuration will be run against. */
    private IAbi mAbi = null;
    /** Optional for a module configuration, the original name of the module. */
    private String mModuleName = null;

    /** a list of options applicable to rerun the test */
    private final List<OptionDef> mRerunOptions = new ArrayList<>();

    /** Returns the list of suite tags the test is part of. */
    public List<String> getSuiteTags() {
        return mSuiteTags;
    }

    /** Sets the list of suite tags the test is part of. */
    public void setSuiteTags(List<String> suiteTags) {
        mSuiteTags = suiteTags;
    }

    /** Retrieves all configured metadata and return a copy of the map. */
    public MultiMap<String, String> getAllMetaData() {
        MultiMap<String, String> copy = new MultiMap<>();
        copy.putAll(mMetaData);
        return copy;
    }

    /** Get the named metadata entries */
    public List<String> getMetaData(String name) {
        List<String> entry = mMetaData.get(name);
        if (entry == null) {
            return null;
        }
        return new ArrayList<>(entry);
    }

    @VisibleForTesting
    public void setMetaData(MultiMap<String, String> metadata) {
        mMetaData = metadata;
    }

    /**
     * Add a value for a given key to the metadata entries.
     *
     * @param key {@link String} of the key to add values to.
     * @param value A{@link String} of the additional value.
     */
    public void addMetadata(String key, String value) {
        mMetaData.put(key, value);
    }

    /**
     * Add more values of a given key to the metadata entries.
     *
     * @param key {@link String} of the key to add values to.
     * @param values a list of {@link String} of the additional values.
     */
    public void addMetadata(String key, List<String> values) {
        for (String source : values) {
            mMetaData.put(key, source);
        }
    }

    /** Returns if the configuration is shardable or not as part of a suite */
    public boolean isNotShardable() {
        return mNotShardable;
    }

    /** Returns if the configuration is strict shardable or not as part of a suite */
    public boolean isNotStrictShardable() {
        return mNotStrictShardable;
    }

    /** Sets the abi the configuration is going to run against. */
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    /** Returns the abi the configuration is running against if known, null otherwise. */
    public IAbi getAbi() {
        return mAbi;
    }

    /** If this configuration represents a module, we can set the module name associated with it. */
    public void setModuleName(String name) {
        mModuleName = name;
    }

    /** Returns the module name of the module configuration. */
    public String getModuleName() {
        return mModuleName;
    }

    /** Returns true if the invocation should run in sandboxed mode. False otherwise. */
    public boolean shouldUseSandbox() {
        return mUseSandboxing;
    }

    /** Sets whether or not a config will run in sandboxed mode or not. */
    public void setSandboxed(boolean useSandboxed) {
        mUseSandboxing = useSandboxed;
    }

    /**
     * Add the option to a list of options that can be used to rerun the test.
     *
     * @param optionDef a {@link OptionDef} object of the test option.
     */
    public void addRerunOption(OptionDef optionDef) {
        mRerunOptions.add(optionDef);
    }

    /** Get the list of {@link OptionDef} that can be used for rerun. */
    public List<OptionDef> getRerunOptions() {
        return mRerunOptions;
    }

    /** Convert the current instance of the descriptor into its proto format. */
    public ConfigurationDescription.Descriptor toProto() {
        Descriptor.Builder descriptorBuilder = Descriptor.newBuilder();
        // Test Suite Tags
        descriptorBuilder.addAllTestSuiteTag(mSuiteTags);
        // Metadata
        List<Metadata> metadatas = new ArrayList<>();
        for (String key : mMetaData.keySet()) {
            Metadata value =
                    Metadata.newBuilder().setKey(key).addAllValue(mMetaData.get(key)).build();
            metadatas.add(value);
        }
        descriptorBuilder.addAllMetadata(metadatas);
        // Shardable
        descriptorBuilder.setShardable(!mNotShardable);
        // Strict Shardable
        descriptorBuilder.setStrictShardable(!mNotStrictShardable);
        // Use sandboxing
        descriptorBuilder.setUseSandboxing(mUseSandboxing);
        // Module name
        if (mModuleName != null) {
            descriptorBuilder.setModuleName(mModuleName);
        }
        // Abi
        if (mAbi != null) {
            descriptorBuilder.setAbi(mAbi.toProto());
        }
        return descriptorBuilder.build();
    }

    /** Inverse operation from {@link #toProto()} to get the object back. */
    public static ConfigurationDescriptor fromProto(
            ConfigurationDescription.Descriptor protoDescriptor) {
        ConfigurationDescriptor configDescriptor = new ConfigurationDescriptor();
        // Test Suite Tags
        configDescriptor.mSuiteTags.addAll(protoDescriptor.getTestSuiteTagList());
        // Metadata
        for (Metadata meta : protoDescriptor.getMetadataList()) {
            for (String value : meta.getValueList()) {
                configDescriptor.mMetaData.put(meta.getKey(), value);
            }
        }
        // Shardable
        configDescriptor.mNotShardable = !protoDescriptor.getShardable();
        // Strict Shardable
        configDescriptor.mNotStrictShardable = !protoDescriptor.getStrictShardable();
        // Use sandboxing
        configDescriptor.mUseSandboxing = protoDescriptor.getUseSandboxing();
        // Module Name
        if (!protoDescriptor.getModuleName().isEmpty()) {
            configDescriptor.mModuleName = protoDescriptor.getModuleName();
        }
        // Abi
        if (protoDescriptor.hasAbi()) {
            configDescriptor.mAbi = Abi.fromProto(protoDescriptor.getAbi());
        }
        return configDescriptor;
    }

    /** Return a deep-copy of the {@link ConfigurationDescriptor} object. */
    @Override
    public ConfigurationDescriptor clone() {
        return fromProto(this.toProto());
    }
}
