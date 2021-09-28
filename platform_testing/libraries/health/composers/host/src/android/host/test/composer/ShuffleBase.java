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

import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * A {@link Compose} function base class for shuffling all objects with an optional seed.
 */
public abstract class ShuffleBase<T, U> implements Compose<T, U> {
    protected static final String SEED_OPTION_NAME = "seed";
    protected static final String SHUFFLE_OPTION_NAME = "shuffle";
    private static final boolean SHUFFLE_DEFAULT_VALUE = false;

    protected final boolean mShuffleDefaultValue;

    public ShuffleBase() {
        this(SHUFFLE_DEFAULT_VALUE);
    }

    public ShuffleBase(boolean shuffleDefaultValue) {
        mShuffleDefaultValue = shuffleDefaultValue;
    }

    @Override
    public List<U> apply(T args, List<U> input) {
        boolean shuffle = getShuffleArgument(args);
        if (shuffle) {
            long seed = getSeedArgument(args);
            Collections.shuffle(input, new Random(seed));
        }
        return input;
    }

    /** Returns if these tests are shuffled from {@code args}. */
    protected abstract boolean getShuffleArgument(T args);

    /** Returns the shuffle seed value from {@code args}. */
    protected abstract long getSeedArgument(T args);
}
