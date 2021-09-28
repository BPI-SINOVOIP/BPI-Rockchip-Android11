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

package android.hdmicec.cts;

import static org.junit.Assume.assumeTrue;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Rule to check if the required device feature is available on device.
 */
public class RequiredFeatureRule implements TestRule {

    private final BaseHostJUnit4Test mTest;
    private final String mFeature;

    public RequiredFeatureRule(BaseHostJUnit4Test test, String feature) {
        mTest = test;
        mFeature = feature;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ITestDevice testDevice = mTest.getDevice();
                // Checks if the device is available.
                assumeTrue("Test device is not available", testDevice != null);
                // Checks if the requested feature is available on the device.
                assumeTrue(mFeature + " not present in DUT " + testDevice.getSerialNumber(),
                    testDevice.hasFeature(mFeature));
                base.evaluate();
            }
        };
    }
}

