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
package com.android.tradefed.invoker.shard;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.shard.token.ITokenRequest;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IReportNotExecuted;

import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Set;

/** Test class that implements {@link ITokenRequest}. */
public class TokenTestClass implements IRemoteTest, ITokenRequest, IReportNotExecuted {

    @Override
    public Set<TokenProperty> getRequiredTokens() {
        Set<TokenProperty> props = new HashSet<>();
        props.add(TokenProperty.SIM_CARD);
        return props;
    }

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        listener.testRunStarted("TestToken", 1);
        TestDescription testId = new TestDescription("StubToken", "MethodToken");
        listener.testStarted(testId);
        listener.testEnded(testId, new LinkedHashMap<String, Metric>());
        listener.testRunEnded(500, new LinkedHashMap<String, Metric>());
    }

    @Override
    public void reportNotExecuted(ITestInvocationListener listener, String message) {
        listener.testFailed(new TestDescription("token.class", "token.test"), message);
    }
}
