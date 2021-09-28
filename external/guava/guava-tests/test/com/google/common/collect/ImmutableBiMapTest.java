/*
 * Copyright (C) 2008 The Guava Authors
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

package com.google.common.collect;

import static com.google.common.collect.testing.Helpers.mapEntry;
import static com.google.common.truth.Truth.assertThat;
import static java.util.stream.Collectors.toList;

import com.google.common.annotations.GwtCompatible;
import com.google.common.annotations.GwtIncompatible;
import com.google.common.base.Equivalence;
import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableBiMap.Builder;
import com.google.common.collect.testing.MapInterfaceTest;
import com.google.common.collect.testing.features.CollectionFeature;
import com.google.common.collect.testing.features.CollectionSize;
import com.google.common.collect.testing.features.MapFeature;
import com.google.common.collect.testing.google.BiMapGenerators.ImmutableBiMapCopyOfEntriesGenerator;
import com.google.common.collect.testing.google.BiMapGenerators.ImmutableBiMapCopyOfGenerator;
import com.google.common.collect.testing.google.BiMapGenerators.ImmutableBiMapGenerator;
import com.google.common.collect.testing.google.BiMapInverseTester;
import com.google.common.collect.testing.google.BiMapTestSuiteBuilder;
import com.google.common.collect.testing.google.TestStringBiMapGenerator;
import com.google.common.testing.CollectorTester;
import com.google.common.testing.SerializableTester;
import com.google.errorprone.annotations.CanIgnoreReturnValue;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.stream.Collector;
import java.util.stream.Stream;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.checkerframework.checker.nullness.qual.Nullable;

/**
 * Tests for {@link ImmutableBiMap}.
 *
 * @author Jared Levy
 */
@GwtCompatible(emulated = true)
public class ImmutableBiMapTest extends TestCase {

  // TODO: Reduce duplication of ImmutableMapTest code

  @GwtIncompatible // suite
  public static Test suite() {
    TestSuite suite = new TestSuite();

    suite.addTestSuite(MapTests.class);
    suite.addTestSuite(InverseMapTests.class);
    suite.addTestSuite(CreationTests.class);
    suite.addTestSuite(BiMapSpecificTests.class);

    suite.addTest(
        BiMapTestSuiteBuilder.using(new ImmutableBiMapGenerator())
            .named("ImmutableBiMap")
            .withFeatures(
                CollectionSize.ANY,
                CollectionFeature.SERIALIZABLE,
                CollectionFeature.KNOWN_ORDER,
                MapFeature.REJECTS_DUPLICATES_AT_CREATION,
                MapFeature.ALLOWS_ANY_NULL_QUERIES)
            .suppressing(BiMapInverseTester.getInverseSameAfterSerializingMethods())
            .createTestSuite());
    suite.addTest(
        BiMapTestSuiteBuilder.using(
                new TestStringBiMapGenerator() {
                  @Override
                  protected BiMap<String, String> create(Entry<String, String>[] entries) {
                    return ImmutableBiMap.<String, String>builder()
                        .putAll(Arrays.asList(entries))
                        .buildJdkBacked();
                  }
                })
            .named("ImmutableBiMap [JDK backed]")
            .withFeatures(
                CollectionSize.ANY,
                CollectionFeature.SERIALIZABLE,
                CollectionFeature.KNOWN_ORDER,
                MapFeature.REJECTS_DUPLICATES_AT_CREATION,
                MapFeature.ALLOWS_ANY_NULL_QUERIES)
            .suppressing(BiMapInverseTester.getInverseSameAfterSerializingMethods())
            .createTestSuite());
    suite.addTest(
        BiMapTestSuiteBuilder.using(new ImmutableBiMapCopyOfGenerator())
            .named("ImmutableBiMap.copyOf[Map]")
            .withFeatures(
                CollectionSize.ANY,
                CollectionFeature.SERIALIZABLE,
                CollectionFeature.KNOWN_ORDER,
                MapFeature.ALLOWS_ANY_NULL_QUERIES)
            .suppressing(BiMapInverseTester.getInverseSameAfterSerializingMethods())
            .createTestSuite());
    suite.addTest(
        BiMapTestSuiteBuilder.using(new ImmutableBiMapCopyOfEntriesGenerator())
            .named("ImmutableBiMap.copyOf[Iterable<Entry>]")
            .withFeatures(
                CollectionSize.ANY,
                CollectionFeature.SERIALIZABLE,
                CollectionFeature.KNOWN_ORDER,
                MapFeature.REJECTS_DUPLICATES_AT_CREATION,
                MapFeature.ALLOWS_ANY_NULL_QUERIES)
            .suppressing(BiMapInverseTester.getInverseSameAfterSerializingMethods())
            .createTestSuite());

    return suite;
  }

