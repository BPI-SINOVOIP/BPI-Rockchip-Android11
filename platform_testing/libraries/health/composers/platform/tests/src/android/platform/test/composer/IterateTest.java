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
package android.platform.test.composer;

import android.host.test.composer.IterateBase;
import android.host.test.composer.IterateTestBase;
import android.os.Bundle;

import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit test the logic for device-side {@link Iterate}
 */
@RunWith(JUnit4.class)
public class IterateTest extends IterateTestBase<Bundle> {
    protected class BundleArgumentsBuilder extends ArgumentsBuilder {
        @Override
        public Bundle build() {
            Bundle args = new Bundle();
            if (mIterations != null) {
                args.putString(IterateTestBase.ITERATIONS_OPTION_NAME, String.valueOf(mIterations));
            }
            if (mAlternateIterations != null) {
                args.putString(
                        IterateTestBase.ITERATIONS_OPTION_ALTERNATE_NAME,
                        String.valueOf(mAlternateIterations));
            }
            if (mOrder != null) {
                args.putString(IterateTestBase.ORDER_OPTION_NAME, String.valueOf(mOrder));
            }
            return args;
        }
    }

    @Override
    protected ArgumentsBuilder getArgumentsBuilder() {
        return new BundleArgumentsBuilder();
    }

    @Override
    protected IterateBase<Bundle, Integer> getIterate() {
        return new Iterate<Integer>();
    }
}
