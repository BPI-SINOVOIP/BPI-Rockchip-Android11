/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.game.qualification.testtype;

import com.android.game.qualification.ApkInfo;
import com.android.game.qualification.CertificationRequirements;
import com.android.game.qualification.GameCoreConfiguration;
import com.android.game.qualification.GameCoreConfigurationXmlParser;
import com.android.game.qualification.metric.BaseGameQualificationMetricCollector;
import com.android.game.qualification.reporter.GameQualificationResultReporter;
import com.android.game.qualification.test.PerformanceTest;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.IMetricCollectorReceiver;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.testtype.ITestFilterReceiver;

import com.google.common.io.ByteStreams;

import org.junit.AssumptionViolatedException;
import org.xml.sax.SAXException;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.xml.parsers.ParserConfigurationException;

public class GameQualificationHostsideController implements
        IShardableTest,
        IDeviceTest,
        IMetricCollectorReceiver,
        IInvocationContextReceiver,
        IConfigurationReceiver,
        ITestFilterReceiver {
    // Package and class of the device side test.
    public static final String PACKAGE = "com.android.game.qualification.device";
    public static final String CLASS = PACKAGE + ".GameQualificationTest";

    private ITestDevice mDevice;
    private IConfiguration mConfiguration = null;
    private GameCoreConfiguration mGameCoreConfiguration = null;
    private List<ApkInfo> mApks = null;
    private File mApkInfoFile;
    private Collection<IMetricCollector> mCollectors;
    private GameQualificationResultReporter mResultReporter;
    private IInvocationContext mContext;
    private ArrayList<BaseGameQualificationMetricCollector> mAGQMetricCollectors;
    private Set<String> mIncludeFilters = new HashSet<>();
    private Set<String> mExcludeFilters = new HashSet<>();

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    @Option(name = "apk-info",
            description = "An XML file describing the list of APKs for qualifications.",
            importance = Option.Importance.ALWAYS)
    private String mApkInfoFileName;

    @Option(name = "apk-dir",
            description =
                    "Directory contains the APKs for qualifications.  If --apk-info is not "
                            + "specified and a file named 'apk-info.xml' exists in --apk-dir, that "
                            + "file will be used as the apk-info.",
            importance = Option.Importance.ALWAYS)
    private String mApkDir;

    private String getApkDir() {
        if (mApkDir == null) {
            String out = System.getenv("ANDROID_PRODUCT_OUT");
            if (out != null) {
                CLog.i("--apk-dir was not set, looking in ANDROID_PRODUCT_OUT(%s) for apk files.",
                        out);
                mApkDir = out + "/data/app";
            } else {
                CLog.i("--apk-dir and ANDROID_PRODUCT_OUT was not set, looking in current "
                        + "directory(%s) for apk files.",
                        new File(".").getAbsolutePath());
                mApkDir = ".";
            }
        }
        return mApkDir;
    }

    @Override
    public void addIncludeFilter(String filter) {
        mIncludeFilters.add(filter);
    }

    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        mIncludeFilters.addAll(filters);
    }

    @Override
    public void addExcludeFilter(String filter) {
        mExcludeFilters.add(filter);
    }

    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        mExcludeFilters.addAll(filters);
    }

    @Override
    public Set<String> getIncludeFilters() {
        return mIncludeFilters;
    }

    @Override
    public Set<String> getExcludeFilters() {
        return mExcludeFilters;
    }

    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    @Override
    public void setMetricCollectors(List<IMetricCollector> list) {
        mCollectors = list;
        mAGQMetricCollectors = new ArrayList<>();
        for (IMetricCollector collector : list) {
            if (collector instanceof BaseGameQualificationMetricCollector) {
                mAGQMetricCollectors.add((BaseGameQualificationMetricCollector) collector);
            }
        }
    }


    @Override
    public void setInvocationContext(IInvocationContext iInvocationContext) {
        mContext = iInvocationContext;
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    @Override
    public Collection<IRemoteTest> split(int shardCountHint) {
        initApkList();
        List<IRemoteTest> shards = new ArrayList<>();
        for(int i = 0; i < shardCountHint; i++) {
            if (i >= mApks.size()) {
                break;
            }
            List<ApkInfo> apkInfo = new ArrayList<>();
            for(int j = i; j < mApks.size(); j += shardCountHint) {
                apkInfo.add(mApks.get(j));
            }
            GameQualificationHostsideController shard = new GameQualificationHostsideController();
            shard.mApks = apkInfo;
            shard.mApkDir = getApkDir();
            shard.mApkInfoFileName = mApkInfoFileName;
            shard.mApkInfoFile = mApkInfoFile;

            shards.add(shard);
        }
        return shards;
    }

    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        // Configuration can null if trigger by TEST_MAPPING.
        if (mConfiguration == null) {
            return;
        }
        // Find result reporter
        if (mResultReporter == null) {
            for (ITestInvocationListener testListener
                    : mConfiguration.getTestInvocationListeners()) {
                if (testListener instanceof GameQualificationResultReporter) {
                    mResultReporter = (GameQualificationResultReporter) testListener;
                }
            }
        }

        assert !(mAGQMetricCollectors.isEmpty());
        for (IMetricCollector collector : mCollectors) {
            listener = collector.init(mContext, listener);
        }

        for (BaseGameQualificationMetricCollector collector : mAGQMetricCollectors) {
            collector.setDevice(getDevice());
        }

        HashMap<String, MetricMeasurement.Metric> runMetrics = new HashMap<>();

        initApkList();
        getDevice().pushFile(mApkInfoFile, ApkInfo.APK_LIST_LOCATION);

        long startTime = System.currentTimeMillis();
        listener.testRunStarted("gamequalification", mApks.size());

        for (ApkInfo apk : mApks) {
            PerformanceTest test =
                    new PerformanceTest(
                            getDevice(),
                            listener,
                            mAGQMetricCollectors,
                            apk,
                            getApkDir(),
                            mApkInfoFile.getParentFile());
            for (BaseGameQualificationMetricCollector collector : mAGQMetricCollectors) {
                collector.setApkInfo(apk);
                collector.setCertificationRequirements(
                        mGameCoreConfiguration.findCertificationRequirements(apk.getName()));
            }
            for (PerformanceTest.Test t : PerformanceTest.Test.values()) {
                TestDescription identifier = new TestDescription(CLASS, t.getName() + "[" + apk.getName() + "]");
                if (t.isEnableCollectors()) {
                    for (BaseGameQualificationMetricCollector collector : mAGQMetricCollectors) {
                        collector.enable();
                    }
                    if (mResultReporter != null) {
                        CertificationRequirements req =
                                mGameCoreConfiguration.findCertificationRequirements(apk.getName());
                        if (req != null) {
                            mResultReporter.putRequirements(identifier, req);
                        }
                    }
                }
                listener.testStarted(identifier);
                try {
                    t.getMethod().run(test);
                } catch(AssumptionViolatedException e) {
                    listener.testAssumptionFailure(identifier, e.getMessage());
                } catch (Error | Exception e) {
                    test.failed();
                    listener.testFailed(identifier, e.getMessage());
                }
                listener.testEnded(identifier, new HashMap<String, MetricMeasurement.Metric>());

                if (t.isEnableCollectors()) {
                    for (BaseGameQualificationMetricCollector collector : mAGQMetricCollectors) {
                        if (collector.hasError()) {
                            listener.testFailed(identifier, collector.getErrorMessage());
                        }
                        collector.disable();
                    }
                }
            }
        }
        listener.testRunEnded(System.currentTimeMillis() - startTime, runMetrics);
    }

    private void initApkList() {
        if (mApks != null) {
            return;
        }

        // Find an apk info file.  The priorities are:
        // 1. Use the specified apk-info if available.
        // 2. Use 'apk-info.xml' if there is one in the apk-dir directory.
        // 3. Use the default apk-info.xml in res.
        if (mApkInfoFileName != null) {
            mApkInfoFile = new File(mApkInfoFileName);
        } else {
            mApkInfoFile = new File(getApkDir(), "apk-info.xml");

            if (!mApkInfoFile.exists()) {
                String resource = "/com/android/game/qualification/apk-info.xml";
                try(InputStream inputStream = ApkInfo.class.getResourceAsStream(resource)) {
                    if (inputStream == null) {
                        throw new FileNotFoundException("Unable to find resource: " + resource);
                    }
                    mApkInfoFile = File.createTempFile("apk-info", ".xml");
                    try (OutputStream ostream = new FileOutputStream(mApkInfoFile)) {
                        ByteStreams.copy(inputStream, ostream);
                    }
                    mApkInfoFile.deleteOnExit();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        }
        GameCoreConfigurationXmlParser parser = new GameCoreConfigurationXmlParser();
        try {
            mGameCoreConfiguration = parser.parse(mApkInfoFile);
            mApks = mGameCoreConfiguration.getApkInfo();
        } catch (IOException | ParserConfigurationException | SAXException e) {
            throw new RuntimeException(e);
        }
    }
}