  public abstract static class AbstractMapTests<K, V> extends MapInterfaceTest<K, V> {
    public AbstractMapTests() {
      super(false, false, false, false, false);
    }

    @Override
    protected Map<K, V> makeEmptyMap() {
      throw new UnsupportedOperationException();
    }

    private static final Joiner joiner = Joiner.on(", ");

    @Override
    protected void assertMoreInvariants(Map<K, V> map) {

      BiMap<K, V> bimap = (BiMap<K, V>) map;

      for (Entry<K, V> entry : map.entrySet()) {
        assertEquals(entry.getKey() + "=" + entry.getValue(), entry.toString());
        assertEquals(entry.getKey(), bimap.inverse().get(entry.getValue()));
      }

      assertEquals("{" + joiner.join(map.entrySet()) + "}", map.toString());
      assertEquals("[" + joiner.join(map.entrySet()) + "]", map.entrySet().toString());
      assertEquals("[" + joiner.join(map.keySet()) + "]", map.keySet().toString());
      assertEquals("[" + joiner.join(map.values()) + "]", map.values().toString());

      assertEquals(Sets.newHashSet(map.entrySet()), map.entrySet());
      assertEquals(Sets.newHashSet(map.keySet()), map.keySet());
    }
  }

  public static class MapTests extends AbstractMapTests<String, Integer> {
    @Override
    protected Map<String, Integer> makeEmptyMap() {
      return ImmutableBiMap.of();
    }

    @Override
    protected Map<String, Integer> makePopulatedMap() {
      return ImmutableBiMap.of("one", 1, "two", 2, "three", 3);
    }

    @Override
    protected String getKeyNotInPopulatedMap() {
      return "minus one";
    }

    @Override
    protected Integer getValueNotInPopulatedMap() {
      return -1;
    }
  }

  public static class InverseMapTests extends AbstractMapTests<String, Integer> {
    @Override
    protected Map<String, Integer> makeEmptyMap() {
      return ImmutableBiMap.of();
    }

    @Override
    protected Map<String, Integer> makePopulatedMap() {
      return ImmutableBiMap.of(1, "one", 2, "two", 3, "three").inverse();
    }

    @Override
    protected String getKeyNotInPopulatedMap() {
      return "minus one";
    }

    @Override
    protected Integer getValueNotInPopulatedMap() {
      return -1;
    }
  }

  public static class CreationTests extends TestCase {
    public void testEmptyBuilder() {
      ImmutableBiMap<String, Integer> map = new Builder<String, Integer>().build();
      assertEquals(Collections.<String, Integer>emptyMap(), map);
      assertEquals(Collections.<Integer, String>emptyMap(), map.inverse());
      assertSame(ImmutableBiMap.of(), map);
    }

    public void testSingletonBuilder() {
      ImmutableBiMap<String, Integer> map = new Builder<String, Integer>().put("one", 1).build();
      assertMapEquals(map, "one", 1);
      assertMapEquals(map.inverse(), 1, "one");
    }

    public void testBuilder_withImmutableEntry() {
      ImmutableBiMap<String, Integer> map =
          new Builder<String, Integer>().put(Maps.immutableEntry("one", 1)).build();
      assertMapEquals(map, "one", 1);
    }

