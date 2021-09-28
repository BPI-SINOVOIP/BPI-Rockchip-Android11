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
package com.android.tradefed.testtype.junit4;

import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IAbiReceiver;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.ISetOptionReceiver;
import com.android.tradefed.testtype.ITestInformationReceiver;

import org.junit.runners.Parameterized;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.List;

import junitparams.JUnitParamsRunner;

/**
 * JUnit4 style parameterized runner for host-side driven parameterized tests.
 *
 * <p>This runner is based on {@link JUnitParamsRunner} and not JUnit4 native {@link Parameterized}
 * but the native parameterized runner is not really good and doesn't allow to run a single method.
 *
 * @see JUnitParamsRunner
 */
public class DeviceParameterizedRunner extends JUnitParamsRunner
        implements IAbiReceiver, ISetOptionReceiver, ITestInformationReceiver {

    @Option(name = HostTest.SET_OPTION_NAME, description = HostTest.SET_OPTION_DESC)
    private List<String> mKeyValueOptions = new ArrayList<>();

    private TestInformation mTestInformation;
    private IAbi mAbi;

    /**
     * @param klass
     * @throws InitializationError
     */
    public DeviceParameterizedRunner(Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected Statement methodInvoker(FrameworkMethod method, Object testObj) {
        if (testObj instanceof IDeviceTest) {
            ((IDeviceTest) testObj).setDevice(mTestInformation.getDevice());
        }
        if (testObj instanceof IBuildReceiver) {
            ((IBuildReceiver) testObj).setBuild(mTestInformation.getBuildInfo());
        }
        // We are more flexible about abi information since not always available.
        if (testObj instanceof IAbiReceiver) {
            ((IAbiReceiver) testObj).setAbi(mAbi);
        }
        if (testObj instanceof IInvocationContextReceiver) {
            ((IInvocationContextReceiver) testObj)
                    .setInvocationContext(mTestInformation.getContext());
        }
        if (testObj instanceof ITestInformationReceiver) {
            ((ITestInformationReceiver) testObj).setTestInformation(mTestInformation);
        }
        // Set options of test object
        HostTest.setOptionToLoadedObject(testObj, mKeyValueOptions);
        return super.methodInvoker(method, testObj);
    }

    @Override
    public void setAbi(IAbi abi) {
        mAbi = abi;
    }

    @Override
    public IAbi getAbi() {
        return mAbi;
    }

    @Override
    public void setTestInformation(TestInformation testInformation) {
        mTestInformation = testInformation;
    }

    @Override
    public TestInformation getTestInformation() {
        return mTestInformation;
    }
}
