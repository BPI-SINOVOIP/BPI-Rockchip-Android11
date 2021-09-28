/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.compatibility.common.tradefed.targetprep;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.util.DynamicConfigFileReader;
import com.android.compatibility.common.util.BusinessLogic;
import com.android.compatibility.common.util.BusinessLogicFactory;
import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.PropertyUtil;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.suite.TestSuiteInfo;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.net.HttpHelper;
import com.android.tradefed.util.net.IHttpHelper;

import com.google.api.client.auth.oauth2.Credential;
import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;

import org.json.JSONException;
import org.json.JSONObject;
import org.xmlpull.v1.XmlPullParserException;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Pushes business Logic to the host and the test device, for use by test cases in the test suite.
 */
@OptionClass(alias = "business-logic-preparer")
public class BusinessLogicPreparer extends BaseTargetPreparer
        implements IAbiReceiver, IInvocationContextReceiver {

    /* Placeholder in the service URL for the suite to be configured */
    private static final String SUITE_PLACEHOLDER = "{suite-name}";

    /* String for the key to get file from GlobalConfiguration */
    private static final String GLOBAL_APE_API_KEY = "ape-api-key";

    /* String for creating files to store the business logic configuration on the host */
    private static final String FILE_LOCATION = "business-logic";
    /* String for creating cached business logic configuration files */
    private static final String BL_CACHE_FILE = "business-logic-cache";
    /* Number of days for which cached business logic is valid */
    private static final int BL_CACHE_DAYS = 5;
    /* BL_CACHE_DAYS converted to millis */
    private static final long BL_CACHE_MILLIS = BL_CACHE_DAYS * 1000 * 60 * 60 * 24L;
    /* Extension of business logic files */
    private static final String FILE_EXT = ".bl";
    /* Default amount of time to attempt connection to the business logic service, in seconds */
    private static final int DEFAULT_CONNECTION_TIME = 60;
    /* Time to wait between connection attempts to the business logic service, in millis */
    private static final long SLEEP_BETWEEN_CONNECTIONS_MS = 5000; // 5 seconds
    /* Dynamic config constants */
    private static final String DYNAMIC_CONFIG_FEATURES_KEY = "business_logic_device_features";
    private static final String DYNAMIC_CONFIG_PROPERTIES_KEY = "business_logic_device_properties";
    private static final String DYNAMIC_CONFIG_PACKAGES_KEY = "business_logic_device_packages";
    private static final String DYNAMIC_CONFIG_EXTENDED_DEVICE_INFO_KEY =
            "business_logic_extended_device_info";

    @Option(name = "business-logic-url", description = "The URL to use when accessing the " +
            "business logic service, parameters not included", mandatory = true)
    private String mUrl;

    @Option(name = "business-logic-api-key", description = "The API key to use when accessing " +
            "the business logic service.", mandatory = true)
    private String mApiKey;

    @Option(name = "business-logic-api-scope", description = "The URI of api scope to use when " +
            "retrieving business logic rules.")
    /* URI of api scope to use when retrieving business logic rules */
    private  String mApiScope;

    @Option(name = "cache-business-logic", description = "Whether to keep and use cached " +
            "business logic files.")
    private boolean mCache = false;

    @Option(name = "clean-cache-business-logic", description = "Like option " +
            "'cache-business-logic', but forces a refresh of the cached business logic file")
    private boolean mCleanCache = false;

    @Option(name = "ignore-business-logic-failure", description = "Whether to proceed with the " +
            "suite invocation if retrieval of business logic fails.")
    private boolean mIgnoreFailure = false;

    @Option(name = "business-logic-connection-time", description = "Amount of time to attempt " +
            "connection to the business logic service, in seconds.")
    private int mMaxConnectionTime = DEFAULT_CONNECTION_TIME;

    @Option(name = "config-filename", description = "The module name for module-level " +
            "configurations, or the suite name for suite-level configurations. Will lookup " +
            "suite name if not provided.")
    private String mModuleName = null;

    @Option(name = "version", description = "The module configuration version to retrieve.")
    private String mModuleVersion = null;

    private String mDeviceFilePushed;
    private String mHostFilePushed;
    private IAbi mAbi = null;
    private IInvocationContext mModuleContext = null;

    /** {@inheritDoc} */
    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    /** {@inheritDoc} */
    @Override
    public IAbi getAbi() {
        return mAbi;
    }


    /** {@inheritDoc} */
    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        mModuleContext = invocationContext;
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        IBuildInfo buildInfo = testInfo.getBuildInfo();
        ITestDevice device = testInfo.getDevice();
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(buildInfo);
        if (buildHelper.hasBusinessLogicHostFile()) {
            CLog.i("Business logic file already collected, skipping BusinessLogicPreparer.");
            return;
        }
        // Ensure mModuleName is set.
        if (mModuleName == null) {
            mModuleName = "";
            CLog.w("Option config-filename isn't set. Using empty string instead.");
        }
        if (mModuleVersion == null) {
            CLog.w("Option version isn't set. Using 'null' instead.");
            mModuleVersion = "null";
        }
        String requestParams = buildRequestParams(device, buildInfo);
        String baseUrl = mUrl.replace(SUITE_PLACEHOLDER, getSuiteName());
        String businessLogicString = null;
        // use cached business logic string if options are set accordingly and cache is valid,
        // otherwise proceed with remote download.
        if (!shouldReadCache()
                || (businessLogicString = readFromCache(baseUrl, requestParams)) == null) {
            CLog.i("Attempting to connect to business logic service...");
        }
        long start = System.currentTimeMillis();
        while (businessLogicString == null
                && System.currentTimeMillis() < (start + (mMaxConnectionTime * 1000))) {
            try {
                businessLogicString = doPost(baseUrl, requestParams);
            } catch (IOException e) {
                // ignore, re-attempt connection with remaining time
                CLog.d("BusinessLogic connection failure message: %s\nRetrying...", e.getMessage());
                RunUtil.getDefault().sleep(SLEEP_BETWEEN_CONNECTIONS_MS);
            }
        }
        if (businessLogicString == null) {
            if (mIgnoreFailure) {
                CLog.e("Failed to connect to business logic service.\nProceeding with test "
                        + "invocation, tests depending on the remote configuration will fail.\n");
                return;
            } else {
                throw new TargetSetupError(String.format("Cannot connect to business logic "
                        + "service for config %s.\nIf this problem persists, re-invoking with "
                        + "option '--ignore-business-logic-failure' will cause tests to execute "
                        + "anyways (though tests depending on the remote configuration will fail).",
                        mModuleName), device.getDeviceDescriptor());
            }
        }

        if (shouldWriteCache()) {
            writeToCache(businessLogicString, baseUrl, requestParams, mCleanCache);
        }
        // Push business logic string to host file
        try {
            File hostFile = FileUtil.createTempFile(FILE_LOCATION, FILE_EXT);
            FileUtil.writeToFile(businessLogicString, hostFile);
            mHostFilePushed = hostFile.getAbsolutePath();
            // Ensure bitness is set.
            String bitness = (mAbi != null) ? mAbi.getBitness() : "";
            buildHelper.setBusinessLogicHostFile(hostFile, bitness + mModuleName);
        } catch (IOException e) {
            throw new TargetSetupError(String.format(
                    "Retrieved business logic for config %s could not be written to host",
                    mModuleName), device.getDeviceDescriptor());
        }
        // Push business logic string to device file
        removeDeviceFile(device); // remove any existing business logic file from device
        if (device.pushString(businessLogicString, BusinessLogic.DEVICE_FILE)) {
            mDeviceFilePushed = BusinessLogic.DEVICE_FILE;
        } else {
            throw new TargetSetupError(String.format(
                    "Retrieved business logic for config %s could not be written to device %s",
                    mModuleName, device.getSerialNumber()), device.getDeviceDescriptor());
        }
    }

    /** Helper to populate the business logic service request with info about the device. */
    @VisibleForTesting
    String buildRequestParams(ITestDevice device, IBuildInfo buildInfo)
            throws DeviceNotAvailableException {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(buildInfo);
        MultiMap<String, String> paramMap = new MultiMap<>();
        String suiteVersion = buildHelper.getSuiteVersion();
        if (suiteVersion == null) {
            suiteVersion = "null";
        }
        paramMap.put("suite_version", suiteVersion);
        paramMap.put("module_version", mModuleVersion);
        paramMap.put("oem", String.valueOf(PropertyUtil.getManufacturer(device)));
        for (String feature : getBusinessLogicFeatures(device, buildInfo)) {
            paramMap.put("features", feature);
        }
        for (String property : getBusinessLogicProperties(device, buildInfo)) {
            paramMap.put("properties", property);
        }
        for (String pkg : getBusinessLogicPackages(device, buildInfo)) {
            paramMap.put("packages", pkg);
        }
        for (String deviceInfo : getExtendedDeviceInfo(buildInfo)) {
            paramMap.put("device_info", deviceInfo);
        }
        IHttpHelper helper = new HttpHelper();
        String paramString = helper.buildParameters(paramMap);
        CLog.d("Built param string: \"%s\"", paramString);
        return paramString;
    }

    /**
     * Return the the first element of test-suite-tag from configuration if it's not empty,
     * otherwise, return the name from test-suite-info.properties.
     */
    @VisibleForTesting
    String getSuiteName() {
        String suiteName = null;
        if (mModuleContext == null) {
            suiteName = TestSuiteInfo.getInstance().getName().toLowerCase();
        } else {
            List<String> testSuiteTags = mModuleContext.getConfigurationDescriptor().
                    getSuiteTags();
            if (!testSuiteTags.isEmpty()) {
                if (testSuiteTags.size() >= 2) {
                    CLog.i("More than 2 test-suite-tag are defined. test-suite-tag: " +
                        testSuiteTags);
                }
                suiteName = testSuiteTags.get(0).toLowerCase();
                CLog.i("Using %s from test suite tags to get value from dynamic config", suiteName);
            } else {
                suiteName = TestSuiteInfo.getInstance().getName().toLowerCase();
                CLog.i("Using %s from TestSuiteInfo to get value from dynamic config", suiteName);
            }
        }
        return suiteName;
    }

    /* Get device properties list, with element format "<property_name>:<property_value>" */
    private List<String> getBusinessLogicProperties(ITestDevice device, IBuildInfo buildInfo)
            throws DeviceNotAvailableException {
        List<String> properties = new ArrayList<>();
        Map<String, String> clientIds = PropertyUtil.getClientIds(device);
        for (Map.Entry<String, String> id : clientIds.entrySet()) {
            // add client IDs to the list of properties
            properties.add(String.format("%s:%s", id.getKey(), id.getValue()));
        }

        try {
            List<String> propertyNames = DynamicConfigFileReader.getValuesFromConfig(buildInfo,
                    getSuiteName(), DYNAMIC_CONFIG_PROPERTIES_KEY);
            for (String name : propertyNames) {
                // Use String.valueOf in case property is undefined for the device ("null")
                String value = String.valueOf(device.getProperty(name));
                properties.add(String.format("%s:%s", name, value));
            }
        } catch (XmlPullParserException | IOException e) {
            CLog.e("Failed to pull business logic properties from dynamic config");
        }
        return properties;
    }

    /* Get device features list */
    private List<String> getBusinessLogicFeatures(ITestDevice device, IBuildInfo buildInfo)
            throws DeviceNotAvailableException {
        try {
            List<String> dynamicConfigFeatures = DynamicConfigFileReader.getValuesFromConfig(
                    buildInfo, getSuiteName(), DYNAMIC_CONFIG_FEATURES_KEY);
            Set<String> deviceFeatures = FeatureUtil.getAllFeatures(device);
            dynamicConfigFeatures.retainAll(deviceFeatures);
            return dynamicConfigFeatures;
        } catch (XmlPullParserException | IOException e) {
            CLog.e("Failed to pull business logic features from dynamic config");
            return new ArrayList<>();
        }
    }

    /* Get device packages list */
    private List<String> getBusinessLogicPackages(ITestDevice device, IBuildInfo buildInfo)
            throws DeviceNotAvailableException {
        try {
            List<String> dynamicConfigPackages = DynamicConfigFileReader.getValuesFromConfig(
                    buildInfo, getSuiteName(), DYNAMIC_CONFIG_PACKAGES_KEY);
            Set<String> devicePackages = device.getInstalledPackageNames();
            dynamicConfigPackages.retainAll(devicePackages);
            return dynamicConfigPackages;
        } catch (XmlPullParserException | IOException e) {
            CLog.e("Failed to pull business logic packages from dynamic config");
            return new ArrayList<>();
        }
    }

    /* Get extended device info*/
    private List<String> getExtendedDeviceInfo(IBuildInfo buildInfo) {
        List<String> extendedDeviceInfo = new ArrayList<>();
        File deviceInfoPath = buildInfo.getFile(DeviceInfoCollector.DEVICE_INFO_DIR);
        if (deviceInfoPath == null || !deviceInfoPath.exists()) {
            CLog.w("Device Info directory was not created (Make sure you are not running plan " +
                    "\"*ts-dev\" or including option -d/--skip-device-info)");
            return extendedDeviceInfo;
        }
        List<String> requiredDeviceInfo = null;
        try {
            requiredDeviceInfo = DynamicConfigFileReader.getValuesFromConfig(
                buildInfo, getSuiteName(), DYNAMIC_CONFIG_EXTENDED_DEVICE_INFO_KEY);
        } catch (XmlPullParserException | IOException e) {
            CLog.e("Failed to pull business logic Extended DeviceInfo from dynamic config. "
                + "Error: %s", e);
            return extendedDeviceInfo;
        }
        File ediFile = null;
        String[] fileAndKey = null;
        try{
            for (String ediEntry: requiredDeviceInfo) {
                fileAndKey = ediEntry.split(":");
                if (fileAndKey.length <= 1) {
                    CLog.e("Dynamic config Extended DeviceInfo key has problem.");
                    return new ArrayList<>();
                }
                ediFile = FileUtil
                    .findFile(deviceInfoPath, fileAndKey[0] + ".deviceinfo.json");
                if (ediFile == null) {
                    CLog.e(
                            "Could not find Extended DeviceInfo JSON file: %s.",
                            deviceInfoPath + fileAndKey[0] + ".deviceinfo.json");
                    return new ArrayList<>();
                }
                String jsonString = FileUtil.readStringFromFile(ediFile);
                JSONObject jsonObj = new JSONObject(jsonString);
                String value = jsonObj.getString(fileAndKey[1]);
                extendedDeviceInfo
                    .add(String.format("%s:%s:%s", fileAndKey[0], fileAndKey[1], value));
            }
        }catch(JSONException | IOException | RuntimeException e){
            CLog.e(
                    "Failed to read or parse Extended DeviceInfo JSON file: %s. Error: %s",
                    deviceInfoPath + fileAndKey[0] + ".deviceinfo.json", e);
            return new ArrayList<>();
        }
        return extendedDeviceInfo;
    }

    private boolean shouldReadCache() {
        return mCache && !mCleanCache;
    }

    private boolean shouldWriteCache() {
        return mCache || mCleanCache;
    }

    /**
     * Read the string from the business logic cache, handling the following cases with a null
     * return value:
     * - The cached file does not exist
     * - The cached file cannot be read
     * - The cached file is timestamped more than BL_CACHE_DAYS prior to now
     * In the last two cases, the file is deleted so an up-to-date configuration may be cached anew
     */
    private static synchronized String readFromCache(String baseUrl, String params) {
        // baseUrl + params hashCode makes file unique, in case host runs invocations for different
        // device builds and/or test suites using business logic
        File cachedFile = getCachedFile(baseUrl, params);
        if (!cachedFile.exists()) {
            CLog.i("No cached business logic found");
            return null;
        }
        try {
            BusinessLogic cachedLogic = BusinessLogicFactory.createFromFile(cachedFile);
            Date cachedDate = cachedLogic.getTimestamp();
            if (System.currentTimeMillis() - cachedDate.getTime() < BL_CACHE_MILLIS) {
                CLog.i("Using cached business logic from: %s", cachedDate.toString());
                return FileUtil.readStringFromFile(cachedFile);
            } else {
                CLog.i("Cached business logic out-of-date, deleting cached file");
                FileUtil.deleteFile(cachedFile);
            }
        } catch (IOException e) {
            CLog.w("Failed to read cached business logic, deleting cached file");
            FileUtil.deleteFile(cachedFile);
        }
        return null;
    }

    /**
     * Write a string retrieved from the business logic service to the cache file, only if the
     * file does not already exist. Synchronize this method to prevent concurrent writes in the
     * sharding case.
     * @param blString the string to cache
     * @param baseUrl the base business logic request url containing suite info
     * @param params the string of params for the business logic request containing device info
     */
    private static synchronized void writeToCache(String blString, String baseUrl, String params,
            boolean overwrite) {
        // baseUrl + params hashCode makes file unique, in case host runs invocations for different
        // device builds and/or test suites using business logic
        File cachedFile = getCachedFile(baseUrl, params);
        if (!cachedFile.exists() || overwrite) {
            // don't overwrite existing file, whether from previous shard or previous invocation
            try {
                FileUtil.writeToFile(blString, cachedFile);
            } catch (IOException e) {
                throw new RuntimeException("Failed to write business logic to cache file", e);
            }
        }
    }

    /**
     * Get the cached business logic file given the base url and params used to retrieve this logic.
     */
    private static File getCachedFile(String baseUrl, String params) {
        int hashCode = (baseUrl + params).hashCode();
        return new File(System.getProperty("java.io.tmpdir"), BL_CACHE_FILE + hashCode);
    }

    private String doPost(String baseUrl, String params) throws IOException {
        String accessToken = getToken();
        if (Strings.isNullOrEmpty(accessToken)) {
            // Set API key on base URL
            baseUrl += String.format("?key=%s", mApiKey);
        }
        URL url = new URL(baseUrl);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");
        conn.setRequestProperty("User-Agent", "BusinessLogicClient");
        if (!Strings.isNullOrEmpty(accessToken)) {
            // Set authorization access token in POST header
            conn.setRequestProperty("Authorization", String.format("Bearer %s", accessToken));
        }
        // Send params in POST request body
        conn.setDoOutput(true);
        try (DataOutputStream wr = new DataOutputStream(conn.getOutputStream())) {
            wr.writeBytes(params);
        }
        int responseCode = conn.getResponseCode();
        CLog.d("Business Logic Service Response Code : %s", responseCode);
        return StreamUtil.getStringFromStream(conn.getInputStream());
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        // Clean up existing host and device files unconditionally
        if (mHostFilePushed != null) {
            FileUtil.deleteFile(new File(mHostFilePushed));
        }
        if (mDeviceFilePushed != null && !(e instanceof DeviceNotAvailableException)) {
            removeDeviceFile(testInfo.getDevice());
        }
    }

    /** Remove business logic file from the device */
    private static void removeDeviceFile(ITestDevice device) throws DeviceNotAvailableException {
        device.deleteFile(BusinessLogic.DEVICE_FILE);
    }

    /**
     * Returns an OAuth2 token string obtained using a service account json key file.
     *
     * Uses the service account key file location stored in environment variable 'APE_API_KEY'
     * to request an OAuth2 token. If APE_API_KEY wasn't set, try to get if file is dynamically
     * downloaded from GlobalConfiguration.
     */
    private String getToken() {
        String keyFilePath = System.getenv("APE_API_KEY");
        if (Strings.isNullOrEmpty(keyFilePath)) {
            File globalKeyFile = GlobalConfiguration.getInstance().getHostOptions().
                getServiceAccountJsonKeyFiles().get(GLOBAL_APE_API_KEY);
            if (globalKeyFile == null || !globalKeyFile.exists()) {
                CLog.d("Unable to fetch the service key because neither environment variable " +
                        "APE_API_KEY is set nor the key file is dynamically downloaded.");
                return null;
            }
            keyFilePath = globalKeyFile.getAbsolutePath();
        }
        if (Strings.isNullOrEmpty(mApiScope)) {
            CLog.d("API scope not set, use flag --business-logic-api-scope.");
            return null;
        }
        try {
            Credential credential = GoogleCredential.fromStream(new FileInputStream(keyFilePath))
                    .createScoped(Collections.singleton(mApiScope));
            credential.refreshToken();
            return credential.getAccessToken();
        } catch (FileNotFoundException e) {
            CLog.e(String.format("Service key file %s doesn't exist.", keyFilePath));
        } catch (IOException e) {
            CLog.e(String.format("Can't read the service key file, %s", keyFilePath));
        }
        return null;
    }
}
