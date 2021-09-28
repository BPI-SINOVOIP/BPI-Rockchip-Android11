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

import java.util.Arrays;
import java.util.List;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

/**
 * Rule class that allows for checking device property value against required values.
 * Static functions in this class can be used to check properties as a list or single value.
 */
public class RequiredPropertyRule implements TestRule {

    // Do not allow instantiation.
    private RequiredPropertyRule(){}

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                base.evaluate();
            }
        };
    }

    private static String getDevicePropertyValue(BaseHostJUnit4Test test, String propertyName)
        throws Throwable {
        ITestDevice testDevice = test.getDevice();
        // Checks if the device is available.
        assumeTrue("Test device is not available", testDevice != null);
        return testDevice.executeShellCommand("getprop " + propertyName).trim();

    }

    public static RequiredPropertyRule isEqualTo(final BaseHostJUnit4Test test,
        final String propertyName, final String propertyValue) {
        return new RequiredPropertyRule() {
            @Override
            public Statement apply(final Statement base, final Description description) {
                return new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        String deviceProperty = getDevicePropertyValue(test, propertyName);
                        assumeTrue("Required property " + propertyName + " = " + propertyValue
                                + " is not present in " + deviceProperty
                                + " of device " + test.getDevice().getSerialNumber(),
                            deviceProperty.equals(propertyValue));
                        base.evaluate();
                    }
                };
            }
        };
    }

    public static RequiredPropertyRule asCsvContainsValue(final BaseHostJUnit4Test test,
        final String propertyName, final String propertyValue) {
        return new RequiredPropertyRule() {
            @Override
            public Statement apply(final Statement base, final Description description) {
                return new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        List<String> deviceProperties =
                            Arrays.asList(getDevicePropertyValue(test, propertyName)
                                .replaceAll("\\s+", "")
                                .split(","));
                        assumeTrue(
                            "Required property " + propertyName + " = " + propertyValue
                                + " is not present in " + deviceProperties.toString()
                                + " of device " + test.getDevice().getSerialNumber(),
                            deviceProperties.contains(propertyValue));
                        base.evaluate();
                    }
                };
            }
        };
    }
}


