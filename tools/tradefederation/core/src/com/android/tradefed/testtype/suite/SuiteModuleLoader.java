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
package com.android.tradefed.testtype.suite;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.ConfigurationUtil;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.OptionDef;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFileFilterReceiver;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.suite.params.IModuleParameter;
import com.android.tradefed.testtype.suite.params.MainlineModuleHandler;
import com.android.tradefed.testtype.suite.params.ModuleParameters;
import com.android.tradefed.testtype.suite.params.ModuleParametersHelper;
import com.android.tradefed.testtype.suite.params.NegativeHandler;
import com.android.tradefed.testtype.suite.params.NotMultiAbiHandler;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.StreamUtil;

import com.google.common.base.Strings;
import com.google.common.net.UrlEscapers;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Retrieves Compatibility test module definitions from the repository. TODO: Add the expansion of
 * suite when loading a module.
 */
public class SuiteModuleLoader {

    public static final String CONFIG_EXT = ".config";
    private Map<String, List<OptionDef>> mTestOrPreparerOptions = new HashMap<>();
    private Map<String, List<OptionDef>> mModuleOptions = new HashMap<>();
    private boolean mIncludeAll;
    private Map<String, List<SuiteTestFilter>> mIncludeFilters = new HashMap<>();
    private Map<String, List<SuiteTestFilter>> mExcludeFilters = new HashMap<>();
    private IConfigurationFactory mConfigFactory = ConfigurationFactory.getInstance();
    private IInvocationContext mContext;

    private boolean mAllowParameterizedModules = false;
    private boolean mAllowMainlineParameterizedModules = false;
    private boolean mAllowOptionalParameterizedModules = false;
    private ModuleParameters mForcedModuleParameter = null;
    private Set<ModuleParameters> mExcludedModuleParameters = new HashSet<>();
    // Check the mainline parameter configured in a test config must end with .apk, .apks, or .apex.
    private static final Set<String> MAINLINE_PARAMETERS_TO_VALIDATE =
            new HashSet<>(Arrays.asList(".apk", ".apks", ".apex"));

    /**
     * Ctor for the SuiteModuleLoader.
     *
     * @param includeFilters The formatted and parsed include filters.
     * @param excludeFilters The formatted and parsed exclude filters.
     * @param testArgs the list of test ({@link IRemoteTest}) arguments.
     * @param moduleArgs the list of module arguments.
     */
    public SuiteModuleLoader(
            Map<String, List<SuiteTestFilter>> includeFilters,
            Map<String, List<SuiteTestFilter>> excludeFilters,
            List<String> testArgs,
            List<String> moduleArgs) {
        mIncludeAll = includeFilters.isEmpty();
        mIncludeFilters = includeFilters;
        mExcludeFilters = excludeFilters;

        parseArgs(testArgs, mTestOrPreparerOptions);
        parseArgs(moduleArgs, mModuleOptions);
    }

    public final void setInvocationContext(IInvocationContext context) {
        mContext = context;
    }

    /** Sets whether or not to allow parameterized modules. */
    public final void setParameterizedModules(boolean allowed) {
        mAllowParameterizedModules = allowed;
    }

    /** Sets whether or not to allow parameterized mainline modules. */
    public final void setMainlineParameterizedModules(boolean allowed) {
        mAllowMainlineParameterizedModules = allowed;
    }

    /** Sets whether or not to allow optional parameterized modules. */
    public final void setOptionalParameterizedModules(boolean allowed) {
        mAllowOptionalParameterizedModules = allowed;
    }

    /** Sets the only {@link ModuleParameters} type that should be run. */
    public final void setModuleParameter(ModuleParameters param) {
        mForcedModuleParameter = param;
    }

    /** Sets the set of {@link ModuleParameters} that should not be considered at all. */
    public final void setExcludedModuleParameters(Set<ModuleParameters> excludedParams) {
        mExcludedModuleParameters = excludedParams;
    }

    /** Main loading of configurations, looking into the specified files */
    public LinkedHashMap<String, IConfiguration> loadConfigsFromSpecifiedPaths(
            List<File> listConfigFiles,
            Set<IAbi> abis,
            String suiteTag) {
        LinkedHashMap<String, IConfiguration> toRun = new LinkedHashMap<>();
        for (File configFile : listConfigFiles) {
            toRun.putAll(
                    loadOneConfig(
                            configFile.getName(), configFile.getAbsolutePath(), abis, suiteTag));
        }
        return toRun;
    }

