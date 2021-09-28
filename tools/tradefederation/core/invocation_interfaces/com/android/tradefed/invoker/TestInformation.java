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
package com.android.tradefed.invoker;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.List;

/**
 * Holder object that contains all the information and dependencies a test runner or test might need
 * to execute properly.
 */
public class TestInformation {
    /** The context of the invocation or module in progress */
    private final IInvocationContext mContext;
    /** Properties generated during execution. */
    private final ExecutionProperties mProperties;
    /**
     * Files generated during execution that needs to be carried, they will be deleted at the end of
     * the invocation.
     */
    private final ExecutionFiles mExecutionFiles;

    /** Main folder for all dependencies of tests */
    private final File mDependenciesFolder;

    private int mPrimaryDeviceIndex = 0;

    private TestInformation(Builder builder) {
        mContext = builder.mContext;
        mProperties = builder.mProperties;
        mDependenciesFolder = builder.mDependenciesFolder;
        mExecutionFiles = builder.mExecutionFiles;
    }

    private TestInformation(
            TestInformation invocationInfo,
            IInvocationContext moduleContext,
            boolean copyExecFile) {
        mContext = moduleContext;
        mProperties = invocationInfo.mProperties;
        mDependenciesFolder = invocationInfo.mDependenciesFolder;
        if (copyExecFile) {
            mExecutionFiles = new ExecutionFiles();
            mExecutionFiles.putAll(invocationInfo.executionFiles());
        } else {
            mExecutionFiles = invocationInfo.mExecutionFiles;
        }
    }

    /** Create a builder for creating {@link TestInformation} instances. */
    public static Builder newBuilder() {
        return new Builder();
    }

    /** Create an {@link TestInformation} representing a module rather than an invocation. */
    public static TestInformation createModuleTestInfo(
            TestInformation invocationInfo, IInvocationContext moduleContext) {
        return new TestInformation(invocationInfo, moduleContext, false);
    }

    /** Create an {@link TestInformation} with a copied {@link ExecutionFiles}. */
    public static TestInformation createCopyTestInfo(
            TestInformation invocationInfo, IInvocationContext context) {
        return new TestInformation(invocationInfo, context, true);
    }

    /** Returns the current invocation context, or the module context if this is a module. */
    public IInvocationContext getContext() {
        return mContext;
    }

    /** Returns the primary device under tests. */
    public ITestDevice getDevice() {
        return mContext.getDevices().get(mPrimaryDeviceIndex);
    }

    /** Returns the list of devices part of the invocation. */
    public List<ITestDevice> getDevices() {
        return mContext.getDevices();
    }

    /** Returns the primary device build information. */
    public IBuildInfo getBuildInfo() {
        return mContext.getBuildInfos().get(mPrimaryDeviceIndex);
    }

    /**
     * Test Harness internal method to switch which device is returned by default with {@link
     * #getDevice()}. Always reset to 0.
     */
    public final void setActiveDeviceIndex(int index) {
        mPrimaryDeviceIndex = index;
    }

    /**
     * Returns the properties generated during the invocation execution. Passing values and
     * information through the {@link ExecutionProperties} is the recommended way to exchange
     * information between target_preparers and tests.
     */
    public ExecutionProperties properties() {
        return mProperties;
    }

    /**
     * Returns the files generated during the invocation execution. Passing files through the {@link
     * ExecutionFiles} is the recommended way to make a file available between target_preparers and
     * tests.
     */
    public ExecutionFiles executionFiles() {
        return mExecutionFiles;
    }

    /** Returns the folder where all the dependencies are stored for an invocation. */
    public File dependenciesFolder() {
        return mDependenciesFolder;
    }

    /** Builder to create a {@link TestInformation} instance. */
    public static class Builder {
        private IInvocationContext mContext;
        private ExecutionProperties mProperties;
        private File mDependenciesFolder;
        private ExecutionFiles mExecutionFiles;

        private Builder() {
            mProperties = new ExecutionProperties();
            mExecutionFiles = new ExecutionFiles();
        }

        public TestInformation build() {
            return new TestInformation(this);
        }

        public Builder setInvocationContext(IInvocationContext context) {
            this.mContext = context;
            return this;
        }

        public Builder setDependenciesFolder(File dependenciesFolder) {
            this.mDependenciesFolder = dependenciesFolder;
            return this;
        }
    }

    /**
     * Search for a dependency/artifact file based on its name, and whether or not it's a target or
     * host file (for quicker search).
     *
     * @param fileName The name of the file we are looking for.
     * @param targetFirst whether or not we are favoring target-side files vs. host-side files for
     *     the search.
     * @return The found artifact file.
     * @throws FileNotFoundException If the file is not found.
     */
    public File getDependencyFile(String fileName, boolean targetFirst)
            throws FileNotFoundException {
        File dependency = null;
        dependency = getFromEnv(fileName, targetFirst);
        if (dependency != null && dependency.isFile()) {
            return dependency;
        }
        dependency = getFromTestsDir(fileName);
        if (dependency != null && dependency.isFile()) {
            return dependency;
        }
        dependency = getFile(fileName);
        if (dependency != null && dependency.isFile()) {
            return dependency;
        }
        dependency = getFromDependencyFolder(fileName);
        if (dependency != null && dependency.isFile()) {
            return dependency;
        }
        throw new FileNotFoundException(
                String.format("Could not find an artifact file associated with %s", fileName));
    }

    private File getFromEnv(String fileName, boolean targetFirst) {
        FilesKey hostOrTarget = FilesKey.HOST_TESTS_DIRECTORY;
        if (targetFirst) {
            hostOrTarget = FilesKey.TARGET_TESTS_DIRECTORY;
        }
        File testsDir = mExecutionFiles.get(hostOrTarget);
        if (testsDir != null && testsDir.exists()) {
            File file = FileUtil.findFile(testsDir, fileName);
            if (file != null) {
                return file;
            }
        }
        return null;
    }

    private File getFromTestsDir(String fileName) {
        File testsDir = mExecutionFiles.get(FilesKey.TESTS_DIRECTORY);
        if (testsDir != null && testsDir.exists()) {
            File file = FileUtil.findFile(testsDir, fileName);
            if (file == null) {
                // TODO(b/138416078): Once build dependency can be fixed and test required
                // APKs are all under the test module directory, we can remove this fallback
                // approach to do individual download from remote artifact.
                // Try to stage the files from remote zip files.
                file = getBuildInfo().stageRemoteFile(fileName, testsDir);
            }
            return file;
        }
        return null;
    }

    private File getFile(String fileName) {
        return mExecutionFiles.get(fileName);
    }

    private File getFromDependencyFolder(String fileName) {
        File testsDir = mDependenciesFolder;
        if (testsDir != null && testsDir.exists()) {
            File file = FileUtil.findFile(testsDir, fileName);
            if (file != null) {
                return file;
            }
        }
        return null;
    }
}
