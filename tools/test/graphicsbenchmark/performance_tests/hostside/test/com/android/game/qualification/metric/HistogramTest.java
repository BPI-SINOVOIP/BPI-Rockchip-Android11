/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.metric;

import static org.junit.Assert.assertEquals;

import com.google.common.collect.ImmutableMap;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Test for {@link Histogram}.
 */
public class HistogramTest {

    @Test
    public void testSimple() throws IOException {
        List<Long> data = Arrays.asList(0L, 1L, 1L, 2L, 0L);
        Histogram histogram = new Histogram(data, 1L, null, null);

        Map<Long, Integer> counts = histogram.getCounts();
        assertEquals(
                ImmutableMap.of(
                        0L, 2,
                        1L, 2,
                        2L, 1,
                        3L, 0),
                counts);

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        histogram.plotAscii(out, 2);
        assertEquals(
                " 0| == (2 = 40.0%) {40.0%}\n"
                        + " 1| == (2 = 40.0%) {80.0%}\n"
                        + " 2| =  (1 = 20.0%) {100.0%}\n"
                        + " 3|    (0 = 0.0%) {100.0%}\n",
                out.toString());
    }

    @Test
    public void testCutoff() {
        List<Long> data = Arrays.asList(-2L, -1L, 0L, 1L, 2L);
        Histogram histogram = new Histogram(data, 1L,-1L, 1L);

        Map<Long, Integer> counts = histogram.getCounts();
        assertEquals(
                ImmutableMap.builder()
                        .put(Long.MIN_VALUE, 1)
                        .put(-1L, 1)
                        .put(0L, 1)
                        .put(1L, 1)
                        .put(Long.MAX_VALUE, 1).build(),
                counts);
    }

    @Test
    public void testPlotCutoff() throws IOException {
        List<Long> data = Arrays.asList(0L, 2L, 4L);
        Histogram histogram = new Histogram(data, 1L, 1L, 3L);

        Map<Long, Integer> counts = histogram.getCounts();
        assertEquals(1, (int)counts.get(Long.MIN_VALUE));
        assertEquals(1, (int)counts.get(2L));
        assertEquals(1, (int)counts.get(Long.MAX_VALUE));

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        histogram.plotAscii(out, 1);
        assertEquals(
                "<1| = (1 = 33.3%) {33.3%}\n"
                        + " 1|   (0 = 0.0%) {33.3%}\n"
                        + " 2| = (1 = 33.3%) {66.7%}\n"
                        + " 3|   (0 = 0.0%) {66.7%}\n"
                        + ">3| = (1 = 33.3%) {100.0%}\n",
                out.toString());
    }

    @Test
    public void testBuckets() {
        List<Long> data = Arrays.asList(0L, 1L, 2L, 3L, 4L);
        Histogram histogram = new Histogram(data, 3L, null, null);

        Map<Long, Integer> counts = histogram.getCounts();
        assertEquals(
                ImmutableMap.of(
                        -1L, 2,
                        2L, 3,
                        5L, 0),
                counts);
    }

    /**
     * With even bucket size, a data point can be on the exact bucket boundary.  In that case, the
     * data point will be categorized into the higher bucket.
     */
    @Test
    public void testEvenBucketSize() {
        List<Long> data = Arrays.asList(-2L, -1L, 0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L);
        Histogram histogram = new Histogram(data, 2L,-1L, 5L);

        Map<Long, Integer> counts = histogram.getCounts();
        assertEquals(
                ImmutableMap.builder()
                        .put(Long.MIN_VALUE, 1)
                        .put(-1L, 2)
                        .put(1L, 2)
                        .put(3L, 2)
                        .put(5L,1)
                        .put(Long.MAX_VALUE, 2).build(),
                counts);
    }

    @Test
    public void testPlot() throws IOException {
        List<Long> data = new ArrayList<>();
        for (long i = 1; i <= 10; i ++) {
            for (int j = 0; j < i; j ++) {
                data.add(i);
            }
        }
        Histogram histogram = new Histogram(data, 1L, 0L, 10L);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        histogram.plotAscii(out, 10);
        assertEquals(
                "  1| =          (1 = 1.8%) {1.8%}\n"
                        + "  2| ==         (2 = 3.6%) {5.5%}\n"
                        + "  3| ===        (3 = 5.5%) {10.9%}\n"
                        + "  4| ====       (4 = 7.3%) {18.2%}\n"
                        + "  5| =====      (5 = 9.1%) {27.3%}\n"
                        + "  6| ======     (6 = 10.9%) {38.2%}\n"
                        + "  7| =======    (7 = 12.7%) {50.9%}\n"
                        + "  8| ========   (8 = 14.5%) {65.5%}\n"
                        + "  9| =========  (9 = 16.4%) {81.8%}\n"
                        + " 10| ========== (10 = 18.2%) {100.0%}\n"
                        + ">10|            (0 = 0.0%) {100.0%}\n",
                out.toString());

    }
}