    /** Main loading of configurations, looking into a folder */
    public LinkedHashMap<String, IConfiguration> loadConfigsFromDirectory(
            List<File> testsDirs,
            Set<IAbi> abis,
            String suitePrefix,
            String suiteTag,
            List<String> patterns) {
        LinkedHashMap<String, IConfiguration> toRun = new LinkedHashMap<>();
        List<File> listConfigFiles = new ArrayList<>();
        listConfigFiles.addAll(
                ConfigurationUtil.getConfigNamesFileFromDirs(suitePrefix, testsDirs, patterns));
        // Ensure stable initial order of configurations.
        Collections.sort(listConfigFiles);
        toRun.putAll(loadConfigsFromSpecifiedPaths(listConfigFiles, abis, suiteTag));
        return toRun;
    }

    /**
     * Main loading of configurations, looking into the resources on the classpath. (TF configs for
     * example).
     */
    public LinkedHashMap<String, IConfiguration> loadConfigsFromJars(
            Set<IAbi> abis, String suitePrefix, String suiteTag) {
        LinkedHashMap<String, IConfiguration> toRun = new LinkedHashMap<>();

        IConfigurationFactory configFactory = ConfigurationFactory.getInstance();
        List<String> configs = configFactory.getConfigList(suitePrefix, false);
        // Sort configs to ensure they are always evaluated and added in the same order.
        Collections.sort(configs);
        toRun.putAll(loadTfConfigsFromSpecifiedPaths(configs, abis, suiteTag));
        return toRun;
    }

    /** Main loading of configurations, looking into the specified resources on the classpath. */
    public LinkedHashMap<String, IConfiguration> loadTfConfigsFromSpecifiedPaths(
        List<String> configs,
        Set<IAbi> abis,
        String suiteTag) {
        LinkedHashMap<String, IConfiguration> toRun = new LinkedHashMap<>();
        for (String configName : configs) {
            toRun.putAll(loadOneConfig(configName, configName, abis, suiteTag));
        }
        return toRun;
    }

    /**
     * Pass the filters to the {@link IRemoteTest}. Default behavior is to ignore if the IRemoteTest
     * does not implements {@link ITestFileFilterReceiver}. This can be overriden to create a more
     * restrictive behavior.
     *
     * @param test The {@link IRemoteTest} that is being considered.
     * @param abi The Abi we are currently working on.
     * @param moduleId The id of the module (usually abi + module name).
     * @param includeFilters The formatted and parsed include filters.
     * @param excludeFilters The formatted and parsed exclude filters.
     */
    public void addFiltersToTest(
            IRemoteTest test,
            IAbi abi,
            String moduleId,
            Map<String, List<SuiteTestFilter>> includeFilters,
            Map<String, List<SuiteTestFilter>> excludeFilters) {
        if (!(test instanceof ITestFilterReceiver)) {
            CLog.e("Test in module %s does not implement ITestFilterReceiver.", moduleId);
            return;
        }
        List<SuiteTestFilter> mdIncludes = getFilterList(includeFilters, moduleId);
        List<SuiteTestFilter> mdExcludes = getFilterList(excludeFilters, moduleId);
        if (!mdIncludes.isEmpty()) {
            addTestIncludes((ITestFilterReceiver) test, mdIncludes, moduleId);
        }
        if (!mdExcludes.isEmpty()) {
            addTestExcludes((ITestFilterReceiver) test, mdExcludes, moduleId);
        }
    }

