/*
 * Copyright (C) 2007 The Android Open Source Project
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

package android.signature.cts.tests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.Date;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;
import java.util.Spliterator;
import java.util.function.Consumer;
import java.util.function.Function;

import org.junit.Test;

/**
 * Test class for {@link android.signature.cts.ByteBufferLineSpliterator}.
 */
public abstract class LineSpliteratorTestBase {

    protected static final Function<String, Integer> CONVERTER = str -> {
        return Integer.parseInt(str.substring(4));
    };
    protected static final Consumer<Integer> FAIL_CONSUMER = str -> {
        fail();
    };

    protected abstract Spliterator<Integer> createSpliterator(String data,
            Function<String, Integer> converter);

    protected static Consumer<Integer> createVerifier(final int i) {
        return intValue -> { assertEquals(i, (int)intValue); };
    }

    protected static String getRepetitions(int c) {
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < c; i++) {
            builder.append("test" + i + "\n");
        }
        return builder.toString();
    }

    protected <T> void doTestConverter(Function<String, Spliterator<T>> createFn,
            Function<Integer, Consumer<T>> verifierFn) {
        Spliterator<T> spliterator = createFn.apply(getRepetitions(10));
        for (int i = 0; i < 10; i++) {
            assertTrue(spliterator.tryAdvance(verifierFn.apply(i)));
        }
        assertFalse(spliterator.tryAdvance(a -> { fail(); }));
    }

    @Test
    public void testConverter() {
        doTestConverter(str -> createSpliterator(str, CONVERTER), i -> createVerifier(i));
    }

    @Test
    public void testSplit() {
        final long seed = new Date().getTime();
        try {
            Random r = new Random(seed);
            List<Spliterator<Integer>> queue = new LinkedList<>();
            queue.add(createSpliterator(getRepetitions(1000), CONVERTER));

            // Limit the maximum number of consecutive splits, just to ensure the test will
            // always terminate.
            int splitsInSequence = 0;
            final int maxSplitsInSequence = 100;

            for (int i = 0; i < 1000; i++) {
                assertTrue(!queue.isEmpty());
                boolean doSplit = r.nextBoolean();
                if (doSplit && splitsInSequence < maxSplitsInSequence) {
                    Spliterator<Integer> split = queue.get(0).trySplit();
                    if (split != null) {
                        queue.add(1, split);
                    }
                    i--;
                    splitsInSequence++;
                } else {
                    splitsInSequence = 0;
                    if (!queue.get(0).tryAdvance(createVerifier(i))) {
                        queue.remove(0);
                        i--;
                    }
                }
            }
            while (!queue.isEmpty()) {
                Spliterator<Integer> first = queue.remove(0);
                assertFalse(first.tryAdvance(FAIL_CONSUMER));
            }
        } catch (Throwable t) {
            throw new RuntimeException("Error, seed=" + seed, t);
        }
    }
}
