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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.signature.cts.ByteBufferLineSpliterator;

import java.nio.charset.Charset;
import java.util.Spliterator;
import java.util.function.Function;

import org.junit.Test;
import org.junit.runners.JUnit4;
import org.junit.runner.RunWith;

/**
 * Test class for {@link android.signature.cts.ByteBufferLineSpliterator}.
 */
@RunWith(JUnit4.class)
public class ByteBufferLineSpliteratorTest extends LineSpliteratorTestBase {

    @Test
    public void testSplitMostlyMiddle() {
        Spliterator<Integer> split1 = createSpliterator(getRepetitions(100),
                CONVERTER);
        Spliterator<Integer> split2 = split1.trySplit();
        assertNotNull(split2);
        int c1 = 0, c2 = 0;
        while (split1.tryAdvance(intValue -> {
        })) {
            c1++;
        }
        while (split2.tryAdvance(intValue -> {
        })) {
            c2++;
        }
        assertEquals(c1 + " + " + c2 + " != 100", 100, c1 + c2);
        assertTrue("c1=" + c1 + ", c2=" + c2, Math.abs(c1 - c2) < 10);
    }

    @Override
    protected Spliterator<Integer> createSpliterator(String data,
            Function<String, Integer> converter) {
        return new ByteBufferLineSpliterator<Integer>(Charset.defaultCharset().encode(data), 6,
                converter);
    }
}