    /**
     * Load a single config location (file or on TF classpath). It can results in several {@link
     * IConfiguration}. If a single configuration get expanded in different ways.
     *
     * @param configName The actual config name only. (no path)
     * @param configFullName The fully qualified config name. (with path, if any).
     * @param abis The set of all abis that needs to run.
     * @param suiteTag the Tag of the suite aimed to be run.
     * @return A map of loaded configuration.
     */
    private LinkedHashMap<String, IConfiguration> loadOneConfig(
            String configName, String configFullName, Set<IAbi> abis, String suiteTag) {
        LinkedHashMap<String, IConfiguration> toRun = new LinkedHashMap<>();
        final String name = configName.replace(CONFIG_EXT, "");
        final String[] pathArg = new String[] {configFullName};
        try {
            boolean primaryAbi = true;
            boolean shouldCreateMultiAbi = true;
            // If a particular parameter was requested to be run, find it.
            IModuleParameter mForcedParameter = null;
            if (mForcedModuleParameter != null) {
                mForcedParameter =
                        ModuleParametersHelper.getParameterHandler(
                                mForcedModuleParameter, /* optionalParams */
                                mAllowOptionalParameterizedModules);
            }

            // Invokes parser to process the test module config file
            // Need to generate a different config for each ABI as we cannot guarantee the
            // configs are idempotent. This however means we parse the same file multiple times
            for (IAbi abi : abis) {
                // Only enable the primary abi filtering when switching to the parameterized mode
                if (mAllowParameterizedModules && !primaryAbi && !shouldCreateMultiAbi) {
                    continue;
                }
                String baseId = AbiUtils.createId(abi.getName(), name);
                IConfiguration config = null;
                try {
                    config = mConfigFactory.createConfigurationFromArgs(pathArg);
                } catch (ConfigurationException e) {
                    // If the module should not have been running in the first place, give it a
                    // pass on the configuration failure.
                    if (!shouldRunModule(baseId)) {
                        primaryAbi = false;
                        // If the module should not run tests based on the state of filters,
                        // skip this name/abi combination.
                        continue;
                    }
                    throw e;
                }

                // If a suiteTag is used, we load with it.
                if (!Strings.isNullOrEmpty(suiteTag)
                        && !config.getConfigurationDescription()
                                .getSuiteTags()
                                .contains(suiteTag)) {
                    // Do not print here, it could leave several hundred lines of logs.
                    continue;
                }

                boolean skipCreatingBaseConfig = false;
                List<IModuleParameter> params = null;
                List<String> mainlineParams = new ArrayList<>();
                try {
                    params = getModuleParameters(name, config);
                    mainlineParams = getMainlineModuleParameters(config);
                } catch (ConfigurationException e) {
                    // If the module should not have been running in the first place, give it a
                    // pass on the configuration failure.
                    if (!shouldRunModule(baseId)) {
                        primaryAbi = false;
                        // If the module should not run tests based on the state of filters,
                        // skip this name/abi combination.
                        continue;
                    }
                    throw e;
                }

                // Handle parameterized modules if enabled.
                if (mAllowParameterizedModules) {

                    if (params.isEmpty()
                            && mForcedParameter != null
                            && !(mForcedParameter instanceof NegativeHandler)) {
                        // If the AndroidTest.xml doesn't specify any parameter but we forced a
                        // parameter like 'instant' to execute. In this case we don't create the
                        // standard module.
                        continue;
                    }

                    shouldCreateMultiAbi = shouldCreateMultiAbiForBase(params);

                    // If we find any parameterized combination.
                    for (IModuleParameter param : params) {
                        if (param instanceof NegativeHandler) {
                            if (mForcedParameter != null
                                    && !param.getClass().equals(mForcedParameter.getClass())) {
                                skipCreatingBaseConfig = true;
                            }
                            continue;
                        }
                        if (mForcedParameter != null) {
                            // When a particular parameter is forced, only create it not the others
                            if (param.getClass().equals(mForcedParameter.getClass())) {
                                skipCreatingBaseConfig = true;
                            } else {
                                continue;
                            }
                        }
                        // Only create primary abi of parameterized modules
                        if (!primaryAbi) {
                            continue;
                        }
                        String fullId =
                                String.format("%s[%s]", baseId, param.getParameterIdentifier());
                        String nameWithParam =
                                String.format("%s[%s]", name, param.getParameterIdentifier());
                        if (shouldRunParameterized(baseId, fullId, nameWithParam, mForcedParameter)) {
                            IConfiguration paramConfig =
                                    mConfigFactory.createConfigurationFromArgs(pathArg);
                            // Mark the parameter in the metadata
                            paramConfig
                                    .getConfigurationDescription()
                                    .addMetadata(
                                            ConfigurationDescriptor.ACTIVE_PARAMETER_KEY,
                                            param.getParameterIdentifier());
                            param.addParameterSpecificConfig(paramConfig);
                            setUpConfig(name, nameWithParam, baseId, fullId, paramConfig, abi);
                            param.applySetup(paramConfig);
                            toRun.put(fullId, paramConfig);
                        }
                    }
                }

                if (mAllowMainlineParameterizedModules) {
                    // If no options defined in a test config, skip generating.
                    // TODO(easoncylee) This is still under discussion.
                    if (mainlineParams.isEmpty()) {
                        continue;
                    }
                    // If we find any parameterized combination for mainline modules.
                    for (String param : mainlineParams) {
                        String fullId = String.format("%s[%s]", baseId, param);
                        String nameWithParam = String.format("%s[%s]", name, param);
                        if (!shouldRunParameterized(baseId, fullId, nameWithParam, null)) {
                            continue;
                        }
                        // Create mainline handler for each defined mainline parameter.
                        MainlineModuleHandler handler =
                                new MainlineModuleHandler(
                                        param,
                                        abi,
                                        mContext
                                );
                        skipCreatingBaseConfig = true;
                        IConfiguration paramConfig =
                                mConfigFactory.createConfigurationFromArgs(pathArg);
                        paramConfig
                                .getConfigurationDescription()
                                .addMetadata(
                                        ITestSuite.ACTIVE_MAINLINE_PARAMETER_KEY,
                                        param);
                        setUpConfig(name, nameWithParam, baseId, fullId, paramConfig, abi);
                        handler.applySetup(paramConfig);
                        toRun.put(fullId, paramConfig);
                    }
                }

                primaryAbi = false;
                // If a parameterized form of the module was forced, we don't create the standard
                // version of it.
                if (skipCreatingBaseConfig) {
                    continue;
                }
                if (shouldRunModule(baseId)) {
                    // Always add the base regular configuration to the execution.
                    // Do not pass the nameWithParam in because it would cause the module args be
                    // injected into config twice if we pass nameWithParam using name.
                    setUpConfig(name, null, baseId, baseId, config, abi);
                    toRun.put(baseId, config);
                }
            }
        } catch (ConfigurationException e) {
            throw new RuntimeException(
                    String.format(
                            "Error parsing configuration: %s: '%s'",
                            configFullName, e.getMessage()),
                    e);
        }

        return toRun;
    }

