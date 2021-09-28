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

package com.android.tradefed.targetprep;

import com.android.annotations.VisibleForTesting;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.EnvUtil;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.VtsPythonRunnerHelper;
import com.android.tradefed.util.VtsVendorConfigFileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Sets up a Python virtualenv on the host and installs packages. To activate it, the working
 * directory is changed to the root of the virtualenv.
 *
 * This's a fork of PythonVirtualenvPreparer and is forked in order to simplify the change
 * deployment process and reduce the deployment time, which are critical for VTS services.
 * That means changes here will be upstreamed gradually.
 */
@OptionClass(alias = "python-venv")
public class VtsPythonVirtualenvPreparer implements IMultiTargetPreparer {
    private static final String LOCAL_PYPI_PATH_ENV_VAR_NAME = "VTS_PYPI_PATH";
    private static final String LOCAL_PYPI_PATH_KEY = "pypi_packages_path";
    private static final int SECOND_IN_MSECS = 1000;
    private static final int MINUTE_IN_MSECS = 60 * SECOND_IN_MSECS;
    protected static final int PIP_RETRY = 3;
    private static final int PIP_RETRY_WAIT = 3 * SECOND_IN_MSECS;
    protected static final int PIP_INSTALL_DELAY = SECOND_IN_MSECS;
    public static final String VIRTUAL_ENV_V3 = "VIRTUAL_ENV_V3";
    public static final String VIRTUAL_ENV = "VIRTUAL_ENV";

    @Option(name = "venv-dir", description = "path of an existing virtualenv to use")
    protected File mVenvDir = null;

    @Option(name = "requirements-file", description = "pip-formatted requirements file")
    private File mRequirementsFile = null;

    @Option(name = "script-file", description = "scripts which need to be executed in advance")
    private Collection<String> mScriptFiles = new TreeSet<>();

    @Option(name = "dep-module", description = "modules which need to be installed by pip")
    protected Collection<String> mDepModules = new LinkedHashSet<>();

    @Option(name = "no-dep-module", description = "modules which should not be installed by pip")
    private Collection<String> mNoDepModules = new TreeSet<>();

    @Option(name = "reuse",
            description = "Reuse an exising virtualenv path if exists in "
                    + "temp directory. When this option is enabled, virtualenv directory used or "
                    + "created by this preparer will not be deleted after tests complete.")
    protected boolean mReuse = true;

    @Option(name = "python-version",
            description = "The version of a Python interpreter to use."
                    + "Currently, only major version number is fully supported."
                    + "Example: \"2\", or \"3\".")
    private String mPythonVersion = "2";

    @Option(name = "virtual-env-intallation-wait-time",
            isTimeVal = true,
            description = "The maximum wait time for virtual env installation.")
    private long mVirtualEnvInstallationWaitTime = 600000L;

    private IBuildInfo mBuildInfo = null;
    private DeviceDescriptor mDescriptor = null;
    private IRunUtil mRunUtil = new RunUtil();

    String mLocalPypiPath = null;
    String mPipPath = null;