    public void testBuilder() {
      ImmutableBiMap<String, Integer> map =
          ImmutableBiMap.<String, Integer>builder()
              .put("one", 1)
              .put("two", 2)
              .put("three", 3)
              .put("four", 4)
              .put("five", 5)
              .build();
      assertMapEquals(map, "one", 1, "two", 2, "three", 3, "four", 4, "five", 5);
      assertMapEquals(map.inverse(), 1, "one", 2, "two", 3, "three", 4, "four", 5, "five");
    }

    @GwtIncompatible
    public void testBuilderExactlySizedReusesArray() {
      ImmutableBiMap.Builder<Integer, Integer> builder = ImmutableBiMap.builderWithExpectedSize(10);
      Entry<Integer, Integer>[] builderArray = builder.entries;
      for (int i = 0; i < 10; i++) {
        builder.put(i, i);
      }
      Entry<Integer, Integer>[] builderArrayAfterPuts = builder.entries;
      RegularImmutableBiMap<Integer, Integer> map =
          (RegularImmutableBiMap<Integer, Integer>) builder.build();
      Entry<Integer, Integer>[] mapInternalArray = map.entries;
      assertSame(builderArray, builderArrayAfterPuts);
      assertSame(builderArray, mapInternalArray);
    }

    public void testBuilder_orderEntriesByValue() {
      ImmutableBiMap<String, Integer> map =
          ImmutableBiMap.<String, Integer>builder()
              .orderEntriesByValue(Ordering.natural())
              .put("three", 3)
              .put("one", 1)
              .put("five", 5)
              .put("four", 4)
              .put("two", 2)
              .build();
      assertMapEquals(map, "one", 1, "two", 2, "three", 3, "four", 4, "five", 5);
      assertMapEquals(map.inverse(), 1, "one", 2, "two", 3, "three", 4, "four", 5, "five");
    }

    public void testBuilder_orderEntriesByValueAfterExactSizeBuild() {
      ImmutableBiMap.Builder<String, Integer> builder =
          new ImmutableBiMap.Builder<String, Integer>(2).put("four", 4).put("one", 1);
      ImmutableMap<String, Integer> keyOrdered = builder.build();
      ImmutableMap<String, Integer> valueOrdered =
          builder.orderEntriesByValue(Ordering.natural()).build();
      assertMapEquals(keyOrdered, "four", 4, "one", 1);
      assertMapEquals(valueOrdered, "one", 1, "four", 4);
    }

    public void testBuilder_orderEntriesByValue_usedTwiceFails() {
      ImmutableBiMap.Builder<String, Integer> builder =
          new Builder<String, Integer>().orderEntriesByValue(Ordering.natural());
      try {
        builder.orderEntriesByValue(Ordering.natural());
        fail("Expected IllegalStateException");
      } catch (IllegalStateException expected) {
      }
    }

    public void testBuilderPutAllWithEmptyMap() {
      ImmutableBiMap<String, Integer> map =
          new Builder<String, Integer>().putAll(Collections.<String, Integer>emptyMap()).build();
      assertEquals(Collections.<String, Integer>emptyMap(), map);
    }

    public void testBuilderPutAll() {
      Map<String, Integer> toPut = new LinkedHashMap<>();
      toPut.put("one", 1);
      toPut.put("two", 2);
      toPut.put("three", 3);
      Map<String, Integer> moreToPut = new LinkedHashMap<>();
      moreToPut.put("four", 4);
      moreToPut.put("five", 5);

      ImmutableBiMap<String, Integer> map =
          new Builder<String, Integer>().putAll(toPut).putAll(moreToPut).build();
      assertMapEquals(map, "one", 1, "two", 2, "three", 3, "four", 4, "five", 5);
      assertMapEquals(map.inverse(), 1, "one", 2, "two", 3, "three", 4, "four", 5, "five");
    }

    public void testBuilderReuse() {
      Builder<String, Integer> builder = new Builder<>();
      ImmutableBiMap<String, Integer> mapOne = builder.put("one", 1).put("two", 2).build();
      ImmutableBiMap<String, Integer> mapTwo = builder.put("three", 3).put("four", 4).build();

      assertMapEquals(mapOne, "one", 1, "two", 2);
      assertMapEquals(mapOne.inverse(), 1, "one", 2, "two");
      assertMapEquals(mapTwo, "one", 1, "two", 2, "three", 3, "four", 4);
      assertMapEquals(mapTwo.inverse(), 1, "one", 2, "two", 3, "three", 4, "four");
    }