    /** @return the {@link Set} of modules whose name contains the given pattern. */
    public static Set<File> getModuleNamesMatching(
            File directory, String suitePrefix, String pattern) {
        List<File> extraTestCasesDirs = Arrays.asList(directory);
        List<String> patterns = new ArrayList<>();
        patterns.add(pattern);
        Set<File> modules =
                ConfigurationUtil.getConfigNamesFileFromDirs(
                        suitePrefix, extraTestCasesDirs, patterns);
        return modules;
    }

    /**
     * Utility method that allows to parse and create a structure with the option filters.
     *
     * @param stringFilters The original option filters format.
     * @param filters The filters parsed from the string format.
     * @param abis The Abis to consider in the filtering.
     */
    public static void addFilters(
            Set<String> stringFilters, Map<String, List<SuiteTestFilter>> filters, Set<IAbi> abis) {
        for (String filterString : stringFilters) {
            SuiteTestFilter filter = SuiteTestFilter.createFrom(filterString);
            String abi = filter.getAbi();
            if (abi == null) {
                for (IAbi a : abis) {
                    addFilter(a.getName(), filter, filters);
                }
            } else {
                addFilter(abi, filter, filters);
            }
        }
    }

    private static void addFilter(
            String abi, SuiteTestFilter filter, Map<String, List<SuiteTestFilter>> filters) {
        getFilterList(filters, AbiUtils.createId(abi, filter.getName())).add(filter);
    }

    private static List<SuiteTestFilter> getFilterList(
            Map<String, List<SuiteTestFilter>> filters, String id) {
        List<SuiteTestFilter> fs = filters.get(id);
        if (fs == null) {
            fs = new ArrayList<>();
            filters.put(id, fs);
        }
        return fs;
    }

    private boolean shouldRunModule(String moduleId) {
        List<SuiteTestFilter> mdIncludes = getFilterList(mIncludeFilters, moduleId);
        List<SuiteTestFilter> mdExcludes = getFilterList(mExcludeFilters, moduleId);
        // if including all modules or includes exist for this module, and there are not excludes
        // for the entire module, this module should be run.
        return (mIncludeAll || !mdIncludes.isEmpty()) && !containsModuleExclude(mdExcludes);
    }

