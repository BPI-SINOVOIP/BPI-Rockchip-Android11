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
import android.os.Bundle;

/**
 * An extension of {@link android.host.test.composer.IterateBase} for device-side testing.
 */
public class Iterate<U> extends IterateBase<Bundle, U> {
    @Override
    protected int getIterationsArgument(Bundle args) {
        return Integer.parseInt(args.getString(getOptionName(), String.valueOf(mDefaultValue)));
    }

    @Override
    protected OrderOptions getOrdersArgument(Bundle args) {
        String orderStr = args.getString(ORDER_OPTION_NAME, ORDER_DEFAULT_VALUE.toString());
        try {
            return OrderOptions.valueOf(orderStr.toUpperCase());
        } catch (IllegalArgumentException e) {
            throw new
                IllegalArgumentException(
                        String.format("The supplied order option \"%s\" is not supported.",
                                orderStr));
        }
    }
}
