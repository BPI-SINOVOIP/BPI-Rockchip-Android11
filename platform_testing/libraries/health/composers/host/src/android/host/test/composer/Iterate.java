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
package android.host.test.composer;

import java.util.Map;

/**
 * An extension of {@link IterateBase} for host-side testing.
 */
public class Iterate<U> extends IterateBase<Map<String, String>, U> {
    @Override
    protected int getIterationsArgument(Map<String, String> args) {
        if (args.containsKey(getOptionName())) {
            String iterations = args.get(getOptionName());
            try {
                return Integer.parseInt(iterations);
            } catch (NumberFormatException e) {
                throw new RuntimeException(
                        String.format("Failed to parse iterations option: %s", iterations), e);
            }
        } else {
            return ITERATIONS_DEFAULT_VALUE;
        }
    }

    @Override
    protected OrderOptions getOrdersArgument(Map<String, String> args) {
        OrderOptions order = ORDER_DEFAULT_VALUE;
        if (args.containsKey(ORDER_OPTION_NAME)) {
            String orderStr = args.get(ORDER_OPTION_NAME);
            try {
                order = OrderOptions.valueOf(orderStr.toUpperCase());
            } catch (IllegalArgumentException e) {
                throw new
                    IllegalArgumentException(
                            String.format("The supplied order option \"%s\" is not supported.",
                                    orderStr));
            }
        }
        return order;
    }
}