    /**
     * Except if the parameterized module is explicitly excluded, including the base module result
     * in including its parameterization variant.
     */
    private boolean shouldRunParameterized(
            String baseModuleId,
            String parameterModuleId,
            String nameWithParam,
            IModuleParameter forcedModuleParameter) {
        // Explicitly excluded
        List<SuiteTestFilter> excluded = getFilterList(mExcludeFilters, parameterModuleId);
        List<SuiteTestFilter> excludedParam = getFilterList(mExcludeFilters, nameWithParam);
        if (containsModuleExclude(excluded) || containsModuleExclude(excludedParam)) {
            return false;
        }

        // Implicitly included due to forced parameter
        if (forcedModuleParameter != null) {
            List<SuiteTestFilter> baseInclude = getFilterList(mIncludeFilters, baseModuleId);
            if (!baseInclude.isEmpty()) {
                return true;
            }
        }
        // Explicitly included
        List<SuiteTestFilter> included = getFilterList(mIncludeFilters, parameterModuleId);
        List<SuiteTestFilter> includedParam = getFilterList(mIncludeFilters, nameWithParam);
        if (mIncludeAll || !included.isEmpty() || !includedParam.isEmpty()) {
            return true;
        }
        return false;
    }

    private void addTestIncludes(
            ITestFilterReceiver test, List<SuiteTestFilter> includes, String moduleId) {
        if (test instanceof ITestFileFilterReceiver) {
            String escapedFileName = escapeFilterFileName(moduleId);
            File includeFile = createFilterFile(escapedFileName, ".include", includes);
            ((ITestFileFilterReceiver) test).setIncludeTestFile(includeFile);
        } else {
            // add test includes one at a time
            for (SuiteTestFilter include : includes) {
                String filterTestName = include.getTest();
                if (filterTestName != null) {
                    test.addIncludeFilter(filterTestName);
                }
            }
        }
    }

    private void addTestExcludes(
            ITestFilterReceiver test, List<SuiteTestFilter> excludes, String moduleId) {
        if (test instanceof ITestFileFilterReceiver) {
            String escapedFileName = escapeFilterFileName(moduleId);
            File excludeFile = createFilterFile(escapedFileName, ".exclude", excludes);
            ((ITestFileFilterReceiver) test).setExcludeTestFile(excludeFile);
        } else {
            // add test excludes one at a time
            for (SuiteTestFilter exclude : excludes) {
                test.addExcludeFilter(exclude.getTest());
            }
        }
    }

    /** module id can contain special characters, avoid them for file names. */
    private String escapeFilterFileName(String moduleId) {
        String escaped = UrlEscapers.urlPathSegmentEscaper().escape(moduleId);
        return escaped;
    }

    private File createFilterFile(String prefix, String suffix, List<SuiteTestFilter> filters) {
        File filterFile = null;
        PrintWriter out = null;
        try {
            filterFile = FileUtil.createTempFile(prefix, suffix);
            out = new PrintWriter(filterFile);
            for (SuiteTestFilter filter : filters) {
                String filterTest = filter.getTest();
                if (filterTest != null) {
                    out.println(filterTest);
                }
            }
            out.flush();
        } catch (IOException e) {
            throw new RuntimeException("Failed to create filter file");
        } finally {
            StreamUtil.close(out);
        }
        filterFile.deleteOnExit();
        return filterFile;
    }

    /** Returns true iff one or more test filters in excludes apply to the entire module. */
    private boolean containsModuleExclude(Collection<SuiteTestFilter> excludes) {
        for (SuiteTestFilter exclude : excludes) {
            if (exclude.getTest() == null) {
                return true;
            }
        }
        return false;
    }

    /** A {@link FilenameFilter} to find all the config files in a directory. */
    public static class ConfigFilter implements FilenameFilter {

        /** {@inheritDoc} */
        @Override
        public boolean accept(File dir, String name) {
            return name.endsWith(CONFIG_EXT);
        }
    }

