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

import android.signature.cts.BufferedReaderLineSpliterator;

import java.io.BufferedReader;
import java.io.StringReader;
import java.util.Spliterator;
import java.util.function.Function;

import org.junit.Test;
import org.junit.runners.JUnit4;
import org.junit.runner.RunWith;

/**
 * Test class for {@link android.signature.cts.ByteBufferLineSpliterator}.
 */
@RunWith(JUnit4.class)
public class BufferedReaderLineSpliteratorTest extends LineSpliteratorTestBase {

    @Test
    public void testBiConverter() {
        doTestConverter(str -> new BufferedReaderLineSpliterator<IntPair>(
                new BufferedReader(new StringReader(str)), (strIn, lineNum) -> {
                    return new IntPair(CONVERTER.apply(strIn), lineNum);
                }), i -> {
                    return intPair -> {
                        assertEquals((int)i, intPair.mP1);
                        assertEquals((int)i + 1, intPair.mP2);
                    };
                });
    }

    static class IntPair {
        public int mP1;
        public int mP2;
        public IntPair(int p1, int p2) {
            mP1 = p1;
            mP2 = p2;
        }
    }

    @Override
    protected Spliterator<Integer> createSpliterator(String data,
            Function<String, Integer> converter) {
        return new BufferedReaderLineSpliterator<Integer>(
                new BufferedReader(new StringReader(data)), converter);
    }
}