    public void testBuilderPutNullKey() {
      Builder<String, Integer> builder = new Builder<>();
      try {
        builder.put(null, 1);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testBuilderPutNullValue() {
      Builder<String, Integer> builder = new Builder<>();
      try {
        builder.put("one", null);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testBuilderPutNullKeyViaPutAll() {
      Builder<String, Integer> builder = new Builder<>();
      try {
        builder.putAll(Collections.<String, Integer>singletonMap(null, 1));
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testBuilderPutNullValueViaPutAll() {
      Builder<String, Integer> builder = new Builder<>();
      try {
        builder.putAll(Collections.<String, Integer>singletonMap("one", null));
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testPuttingTheSameKeyTwiceThrowsOnBuild() {
      Builder<String, Integer> builder =
          new Builder<String, Integer>()
              .put("one", 1)
              .put("one", 1); // throwing on this line would be even better

      try {
        builder.build();
        fail();
      } catch (IllegalArgumentException expected) {
        assertThat(expected.getMessage()).contains("one");
      }
    }

    public void testOf() {
      assertMapEquals(ImmutableBiMap.of("one", 1), "one", 1);
      assertMapEquals(ImmutableBiMap.of("one", 1).inverse(), 1, "one");
      assertMapEquals(ImmutableBiMap.of("one", 1, "two", 2), "one", 1, "two", 2);
      assertMapEquals(ImmutableBiMap.of("one", 1, "two", 2).inverse(), 1, "one", 2, "two");
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3), "one", 1, "two", 2, "three", 3);
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3).inverse(),
          1,
          "one",
          2,
          "two",
          3,
          "three");
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3, "four", 4),
          "one",
          1,
          "two",
          2,
          "three",
          3,
          "four",
          4);
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3, "four", 4).inverse(),
          1,
          "one",
          2,
          "two",
          3,
          "three",
          4,
          "four");
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3, "four", 4, "five", 5),
          "one",
          1,
          "two",
          2,
          "three",
          3,
          "four",
          4,
          "five",
          5);
      assertMapEquals(
          ImmutableBiMap.of("one", 1, "two", 2, "three", 3, "four", 4, "five", 5).inverse(),
          1,
          "one",
          2,
          "two",
          3,
          "three",
          4,
          "four",
          5,
          "five");
    }

    public void testOfNullKey() {
      try {
        ImmutableBiMap.of(null, 1);
        fail();
      } catch (NullPointerException expected) {
      }

      try {
        ImmutableBiMap.of("one", 1, null, 2);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testOfNullValue() {
      try {
        ImmutableBiMap.of("one", null);
        fail();
      } catch (NullPointerException expected) {
      }

      try {
        ImmutableBiMap.of("one", 1, "two", null);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    public void testOfWithDuplicateKey() {
      try {
        ImmutableBiMap.of("one", 1, "one", 1);
        fail();
      } catch (IllegalArgumentException expected) {
        assertThat(expected.getMessage()).contains("one");
      }
    }

    public void testCopyOfEmptyMap() {
      ImmutableBiMap<String, Integer> copy =
          ImmutableBiMap.copyOf(Collections.<String, Integer>emptyMap());
      assertEquals(Collections.<String, Integer>emptyMap(), copy);
      assertSame(copy, ImmutableBiMap.copyOf(copy));
      assertSame(ImmutableBiMap.of(), copy);
    }

    public void testCopyOfSingletonMap() {
      ImmutableBiMap<String, Integer> copy =
          ImmutableBiMap.copyOf(Collections.singletonMap("one", 1));
      assertMapEquals(copy, "one", 1);
      assertSame(copy, ImmutableBiMap.copyOf(copy));
    }

    public void testCopyOf() {
      Map<String, Integer> original = new LinkedHashMap<>();
      original.put("one", 1);
      original.put("two", 2);
      original.put("three", 3);

      ImmutableBiMap<String, Integer> copy = ImmutableBiMap.copyOf(original);
      assertMapEquals(copy, "one", 1, "two", 2, "three", 3);
      assertSame(copy, ImmutableBiMap.copyOf(copy));
    }

    public void testEmpty() {
      ImmutableBiMap<String, Integer> bimap = ImmutableBiMap.of();
      assertEquals(Collections.<String, Integer>emptyMap(), bimap);
      assertEquals(Collections.<String, Integer>emptyMap(), bimap.inverse());
    }

    public void testFromHashMap() {
      Map<String, Integer> hashMap = Maps.newLinkedHashMap();
      hashMap.put("one", 1);
      hashMap.put("two", 2);
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2));
      assertMapEquals(bimap, "one", 1, "two", 2);
      assertMapEquals(bimap.inverse(), 1, "one", 2, "two");
    }

    public void testFromImmutableMap() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(
              new ImmutableMap.Builder<String, Integer>()
                  .put("one", 1)
                  .put("two", 2)
                  .put("three", 3)
                  .put("four", 4)
                  .put("five", 5)
                  .build());
      assertMapEquals(bimap, "one", 1, "two", 2, "three", 3, "four", 4, "five", 5);
      assertMapEquals(bimap.inverse(), 1, "one", 2, "two", 3, "three", 4, "four", 5, "five");
    }

    public void testDuplicateValues() {
      ImmutableMap<String, Integer> map =
          new ImmutableMap.Builder<String, Integer>()
              .put("one", 1)
              .put("two", 2)
              .put("uno", 1)
              .put("dos", 2)
              .build();

      try {
        ImmutableBiMap.copyOf(map);
        fail();
      } catch (IllegalArgumentException expected) {
        assertThat(expected.getMessage()).contains("1");
      }
    }

    public void testToImmutableBiMap() {
      Collector<Entry<String, Integer>, ?, ImmutableBiMap<String, Integer>> collector =
          ImmutableBiMap.toImmutableBiMap(Entry::getKey, Entry::getValue);
      Equivalence<ImmutableBiMap<String, Integer>> equivalence =
          Equivalence.equals()
              .<Entry<String, Integer>>pairwise()
              .onResultOf(ImmutableBiMap::entrySet);
      CollectorTester.of(collector, equivalence)
          .expectCollects(
              ImmutableBiMap.of("one", 1, "two", 2, "three", 3),
              mapEntry("one", 1),
              mapEntry("two", 2),
              mapEntry("three", 3));
    }

    public void testToImmutableBiMap_exceptionOnDuplicateKey() {
      Collector<Entry<String, Integer>, ?, ImmutableBiMap<String, Integer>> collector =
          ImmutableBiMap.toImmutableBiMap(Entry::getKey, Entry::getValue);
      try {
        Stream.of(mapEntry("one", 1), mapEntry("one", 11)).collect(collector);
        fail("Expected IllegalArgumentException");
      } catch (IllegalArgumentException expected) {
      }
    }
  }

  public static class BiMapSpecificTests extends TestCase {

    public void testForcePut() {
      BiMap<String, Integer> bimap = ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2));
      try {
        bimap.forcePut("three", 3);
        fail();
      } catch (UnsupportedOperationException expected) {
      }
    }

    public void testKeySet() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2, "three", 3, "four", 4));
      Set<String> keys = bimap.keySet();
      assertEquals(Sets.newHashSet("one", "two", "three", "four"), keys);
      assertThat(keys).containsExactly("one", "two", "three", "four").inOrder();
    }

    public void testValues() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2, "three", 3, "four", 4));
      Set<Integer> values = bimap.values();
      assertEquals(Sets.newHashSet(1, 2, 3, 4), values);
      assertThat(values).containsExactly(1, 2, 3, 4).inOrder();
    }

    public void testDoubleInverse() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2));
      assertSame(bimap, bimap.inverse().inverse());
    }

    @GwtIncompatible // SerializableTester
    public void testEmptySerialization() {
      ImmutableBiMap<String, Integer> bimap = ImmutableBiMap.of();
      assertSame(bimap, SerializableTester.reserializeAndAssert(bimap));
    }

    @GwtIncompatible // SerializableTester
    public void testSerialization() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of("one", 1, "two", 2));
      ImmutableBiMap<String, Integer> copy = SerializableTester.reserializeAndAssert(bimap);
      assertEquals(Integer.valueOf(1), copy.get("one"));
      assertEquals("one", copy.inverse().get(1));
      assertSame(copy, copy.inverse().inverse());
    }

    @GwtIncompatible // SerializableTester
    public void testInverseSerialization() {
      ImmutableBiMap<String, Integer> bimap =
          ImmutableBiMap.copyOf(ImmutableMap.of(1, "one", 2, "two")).inverse();
      ImmutableBiMap<String, Integer> copy = SerializableTester.reserializeAndAssert(bimap);
      assertEquals(Integer.valueOf(1), copy.get("one"));
      assertEquals("one", copy.inverse().get(1));
      assertSame(copy, copy.inverse().inverse());
    }
  }

  private static <K, V> void assertMapEquals(Map<K, V> map, Object... alternatingKeysAndValues) {
    int i = 0;
    for (Entry<K, V> entry : map.entrySet()) {
      assertEquals(alternatingKeysAndValues[i++], entry.getKey());
      assertEquals(alternatingKeysAndValues[i++], entry.getValue());
    }
  }

  /**
   * A Comparable wrapper around a String which executes callbacks on calls to hashCode, equals, and
   * compareTo.
   */
  private static class CountsHashCodeAndEquals implements Comparable<CountsHashCodeAndEquals> {
    private final String delegateString;
    private final Runnable onHashCode;
    private final Runnable onEquals;
    private final Runnable onCompareTo;

    CountsHashCodeAndEquals(
        String delegateString, Runnable onHashCode, Runnable onEquals, Runnable onCompareTo) {
      this.delegateString = delegateString;
      this.onHashCode = onHashCode;
      this.onEquals = onEquals;
      this.onCompareTo = onCompareTo;
    }

    @Override
    public int hashCode() {
      onHashCode.run();
      return delegateString.hashCode();
    }

    @Override
    public boolean equals(@Nullable Object other) {
      onEquals.run();
      return other instanceof CountsHashCodeAndEquals
          && delegateString.equals(((CountsHashCodeAndEquals) other).delegateString);
    }

    @Override
    public int compareTo(CountsHashCodeAndEquals o) {
      onCompareTo.run();
      return delegateString.compareTo(o.delegateString);
    }
  }

  /** A holder of counters for calls to hashCode, equals, and compareTo. */
  private static final class CallsCounter {
    long hashCode;
    long equals;
    long compareTo;

    long total() {
      return hashCode + equals + compareTo;
    }

    void zero() {
      hashCode = 0;
      equals = 0;
      compareTo = 0;
    }
  }

  /** All the ways to create an ImmutableBiMap. */
  enum ConstructionPathway {
    COPY_OF_MAP {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        Map<Object, Object> sourceMap = new LinkedHashMap<>();
        for (Entry<?, ?> entry : entries) {
          if (sourceMap.put(entry.getKey(), entry.getValue()) != null) {
            throw new UnsupportedOperationException("duplicate key");
          }
        }
        counter.zero();
        return ImmutableBiMap.copyOf(sourceMap);
      }
    },
    COPY_OF_ENTRIES {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        return ImmutableBiMap.copyOf(entries);
      }
    },
    BUILDER_PUT_ONE_BY_ONE {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        ImmutableBiMap.Builder<Object, Object> builder = ImmutableBiMap.builder();
        for (Entry<?, ?> entry : entries) {
          builder.put(entry.getKey(), entry.getValue());
        }
        return builder.build();
      }
    },
    BUILDER_PUT_ENTRY_ONE_BY_ONE {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        ImmutableBiMap.Builder<Object, Object> builder = ImmutableBiMap.builder();
        for (Entry<?, ?> entry : entries) {
          builder.put(entry);
        }
        return builder.build();
      }
    },
    BUILDER_PUT_ALL_MAP {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        Map<Object, Object> sourceMap = new LinkedHashMap<>();
        for (Entry<?, ?> entry : entries) {
          if (sourceMap.put(entry.getKey(), entry.getValue()) != null) {
            throw new UnsupportedOperationException("duplicate key");
          }
        }
        counter.zero();
        ImmutableBiMap.Builder<Object, Object> builder = ImmutableBiMap.builder();
        builder.putAll(sourceMap);
        return builder.build();
      }
    },
    BUILDER_PUT_ALL_ENTRIES {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        ImmutableBiMap.Builder<Object, Object> builder = ImmutableBiMap.builder();
        builder.putAll(entries);
        return builder.build();
      }
    },
    FORCE_JDK {
      @Override
      ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter) {
        ImmutableBiMap.Builder<Object, Object> builder = ImmutableBiMap.builder();
        builder.putAll(entries);
        return builder.buildJdkBacked();
      }
    };

    @CanIgnoreReturnValue
    abstract ImmutableBiMap<?, ?> create(List<? extends Entry<?, ?>> entries, CallsCounter counter);
  }

  /**
   * Returns a list of objects with the same hash code, of size 2^power, counting calls to equals,
   * hashCode, and compareTo in counter.
   */
  static List<CountsHashCodeAndEquals> createAdversarialObjects(int power, CallsCounter counter) {
    String str1 = "Aa";
    String str2 = "BB";
    assertEquals(str1.hashCode(), str2.hashCode());
    List<String> haveSameHashes2 = Arrays.asList(str1, str2);
    List<CountsHashCodeAndEquals> result =
        Lists.newArrayList(
            Lists.transform(
                Lists.cartesianProduct(Collections.nCopies(power, haveSameHashes2)),
                strs ->
                    new CountsHashCodeAndEquals(
                        String.join("", strs),
                        () -> counter.hashCode++,
                        () -> counter.equals++,
                        () -> counter.compareTo++)));
    assertEquals(
        result.get(0).delegateString.hashCode(),
        result.get(result.size() - 1).delegateString.hashCode());
    return result;
  }

  enum AdversaryType {
    ADVERSARIAL_KEYS {
      @Override
      List<? extends Entry<?, ?>> createAdversarialEntries(int power, CallsCounter counter) {
        return createAdversarialObjects(power, counter).stream()
            .map(k -> Maps.immutableEntry(k, new Object()))
            .collect(toList());
      }
    },
    ADVERSARIAL_VALUES {
      @Override
      List<? extends Entry<?, ?>> createAdversarialEntries(int power, CallsCounter counter) {
        return createAdversarialObjects(power, counter).stream()
            .map(k -> Maps.immutableEntry(new Object(), k))
            .collect(toList());
      }
    },
    ADVERSARIAL_KEYS_AND_VALUES {
      @Override
      List<? extends Entry<?, ?>> createAdversarialEntries(int power, CallsCounter counter) {
        List<?> keys = createAdversarialObjects(power, counter);
        List<?> values = createAdversarialObjects(power, counter);
        return Streams.zip(keys.stream(), values.stream(), Maps::immutableEntry).collect(toList());
      }
    };

    abstract List<? extends Entry<?, ?>> createAdversarialEntries(int power, CallsCounter counter);
  }

  @GwtIncompatible
  public void testResistsHashFloodingInConstruction() {
    for (AdversaryType adversary : AdversaryType.values()) {
      CallsCounter smallCounter = new CallsCounter();
      List<? extends Entry<?, ?>> smallEntries =
          adversary.createAdversarialEntries(10, smallCounter);
      int smallSize = smallEntries.size();

      CallsCounter largeCounter = new CallsCounter();
      List<? extends Entry<?, ?>> largeEntries =
          adversary.createAdversarialEntries(15, largeCounter);
      int largeSize = largeEntries.size();

      for (ConstructionPathway pathway : ConstructionPathway.values()) {
        smallCounter.zero();
        pathway.create(smallEntries, smallCounter);
        long smallOps = smallCounter.total();

        largeCounter.zero();
        pathway.create(largeEntries, largeCounter);
        long largeOps = largeCounter.total();

        double ratio = (double) largeOps / smallOps;
        assertThat(ratio)
            .named(
                "ratio of equals/hashCode/compareTo operations to build an ImmutableBiMap with %s"
                    + " via %s with %s entries versus %s entries",
                adversary, pathway, largeSize, smallSize)
            .isAtMost(2 * (largeSize * Math.log(largeSize)) / (smallSize * Math.log(smallSize)));
        // allow up to 2x wobble in the constant factors
      }
    }
  }

  @GwtIncompatible
  public void testResistsHashFloodingOnForwardGet() {
    for (AdversaryType adversary : AdversaryType.values()) {
      CallsCounter smallCounter = new CallsCounter();
      List<? extends Entry<?, ?>> smallEntries =
          adversary.createAdversarialEntries(10, smallCounter);
      ImmutableBiMap<?, ?> smallMap =
          ConstructionPathway.COPY_OF_ENTRIES.create(smallEntries, smallCounter);
      int smallSize = smallEntries.size();
      long smallOps = worstCaseQueryOperations(smallMap, smallCounter);

      CallsCounter largeCounter = new CallsCounter();
      List<? extends Entry<?, ?>> largeEntries =
          adversary.createAdversarialEntries(15, largeCounter);
      ImmutableBiMap<?, ?> largeMap =
          ConstructionPathway.COPY_OF_ENTRIES.create(largeEntries, largeCounter);
      int largeSize = largeEntries.size();
      long largeOps = worstCaseQueryOperations(largeMap, largeCounter);

      if (smallOps == 0 && largeOps == 0) {
        continue; // no queries on the CHCAE objects
      }

      double ratio = (double) largeOps / smallOps;
      assertThat(ratio)
          .named(
              "Ratio of worst case get operations for an ImmutableBiMap with %s of size "
                  + "%s versus %s",
              adversary, largeSize, smallSize)
          .isAtMost(2 * Math.log(largeSize) / Math.log(smallSize));
      // allow up to 2x wobble in the constant factors
    }
  }

  @GwtIncompatible
  public void testResistsHashFloodingOnInverseGet() {
    for (AdversaryType adversary : AdversaryType.values()) {
      CallsCounter smallCounter = new CallsCounter();
      List<? extends Entry<?, ?>> smallEntries =
          adversary.createAdversarialEntries(10, smallCounter);
      ImmutableBiMap<?, ?> smallMap =
          ConstructionPathway.COPY_OF_ENTRIES.create(smallEntries, smallCounter);
      int smallSize = smallEntries.size();
      long smallOps = worstCaseQueryOperations(smallMap.inverse(), smallCounter);

      CallsCounter largeCounter = new CallsCounter();
      List<? extends Entry<?, ?>> largeEntries =
          adversary.createAdversarialEntries(15, largeCounter);
      ImmutableBiMap<?, ?> largeMap =
          ConstructionPathway.COPY_OF_ENTRIES.create(largeEntries, largeCounter);
      int largeSize = largeEntries.size();
      long largeOps = worstCaseQueryOperations(largeMap.inverse(), largeCounter);

      if (smallOps == 0 && largeOps == 0) {
        continue; // no queries on the CHCAE objects
      }
      double ratio = (double) largeOps / smallOps;
      assertThat(ratio)
          .named(
              "Ratio of worst case get operations for an ImmutableBiMap with %s of size "
                  + "%s versus %s",
              adversary, largeSize, smallSize)
          .isAtMost(2 * Math.log(largeSize) / Math.log(smallSize));
      // allow up to 2x wobble in the constant factors
    }
  }

  private static long worstCaseQueryOperations(Map<?, ?> map, CallsCounter counter) {
    long worstCalls = 0;
    for (Object k : map.keySet()) {
      counter.zero();
      Object unused = map.get(k);
      worstCalls = Math.max(worstCalls, counter.total());
    }
    return worstCalls;
  }
}