    /**
     * Parse a list of args formatted as expected into {@link OptionDef} to be injected to module
     * configurations.
     *
     * <p>Format: <module name / module id / class runner>:<option name>:[<arg-key>:=]<arg-value>
     */
    private void parseArgs(List<String> args, Map<String, List<OptionDef>> moduleOptions) {
        for (String arg : args) {
            int moduleSep = arg.indexOf(":");
            if (moduleSep == -1) {
                throw new RuntimeException("Expected delimiter ':' for module or class.");
            }
            String moduleName = arg.substring(0, moduleSep);
            String remainder = arg.substring(moduleSep + 1);
            List<OptionDef> listOption = moduleOptions.get(moduleName);
            if (listOption == null) {
                listOption = new ArrayList<>();
                moduleOptions.put(moduleName, listOption);
            }
            int optionNameSep = remainder.indexOf(":");
            if (optionNameSep == -1) {
                throw new RuntimeException(
                        "Expected delimiter ':' between option name and values.");
            }
            String optionName = remainder.substring(0, optionNameSep);
            Pattern pattern = Pattern.compile("\\{(.*)\\}(.*)");
            Matcher match = pattern.matcher(optionName);
            if (match.find()) {
                String alias = match.group(1);
                String name = match.group(2);
                optionName = alias + ":" + name;
            }
            String optionValueString = remainder.substring(optionNameSep + 1);
            // TODO: See if QuotationTokenizer can be improved for multi-character delimiter.
            // or change the delimiter to a single char.
            String[] tokens = optionValueString.split(":=", 2);
            OptionDef option = null;
            if (tokens.length == 1) {
                option = new OptionDef(optionName, tokens[0], moduleName);
            } else if (tokens.length == 2) {
                option = new OptionDef(optionName, tokens[0], tokens[1], moduleName);
            }
            listOption.add(option);
        }
    }

    /** Gets the list of {@link IModuleParameter}s associated with a module. */
    private List<IModuleParameter> getModuleParameters(String moduleName, IConfiguration config)
            throws ConfigurationException {
        List<IModuleParameter> params = new ArrayList<>();
        Set<String> processedParameterArgs = new HashSet<>();
        // Track family of the parameters to make sure we have no duplicate.
        Map<String, ModuleParameters> duplicateModule = new LinkedHashMap<>();

        List<String> parameters =
                config.getConfigurationDescription().getMetaData(ITestSuite.PARAMETER_KEY);
        if (parameters == null || parameters.isEmpty()) {
            return params;
        }
        for (String p : parameters) {
            if (!processedParameterArgs.add(p)) {
                // Avoid processing the same parameter twice
                continue;
            }
            ModuleParameters suiteParam = ModuleParameters.valueOf(p.toUpperCase());
            String family = suiteParam.getFamily();
            if (duplicateModule.containsKey(family)) {
                // Duplicate family members are not accepted.
                throw new ConfigurationException(
                        String.format(
                                "Module %s is declaring parameter: "
                                        + "%s and %s when only one expected.",
                                moduleName, suiteParam, duplicateModule.get(family)));
            } else {
                duplicateModule.put(suiteParam.getFamily(), suiteParam);
            }
            // Do not consider the excluded parameterization dimension
            if (mExcludedModuleParameters.contains(suiteParam)) {
                CLog.d("'%s' was excluded via exclude-module-parameters.", moduleName);
                continue;
            }
            IModuleParameter handler =
                    ModuleParametersHelper.getParameterHandler(
                            suiteParam, /* optionalParams */ mAllowOptionalParameterizedModules);
            if (handler != null) {
                params.add(handler);
            }
        }
        return params;
    }

    /** Gets the list of parameterized mainline modules associated with a module. */
    @VisibleForTesting
    List<String> getMainlineModuleParameters(IConfiguration config)
            throws ConfigurationException {
        List<String> params = new ArrayList<>();

        List<String> parameters =
                config.getConfigurationDescription().getMetaData(ITestSuite.MAINLINE_PARAMETER_KEY);
        if (parameters == null || parameters.isEmpty()) {
            return params;
        }

        return new ArrayList<>(dedupMainlineParameters(parameters, config.getName()));
    }

