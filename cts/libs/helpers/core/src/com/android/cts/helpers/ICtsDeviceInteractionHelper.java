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

package com.android.cts.helpers;

import android.platform.helpers.ITestHelper;

/**
 * An interface for device interaction helper classes that will be loaded into CTS tests at runtime
 * by {@link HelperManager}. The @Rule ({@link DeviceInteractionHelperRule}) that test classes use
 * to manage {@link HelperManager} enforces that concrete implementation classes must inherit from
 * {@link ICtsDeviceInteractionHelper}, which ensures that helper classes are not loaded unless they
 * were intended to be part of a test.
 *
 * <p>Because every CTS module needs a different set of abstractions, this interface imposes minimal
 * structure. Each CTS module should define its own specific interface that extends {@link
 * ICtsDeviceInteractionHelper} with whatever abstractions make sense for its particular device
 * interactions.
 */
public interface ICtsDeviceInteractionHelper extends ITestHelper {
    /**
     * Perform any operations needed before the test runs. Should be called from the test's {@code
     * setUp} method or another {@code @Before} method.
     */
    default void setUp() {};

    /**
     * Perform any operations needed after the test runs. Should be called from the test's {@code
     * tearDown} method or another {@code @After} method.
     */
    default void tearDown() {};
}
