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
//import static org.junit.Assert.assertThrows;

import java.util.List;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

import com.google.common.collect.Lists;
import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.Rule;
import org.junit.rules.ExpectedException;

/**
 * Base class to unit test the logic for {@link Iterate}
 */
public abstract class IterateTestBase<T> {
    protected static final int NUM_TESTS = 10;
    protected static final String ITERATIONS_OPTION_NAME = "iterations";
    protected static final String ITERATIONS_OPTION_ALTERNATE_NAME = "iterationsAlternate";
    protected static final String ORDER_OPTION_NAME = "order";
    protected static final String ORDER_VAL_CYCLIC = "cyclic";
    protected static final String ORDER_VAL_SEQUENTIAL = "sequential";
    protected static final ImmutableList<Integer> SIMPLE_INPUT = ImmutableList.copyOf(
            IntStream.range(0, NUM_TESTS).boxed().collect(Collectors.toList()));
    protected static final int EXPECTED_ITERATIONS = 25;

    @Rule
    public ExpectedException illegalArgumentExceptionThrown = ExpectedException.none();

    /**
     * Unit test the iteration count is respected.
     */
    @Test
    public void testIterationsRespected() {
        // Apply the iterate function.
        List<Integer> output = getIterate().apply(
                getArgumentsBuilder().setIteration(EXPECTED_ITERATIONS).build(), SIMPLE_INPUT);
        // Count occurrences of each integer into a map.
        Map<Integer, Long> countMap = output.stream()
                .collect(Collectors.groupingBy(Function.identity(), Collectors.counting()));
        // Ensure each of the integers have N entries.
        boolean respected =
                countMap.entrySet()
                        .stream()
                        .allMatch(entry -> (entry.getValue() == EXPECTED_ITERATIONS));
        assertThat(respected).isTrue();
    }

    /**
     * Unit test that the cyclic order is respected.
     */
    @Test
    public void testCyclicOrderRespected() {
        // Apply the iterate function.
        List<Integer> output = getIterate().apply(
                getArgumentsBuilder()
                        .setIteration(EXPECTED_ITERATIONS).setOrder(ORDER_VAL_CYCLIC).build(),
                SIMPLE_INPUT);
        // Split the output list to sublists of length NUM_TESTS; each of these
        // sublists should be identical to the input.
        List<List<Integer>> cycles = Lists.partition(output, NUM_TESTS);
        boolean cyclesSameAsInput = cycles.stream().allMatch(SIMPLE_INPUT::equals);
        assertThat(cyclesSameAsInput).isTrue();
    }

    /**
     * Unit test that the sequential order is respected.
     */
    @Test
    public void testSequentialOrderRespected() {
        // Apply the iteration function.
        List<Integer> output = getIterate().apply(
                getArgumentsBuilder()
                        .setIteration(EXPECTED_ITERATIONS).setOrder(ORDER_VAL_SEQUENTIAL).build(),
                SIMPLE_INPUT);
        // Split the output list to sublists of length EXPECTED_ITERATIONS; each
        // of these sublists should only have the same test, and the sublists
        // should follow the same order as input.
        List<List<Integer>> testRuns = Lists.partition(output, EXPECTED_ITERATIONS);
        boolean testRunsAllContainSingleTest =
                testRuns.stream().allMatch(l -> l.stream().distinct().count() == 1);
        assertThat(testRunsAllContainSingleTest).isTrue();
        boolean testRunsFollowInputOrder = SIMPLE_INPUT.equals(
                testRuns.stream().map(l -> l.get(0)).collect(Collectors.toList()));
        assertThat(testRunsFollowInputOrder).isTrue();
    }

    /** Unit test that the option name change is respected. */
    @Test
    public void testOverrideOptionName() {
        // Apply the iteration function.
        IterateBase<T, Integer> iterator = getIterate();
        iterator.setOptionName(ITERATIONS_OPTION_ALTERNATE_NAME);
        List<Integer> output =
                iterator.apply(
                        getArgumentsBuilder()
                                .setIteration(1)
                                .setAlternateIteration(EXPECTED_ITERATIONS)
                                .build(),
                        SIMPLE_INPUT);
        // Count occurrences of each integer into a map.
        Map<Integer, Long> countMap =
                output.stream()
                        .collect(Collectors.groupingBy(Function.identity(), Collectors.counting()));
        // Ensure each of the integers have N entries.
        boolean respected =
                countMap.entrySet()
                        .stream()
                        .allMatch(entry -> (entry.getValue() == EXPECTED_ITERATIONS));
        assertThat(respected).isTrue();
    }

    /**
     * Unit test that an exception is thrown for an invalid order argument.
     */
    @Test
    public void testInvalidOrderThrows() {
        // Describe what should be thrown.
        illegalArgumentExceptionThrown.expect(IllegalArgumentException.class);
        illegalArgumentExceptionThrown.expectMessage("order option");
        illegalArgumentExceptionThrown.expectMessage("invalid");
        illegalArgumentExceptionThrown.expectMessage("not supported");

        // Attempt to apply input with invalid ordering; an expected exception
        // should be thrown.
        List<Integer> output = getIterate().apply(
                getArgumentsBuilder()
                        .setIteration(EXPECTED_ITERATIONS).setOrder("invalid").build(),
                SIMPLE_INPUT);
    }

    protected abstract IterateBase<T, Integer> getIterate();

    // Test arguments builder and factory method.
    protected abstract class ArgumentsBuilder {
        protected Integer mIterations;
        protected Integer mAlternateIterations;
        protected String mOrder;

        public ArgumentsBuilder setIteration(Integer iterations) {
            mIterations = iterations;
            return this;
        }

        public ArgumentsBuilder setAlternateIteration(Integer iterations) {
            mAlternateIterations = iterations;
            return this;
        }

        public ArgumentsBuilder setOrder(String order) {
            mOrder = order;
            return this;
        }

        public abstract T build();
    }

    protected abstract ArgumentsBuilder getArgumentsBuilder();
}