    /**
     * De-duplicate the given mainline parameters.
     *
     * @param parameters The list of given mainline parameters.
     * @param configName The test configuration name.
     * @return The de-duplicated mainline modules list.
     */
    @VisibleForTesting
    Set<String> dedupMainlineParameters(List<String> parameters, String configName)
            throws ConfigurationException {
        Set<String> results = new HashSet<>();
        for (String param : parameters) {
            if (!isValidMainlineParam(param)) {
                throw new ConfigurationException(
                        String.format(
                                "Illegal mainline module parameter: \"%s\" configured in the " +
                                "test config: %s. Parameter must end with .apk/.apex/.apks and " +
                                "have no any spaces configured.", param, configName)
                );
            }
            if (!isInAlphabeticalOrder(param)) {
                throw new ConfigurationException(
                        String.format(
                                "Illegal mainline module parameter: \"%s\" configured in the " +
                                "test config: %s. Parameter must be configured in alphabetical " +
                                "order or with no duplicated modules.", param, configName)
                );
            }
            results.add(param);
        }
        return results;
    }

    /** Whether a mainline parameter configured in a test config is in alphabetical order or not. */
    @VisibleForTesting
    boolean isInAlphabeticalOrder(String param) {
        String previousString = "";
        for (String currentString : param.split(String.format("\\+"))) {
            // This is to check if the parameter is in alphabetical order or duplicated.
            if (currentString.compareTo(previousString) <= 0) {
                return false;
            }
            previousString = currentString;
        }
        return true;
    }

    /** Whether the mainline parameter configured in the test config is valid or not. */
    @VisibleForTesting
    boolean isValidMainlineParam(String param) {
        if (param.contains(" ")) {
            return false;
        }
        for (String m : param.split(String.format("\\+"))) {
            if (!MAINLINE_PARAMETERS_TO_VALIDATE.stream().anyMatch(entry -> m.endsWith(entry))) {
                return false;
            }
        }
        return true;
    }

    /**
     * Setup the options for the module configuration.
     *
     * @param name The base name of the module
     * @param nameWithParam The id of the parameterized mainline module (module name + parameters)
     * @param id The base id name of the module.
     * @param fullId The full id of the module (usually abi + module name + parameters)
     * @param config The module configuration.
     * @param abi The abi of the module.
     * @throws ConfigurationException
     */
    private void setUpConfig(String name, String nameWithParam, String id, String fullId,
            IConfiguration config, IAbi abi)
        throws ConfigurationException {
        List<OptionDef> optionsToInject = new ArrayList<>();
        if (mModuleOptions.containsKey(name)) {
            optionsToInject.addAll(mModuleOptions.get(name));
        }
        if (nameWithParam != null && mModuleOptions.containsKey(nameWithParam)) {
            optionsToInject.addAll(mModuleOptions.get(nameWithParam));
        }
        if (mModuleOptions.containsKey(id)) {
            optionsToInject.addAll(mModuleOptions.get(id));
        }
        if (mModuleOptions.containsKey(fullId)) {
            optionsToInject.addAll(mModuleOptions.get(fullId));
        }
        config.injectOptionValues(optionsToInject);

        // Set target preparers
        for (IDeviceConfiguration holder : config.getDeviceConfig()) {
            for (ITargetPreparer preparer : holder.getTargetPreparers()) {
                String className = preparer.getClass().getName();
                if (mTestOrPreparerOptions.containsKey(className)) {
                    config.injectOptionValues(mTestOrPreparerOptions.get(className));
                }
                if (preparer instanceof IAbiReceiver) {
                    ((IAbiReceiver) preparer).setAbi(abi);
                }
            }
        }

        // Set IRemoteTests
        List<IRemoteTest> tests = config.getTests();
        for (IRemoteTest test : tests) {
            String className = test.getClass().getName();
            if (mTestOrPreparerOptions.containsKey(className)) {
                config.injectOptionValues(mTestOrPreparerOptions.get(className));
            }
            addFiltersToTest(test, abi, fullId, mIncludeFilters, mExcludeFilters);
            if (test instanceof IAbiReceiver) {
                ((IAbiReceiver) test).setAbi(abi);
            }
        }

        // add the abi and module name to the description
        config.getConfigurationDescription().setAbi(abi);
        config.getConfigurationDescription().setModuleName(name);

        config.validateOptions();
    }


    /** Whether or not the base configuration should be created for all abis or not. */
    private boolean shouldCreateMultiAbiForBase(List<IModuleParameter> params) {
        for (IModuleParameter param : params) {
            if (param instanceof NotMultiAbiHandler) {
                return false;
            }
        }
        return true;
    }
}
