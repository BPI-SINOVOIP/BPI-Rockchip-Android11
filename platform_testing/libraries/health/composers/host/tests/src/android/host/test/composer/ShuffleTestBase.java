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

import static com.google.common.truth.Truth.assertThat;

import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import org.junit.Test;

/**
 * Unit test the logic for {@link Shuffle}
 */
public abstract class ShuffleTestBase<T> {
    protected static final String SHUFFLE_OPTION_NAME = "shuffle";
    protected static final String SEED_OPTION_NAME = "seed";

    /**
     * Unit test that shuffling with a specific seed is respected.
     */
    @Test
    public void testShuffleSeedRespected()  {
        long seedValue = new Random().nextLong();
        // Construct the input list of integers and apply the shuffle function.
        List<Integer> input = IntStream.range(1, 10).boxed().collect(Collectors.toList());
        List<Integer> output = getShuffle().apply(getArguments(true, seedValue), input);
        // Shuffle locally against the same seed and compare results.
        Collections.shuffle(input, new Random(seedValue));
        assertThat(input).isEqualTo(output);
    }

    protected abstract ShuffleBase<T, Integer> getShuffle();

    protected abstract T getArguments(boolean shuffle, long seed);
}