    // A map of initially installed pip modules and versions. Newly installed modules are not
    // currently added automatically.
    private Map<String, String> mPipInstallList = null;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setUp(IInvocationContext context)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        mBuildInfo = context.getBuildInfos().get(0);
        ITestDevice device = context.getDevices().get(0);
        mDescriptor = device.getDeviceDescriptor();
        // Ensure the method is locked even across instances
        synchronized (VtsPythonVirtualenvPreparer.class) {
            // Get virtual-env if existing
            if (mVenvDir == null) {
                mVenvDir = checkTestPlanLevelVirtualenv(mBuildInfo);
                if (mVenvDir == null) {
                    mVenvDir = createVirtualEnvCache(mBuildInfo);
                }
            }
            if (new File(mVenvDir, "complete").exists()) {
                VtsPythonRunnerHelper.activateVirtualenv(getRunUtil(), mVenvDir.getAbsolutePath());
            } else {
                // If cache is not good.
                CLog.d("Preparing python dependencies...");

                if (!createVirtualenv(mVenvDir)) {
                    throw new TargetSetupError("Failed to create the virtual-env", mDescriptor);
                }
                CLog.d("Python virtualenv path is: " + mVenvDir);
                VtsPythonRunnerHelper.activateVirtualenv(getRunUtil(), mVenvDir.getAbsolutePath());
                try {
                    new File(mVenvDir, "complete").createNewFile();
                } catch (IOException e) {
                    throw new TargetSetupError(
                            "Failed to mark virtualenv complete.", e, mDescriptor);
                }
            }
            // Setup the dependencies no matter what.
            setLocalPypiPath();
            installDeps();
            // Set the built virtual-env in the build info.
            CLog.d("Python virtualenv path is: " + mVenvDir);
            addPathToBuild(mBuildInfo, mVenvDir);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void tearDown(IInvocationContext context, Throwable e)
            throws DeviceNotAvailableException {
        mVenvDir = null;
    }

    /**
     * This method sets mLocalPypiPath, the local PyPI package directory to
     * install python packages from in the installDeps method.
     */
    protected void setLocalPypiPath() {
        VtsVendorConfigFileUtil configReader = new VtsVendorConfigFileUtil();
        if (configReader.LoadVendorConfig(mBuildInfo)) {
            // First try to load local PyPI directory path from vendor config file
            try {
                String pypiPath = configReader.GetVendorConfigVariable(LOCAL_PYPI_PATH_KEY);
                if (pypiPath.length() > 0 && dirExistsAndHaveReadAccess(pypiPath)) {
                    mLocalPypiPath = pypiPath;
                    CLog.d(String.format("Loaded %s: %s", LOCAL_PYPI_PATH_KEY, mLocalPypiPath));
                }
            } catch (NoSuchElementException e) {
                /* continue */
            }
        }

        // If loading path from vendor config file is unsuccessful,
        // check local pypi path defined by LOCAL_PYPI_PATH_ENV_VAR_NAME
        if (mLocalPypiPath == null) {
            CLog.d("Checking whether local pypi packages directory exists");
            String pypiPath = System.getenv(LOCAL_PYPI_PATH_ENV_VAR_NAME);
            if (pypiPath == null) {
                CLog.d("Local pypi packages directory not specified by env var %s",
                        LOCAL_PYPI_PATH_ENV_VAR_NAME);
            } else if (dirExistsAndHaveReadAccess(pypiPath)) {
                mLocalPypiPath = pypiPath;
                CLog.d("Set local pypi packages directory to %s", pypiPath);
            }
        }

        if (mLocalPypiPath == null) {
            CLog.d("Failed to set local pypi packages path. Therefore internet connection to "
                    + "https://pypi.python.org/simple/ must be available to run VTS tests.");
        }
    }

    /**
     * This method returns whether the given path is a dir that exists and the user has read access.
     */
    private boolean dirExistsAndHaveReadAccess(String path) {
        File pathDir = new File(path);
        if (!pathDir.exists() || !pathDir.isDirectory()) {
            CLog.d("Directory %s does not exist.", pathDir);
            return false;
        }

        if (!EnvUtil.isOnWindows()) {
            CommandResult c = getRunUtil().runTimedCmd(MINUTE_IN_MSECS, "ls", path);
            if (c.getStatus() != CommandStatus.SUCCESS) {
                CLog.d(String.format("Failed to read dir: %s. Result %s. stdout: %s, stderr: %s",
                        path, c.getStatus(), c.getStdout(), c.getStderr()));
                return false;
            }
            return true;
        } else {
            try {
                String[] pathDirList = pathDir.list();
                if (pathDirList == null) {
                    CLog.d("Failed to read dir: %s. Please check access permission.", pathDir);
                    return false;
                }
            } catch (SecurityException e) {
                CLog.d(String.format(
                        "Failed to read dir %s with SecurityException %s", pathDir, e));
                return false;
            }
            return true;
        }
    }

    /**
     * Installs all python pip module dependencies specified in options.
     * @throws TargetSetupError if failed
     */
    protected void installDeps() throws TargetSetupError {
        boolean hasDependencies = false;
        if (!mScriptFiles.isEmpty()) {
            for (String scriptFile : mScriptFiles) {
                CLog.d("Attempting to execute a script, %s", scriptFile);
                CommandResult c = getRunUtil().runTimedCmd(5 * MINUTE_IN_MSECS, scriptFile);
                if (c.getStatus() != CommandStatus.SUCCESS) {
                    CLog.e("Executing script %s failed", scriptFile);
                    throw new TargetSetupError("Failed to source a script", mDescriptor);
                }
            }
        }

        if (mRequirementsFile != null) {
            hasDependencies = true;
            boolean success = false;

            long retry_interval = PIP_RETRY_WAIT;
            for (int try_count = 0; try_count < PIP_RETRY + 1; try_count++) {
                if (try_count > 0) {
                    getRunUtil().sleep(retry_interval);
                    retry_interval *= 3;
                }

                if (installPipRequirementFile(mRequirementsFile)) {
                    success = true;
                    break;
                }
            }

            if (!success) {
                throw new TargetSetupError(
                        "Failed to install pip requirement file " + mRequirementsFile, mDescriptor);
            }
        }

        if (!mDepModules.isEmpty()) {
            for (String dep : mDepModules) {
                hasDependencies = true;

                if (mNoDepModules.contains(dep) || isPipModuleInstalled(dep)) {
                    continue;
                }

                boolean success = installPipModuleLocally(dep);

                long retry_interval = PIP_RETRY_WAIT;
                for (int retry_count = 0; retry_count < PIP_RETRY + 1; retry_count++) {
                    if (retry_count > 0) {
                        getRunUtil().sleep(retry_interval);
                        retry_interval *= 3;
                    }

                    if (success || (!success && installPipModule(dep))) {
                        success = true;
                        getRunUtil().sleep(PIP_INSTALL_DELAY);
                        break;
                    }
                }

                if (!success) {
                    throw new TargetSetupError("Failed to install pip module " + dep, mDescriptor);
                }
            }
        }
        if (!hasDependencies) {
            CLog.d("No dependencies to install");
        }
    }

    /**
     * Installs a pip requirement file from Internet.
     * @param req pip module requirement file object
     * @return true if success. False otherwise
     */
    private boolean installPipRequirementFile(File req) {
        CommandResult result = getRunUtil().runTimedCmd(10 * MINUTE_IN_MSECS, getPipPath(),
                "install", "-r", mRequirementsFile.getAbsolutePath());
        CLog.d(String.format("Result %s. stdout: %s, stderr: %s", result.getStatus(),
                result.getStdout(), result.getStderr()));

        return result.getStatus() == CommandStatus.SUCCESS;
    }

    /**
     * Installs a pip module from local directory.
     * @param name of the module
     * @return true if the module is successfully installed; false otherwise.
     */
    private boolean installPipModuleLocally(String name) {
        if (mLocalPypiPath == null) {
            return false;
        }
        CLog.d("Attempting installation of %s from local directory", name);
        CommandResult result = getRunUtil().runTimedCmd(5 * MINUTE_IN_MSECS, getPipPath(),
                "install", name, "--no-index", "--find-links=" + mLocalPypiPath);
        CLog.d(String.format("Result %s. stdout: %s, stderr: %s", result.getStatus(),
                result.getStdout(), result.getStderr()));

        return result.getStatus() == CommandStatus.SUCCESS;
    }

    /**
     * Install a pip module from Internet
     * @param name of the module
     * @return true if success. False otherwise
     */
    private boolean installPipModule(String name) {
        CLog.d("Attempting installation of %s from PyPI", name);
        CommandResult result =
                getRunUtil().runTimedCmd(5 * MINUTE_IN_MSECS, getPipPath(), "install", name);
        CLog.d("Result %s. stdout: %s, stderr: %s", result.getStatus(), result.getStdout(),
                result.getStderr());
        if (result.getStatus() != CommandStatus.SUCCESS) {
            CLog.e("Installing %s from PyPI failed.", name);
            CLog.d("Attempting to upgrade %s", name);
            result = getRunUtil().runTimedCmd(
                    5 * MINUTE_IN_MSECS, getPipPath(), "install", "--upgrade", name);

            CLog.d(String.format("Result %s. stdout: %s, stderr: %s", result.getStatus(),
                    result.getStdout(), result.getStderr()));
        }

        return result.getStatus() == CommandStatus.SUCCESS;
    }

    /**
     * This method returns absolute pip path in virtualenv.
     *
     * This method is needed because although PATH is set in IRunUtil, IRunUtil will still
     * use pip from system path.
     *
     * @return absolute pip path in virtualenv. null if virtualenv not available.
     */
    public String getPipPath() {
        if (mPipPath != null) {
            return mPipPath;
        }

        String virtualenvPath = mVenvDir.getAbsolutePath();
        if (virtualenvPath == null) {
            return null;
        }
        mPipPath = new File(VtsPythonRunnerHelper.getPythonBinDir(virtualenvPath), "pip")
                           .getAbsolutePath();
        return mPipPath;
    }

    /**
     * Get the major python version from option.
     *
     * Currently, only 2 and 3 are supported.
     *
     * @return major version number
     * @throws TargetSetupError
     */
    protected int getConfiguredPythonVersionMajor() throws TargetSetupError {
        if (mPythonVersion.startsWith("3.") || mPythonVersion.equals("3")) {
            return 3;
        } else if (mPythonVersion.startsWith("2.") || mPythonVersion.equals("2")) {
            return 2;
        } else {
            throw new TargetSetupError("Unsupported python version " + mPythonVersion, mDescriptor);
        }
    }

    /**
     * Add PYTHONPATH and VIRTUAL_ENV_PATH to BuildInfo.
     * @param buildInfo
     * @throws TargetSetupError
     */
    private void addPathToBuild(IBuildInfo buildInfo, File virtualEnvDir) throws TargetSetupError {
        String target = null;
        switch (getConfiguredPythonVersionMajor()) {
            case 2:
                target = VtsPythonVirtualenvPreparer.VIRTUAL_ENV;
                break;
            case 3:
                target = VtsPythonVirtualenvPreparer.VIRTUAL_ENV_V3;
                break;
        }

        if (!buildInfo.getBuildAttributes().containsKey(target)) {
            buildInfo.addBuildAttribute(target, virtualEnvDir.getAbsolutePath());
        }
    }

    /**
     * Completes the creation of virtualenv.
     * @return true if the directory is successfully prepared as virtualenv; false otherwise
     */
    protected boolean createVirtualenv(File virtualEnvDir) {
        CLog.d("Creating virtualenv at " + virtualEnvDir);

        String[] cmd = new String[] {
                "virtualenv", "-p", "python" + mPythonVersion, virtualEnvDir.getAbsolutePath()};

        long waitRetryCreate = 5 * SECOND_IN_MSECS;

        for (int try_count = 0; try_count < PIP_RETRY + 1; try_count++) {
            if (try_count > 0) {
                getRunUtil().sleep(waitRetryCreate);
            }
            CommandResult c = getRunUtil().runTimedCmd(mVirtualEnvInstallationWaitTime, cmd);

            if (!CommandStatus.SUCCESS.equals(c.getStatus())) {
                String message_lower = (c.getStdout() + c.getStderr()).toLowerCase();
                if (message_lower.contains("errno 17") // File exists
                        || message_lower.contains("errno 26")
                        || message_lower.contains("text file busy")) {
                    // Race condition, retry.
                    CLog.e("detected the virtualenv path is being created by other process.");
                } else {
                    // Other error, abort.
                    CLog.e("Exit code: %s, stdout: %s, stderr: %s", c.getStatus(), c.getStdout(),
                            c.getStderr());
                    break;
                }
            } else {
                CLog.d("Successfully created virtualenv at " + virtualEnvDir);
                return true;
            }
        }

        return false;
    }

    private File createVirtualEnvCache(IBuildInfo buildInfo) throws TargetSetupError {
        File workingDir = null;
        File virtualEnvDir = null;
        if (mReuse) {
            try {
                workingDir = new CompatibilityBuildHelper(buildInfo).getDir();
            } catch (FileNotFoundException e) {
                workingDir = new File(System.getProperty("java.io.tmpdir"));
            }
            virtualEnvDir = new File(workingDir, "vts-virtualenv-" + mPythonVersion);
            if (virtualEnvDir.exists()) {
                // Use the cache
                return virtualEnvDir;
            }
            // Create it first
            virtualEnvDir.mkdirs();
        } else {
            try {
                virtualEnvDir = FileUtil.createTempDir("vts-virtualenv-" + mPythonVersion + "-"
                        + normalizeName(buildInfo.getTestTag()) + "_");
            } catch (IOException e) {
                throw new TargetSetupError(
                        "Failed to create a directory for the virtual env.", e, mDescriptor);
            }
        }
        return virtualEnvDir;
    }

    /**
     * Checks whether a test plan-wise common virtualenv directory can be used.
     * @param buildInfo
     * @return true if a test plan-wise virtuanenv directory exists; false otherwise
     * @throws TargetSetupError
     */
    protected File checkTestPlanLevelVirtualenv(IBuildInfo buildInfo) throws TargetSetupError {
        String venvDir = null;
        switch (getConfiguredPythonVersionMajor()) {
            case 2:
                venvDir =
                        buildInfo.getBuildAttributes().get(VtsPythonVirtualenvPreparer.VIRTUAL_ENV);
                break;
            case 3:
                venvDir = buildInfo.getBuildAttributes().get(
                        VtsPythonVirtualenvPreparer.VIRTUAL_ENV_V3);
                break;
        }

        if (venvDir != null && new File(venvDir).exists()) {
            return new File(venvDir);
        }
        return null;
    }

    protected void addDepModule(String module) {
        mDepModules.add(module);
    }

    protected void setRequirementsFile(File f) {
        mRequirementsFile = f;
    }

    /**
     * Get an instance of {@link IRunUtil}.
     */
    @VisibleForTesting
    protected IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = new RunUtil();
        }
        return mRunUtil;
    }

    /**
     * Locally checks whether a pip module is installed.
     *
     * This read the installed module list from command "pip list" and check whether the
     * module in requirement string is installed and its version satisfied.
     *
     * Note: This method is only a help method for speed optimization purpose.
     *       It does not check dependencies of the module.
     *       It replace dots "." in module name with dash "-".
     *       If the "pip list" command failed, it will return false and will not throw exception
     *       It can also only accept one ">=" version requirement string.
     *       If this method returns false, the requirement should still be checked using pip itself.
     *
     * @param requirement such as "numpy", "pip>=9"
     * @return True if module is installed locally with correct version. False otherwise
     */
    private boolean isPipModuleInstalled(String requirement) {
        if (mPipInstallList == null) {
            mPipInstallList = getInstalledPipModules();
            if (mPipInstallList == null) {
                CLog.e("Failed to read local pip install list.");
                return false;
            }
        }

        String name;
        String version = null;
        if (requirement.contains(">=")) {
            String[] tokens = requirement.split(">=");
            if (tokens.length != 2) {
                return false;
            }
            name = tokens[0];
            version = tokens[1];
        } else if (requirement.contains("=") || requirement.contains("<")
                || requirement.contains(">")) {
            return false;
        } else {
            name = requirement;
        }

        name = name.replaceAll("\\.", "-");

        if (!mPipInstallList.containsKey(name)) {
            return false;
        }

        // TODO: support other comparison and multiple condition if there's a use case.
        if (version != null && !isVersionGreaterEqual(mPipInstallList.get(name), version)) {
            return false;
        }

        return true;
    }

    /**
     * Compares whether version string 1 is greater or equal to version string 2
     * @param version1
     * @param version2
     * @return True if the value of version1 >= version2
     */
    private static boolean isVersionGreaterEqual(String version1, String version2) {
        version1 = version1.replaceAll("[^0-9.]+", "");
        version2 = version2.replaceAll("[^0-9.]+", "");

        String[] tokens1 = version1.split("\\.");
        String[] tokens2 = version2.split("\\.");

        int length = Math.max(tokens1.length, tokens2.length);
        for (int i = 0; i < length; i++) {
            try {
                int token1 = i < tokens1.length ? Integer.parseInt(tokens1[i]) : 0;
                int token2 = i < tokens2.length ? Integer.parseInt(tokens2[i]) : 0;
                if (token1 < token2) {
                    return false;
                }
            } catch (NumberFormatException e) {
                CLog.e("failed to compare pip module version: %s and %s", version1, version2);
                return false;
            }
        }

        return true;
    }

    /**
     * Gets map of installed pip packages and their versions.
     * @return installed pip packages
     */
    private Map<String, String> getInstalledPipModules() {
        CommandResult res = getRunUtil().runTimedCmd(30 * SECOND_IN_MSECS, getPipPath(), "list");
        if (res.getStatus() != CommandStatus.SUCCESS) {
            CLog.e(String.format("Failed to read pip installed list: "
                            + "Result %s. stdout: %s, stderr: %s",
                    res.getStatus(), res.getStdout(), res.getStderr()));
            return null;
        }
        String raw = res.getStdout();
        String[] lines = raw.split("\\r?\\n");

        TreeMap<String, String> pipInstallList = new TreeMap<>();

        for (String line : lines) {
            line = line.trim();
            if (line.length() == 0 || line.startsWith("Package ") || line.startsWith("-")) {
                continue;
            }
            String[] tokens = line.split("\\s+");
            if (tokens.length != 2) {
                CLog.e("Error parsing pip installed package list. Line text: " + line);
                continue;
            }
            pipInstallList.put(tokens[0], tokens[1]);
        }

        return pipInstallList;
    }

    /**
     * Replacing characters in a string to make it a valid file name.
     *
     * The current method is to replace any non-word character with '_' except '.' and '-'.
     *
     * @param name the potential name of a file to normalize.
     *                 Do not use path here as path delimitor will be replaced
     * @return normalized file name
     */
    private String normalizeName(String name) {
        return name.replaceAll("[^\\w.-]", "_");
    }
}
