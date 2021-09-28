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

import java.lang.AssertionError;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

/**
 * A {@link Compose} function base class for repeating objects a configurable number of times.
 */
public abstract class IterateBase<T, U> implements Compose<T, U> {
    protected static final String ITERATIONS_OPTION_NAME = "iterations";
    protected static final int ITERATIONS_DEFAULT_VALUE = 1;

    protected enum OrderOptions { CYCLIC, SEQUENTIAL };
    protected static final String ORDER_OPTION_NAME = "order";
    protected static final OrderOptions ORDER_DEFAULT_VALUE = OrderOptions.CYCLIC;

    protected final int mDefaultValue;
    protected String mOptionName = ITERATIONS_OPTION_NAME;

    public IterateBase() {
        this(ITERATIONS_DEFAULT_VALUE);
    }

    public IterateBase(int defaultIterations) {
        mDefaultValue = defaultIterations;
    }

    @Override
    public List<U> apply(T args, List<U> input) {
        int iterations = getIterationsArgument(args);
        OrderOptions order = getOrdersArgument(args);
        switch (order) {
            case CYCLIC:
                return Collections.nCopies(iterations, input)
                        .stream()
                        .flatMap(Collection::stream)
                        .collect(Collectors.toList());
            case SEQUENTIAL:
                return input.stream()
                        .map(u -> Collections.nCopies(iterations, u))
                        .flatMap(Collection::stream)
                        .collect(Collectors.toList());
        }
        // We should never get here as the switch statement should exhaust the order options.
        throw new AssertionError(
                String.format("Order option \"%s\" is not supported", order.toString()));
    }

    /** Returns the number of iterations to run from {@code args}. */
    protected abstract int getIterationsArgument(T args);

    /** Returns the order that the iteration should happen in from {@code args}. */
    protected abstract OrderOptions getOrdersArgument(T args);

    /** Returns the option name to supply values to this iterator. */
    protected String getOptionName() {
        return mOptionName;
    }

    /** Sets the option name for supply values to this iterator. */
    public void setOptionName(String name) {
        mOptionName = name;
    }
}
