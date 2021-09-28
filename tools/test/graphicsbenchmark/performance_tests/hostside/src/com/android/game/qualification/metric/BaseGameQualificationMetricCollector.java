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

package com.android.game.qualification.metric;

import com.android.annotations.Nullable;
import com.android.game.qualification.ApkInfo;
import com.android.game.qualification.CertificationRequirements;
import com.android.game.qualification.proto.ResultDataProto;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.metric.BaseDeviceMetricCollector;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement;
import com.android.tradefed.result.TestDescription;

import java.util.Map;

public abstract class BaseGameQualificationMetricCollector extends BaseDeviceMetricCollector {
    @Nullable
    private ApkInfo mTestApk;
    @Nullable
    protected CertificationRequirements mCertificationRequirements;
    protected ResultDataProto.Result mDeviceResultData;
    protected ITestDevice mDevice;
    private boolean mEnabled;
    private boolean mHasError;
    private String mErrorMessage = "";

    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Nullable
    protected ApkInfo getApkInfo() {
        return mTestApk;
    }

    public void setApkInfo(ApkInfo apk) {
        mTestApk = apk;
    }

    public void setCertificationRequirements(@Nullable CertificationRequirements requirements) {
        mCertificationRequirements = requirements;
    }

    public void setDeviceResultData(ResultDataProto.Result resultData) {
        mDeviceResultData = resultData;
    }

    public boolean hasError() {
        return mHasError;
    }

    protected void setHasError(boolean hasError) {
        mHasError = hasError;
    }

    public String getErrorMessage() {
        return mErrorMessage;
    }

    protected void setErrorMessage(String msg) {
        mErrorMessage = msg;
    }

    public boolean isEnabled() {
        return mEnabled;
    }

    public void enable() {
        mEnabled = true;
    }

    public void disable() {
        mEnabled = false;
    }

    // If an exception is thrown, hasError() must return true in order for the host controller to
    // recognize an error has occurred.  Make onTestStart and onTestEnd final to ensure child
    // classes do not forget to call setHasError(true).  Child classes should override onStart and
    // onEnd instead.

    @Override
    public final void onTestStart(DeviceMetricData testData) {
        super.onTestStart(testData);
        try {
            onStart(testData);
        } catch (Exception e) {
            setHasError(true);
            if (getErrorMessage().isEmpty()) {
                setErrorMessage(e.getMessage());
            }
            throw e;
        }
    }

    @Override
    public final void onTestEnd(
            DeviceMetricData testData,
            Map<String, MetricMeasurement.Metric> currentTestCaseMetrics) {
        super.onTestEnd(testData, currentTestCaseMetrics);
        try {
            onEnd(testData, currentTestCaseMetrics);
        } catch (Exception e) {
            setHasError(true);
            if (getErrorMessage().isEmpty()) {
                setErrorMessage(e.getMessage());
            }
            throw e;
        }
    }

    protected void onStart(DeviceMetricData testData) {
        // Do nothing.
    }

    protected void onEnd(
            DeviceMetricData testData,
            Map<String, MetricMeasurement.Metric> currentTestCaseMetrics) {
        // Do nothing.
    }

}
