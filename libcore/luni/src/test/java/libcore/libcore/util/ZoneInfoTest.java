/*
 * Copyright (C) 2016 The Android Open Source Project
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
package libcore.libcore.util;

import junit.framework.TestCase;

import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.nio.ByteBuffer;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.Date;
import libcore.io.BufferIterator;
import libcore.timezone.ZoneInfoDb;
import libcore.timezone.testing.ZoneInfoTestHelper;
import libcore.util.ZoneInfo;

/**
 * Tests for {@link ZoneInfo}
 */
public class ZoneInfoTest extends TestCase {

  /**
   * Checks that a {@link ZoneInfo} cannot be created without any types.
   */
  public void testMakeTimeZone_NoTypes() throws Exception {
    long[][] transitions = {};
    int[][] types = {};
    try {
      createZoneInfo(transitions, types);
      fail();
    } catch (IOException expected) {
    }
  }

  /**
   * Checks that a {@link ZoneInfo} can be created with one type and no transitions.
   */
  public void testMakeTimeZone_OneType_NoTransitions() throws Exception {
    long[][] transitions = {};
    int[][] types = {
        { 4800, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types);

    // If there are no transitions then the offset should be constant irrespective of the time.
    Instant[] times = {
            Instant.ofEpochMilli(Long.MIN_VALUE),
            Instant.ofEpochMilli(0),
            Instant.ofEpochMilli(Long.MAX_VALUE),
    };
    assertOffsetAt(zoneInfo, offsetFromSeconds(4800), times);

    // No transitions means no DST.
    assertFalse("Doesn't use DST", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));

    // The raw offset should be the offset of the first type.
    assertRawOffset(zoneInfo, offsetFromSeconds(4800));
  }

  /**
   * Checks that a {@link ZoneInfo} can be created with one non-DST transition.
   */
  public void testReadTimeZone_OneNonDstTransition() throws Exception {
    long[][] transitions = {
        { 0, 0 }
    };
    int[][] types = {
        { 3600, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types);

    // Any time before the first transition is assumed to use the first standard transition.
    Instant[] times = { timeFromSeconds(-2), timeFromSeconds(0), timeFromSeconds(2) };
    assertOffsetAt(zoneInfo, offsetFromSeconds(3600), times);

    // No transitions means no DST.
    assertFalse("Doesn't use DST", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));

    // The raw offset should be the offset of the first type.
    assertRawOffset(zoneInfo, offsetFromSeconds(3600));
  }

  /**
   * Checks that a {@link ZoneInfo} cannot be created with one DST but no non-DSTs transitions.
   */
  public void testReadTimeZone_OneDstTransition() throws Exception {
    long[][] transitions = {
        { 0, 0 }
    };
    int[][] types = {
        { 3600, 1 }
    };
    try {
      createZoneInfo(transitions, types);
      fail("Did not detect no non-DST transitions");
    } catch (IllegalStateException expected) {
    }
  }

  /**
   * Checks to make sure that rounding the time from milliseconds to seconds does not cause issues
   * around the boundary of negative transitions.
   */
  public void testReadTimeZone_NegativeTransition() throws Exception {
    long[][] transitions = {
        { -2000, 0 },
        { -5, 1 },
        { 0, 2 },
    };
    int[][] types = {
        { 1800, 0 },
        { 3600, 1 },
        { 5400, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types);
    Instant transitionTime = timeFromSeconds(-5);

    // Even a millisecond before a transition means that the transition is not active.
    Instant beforeTransitionTime = transitionTime.minusMillis(1);
    assertOffsetAt(zoneInfo, offsetFromSeconds(1800), beforeTransitionTime);
    assertInDaylightTime(zoneInfo, beforeTransitionTime, false);

    // A time equal to the transition point activates the transition.
    assertOffsetAt(zoneInfo, offsetFromSeconds(3600), transitionTime);
    assertInDaylightTime(zoneInfo, transitionTime, true);

    // A time after the transition point but before the next activates the transition.
    Instant afterTransitionTime = transitionTime.plusMillis(1);
    assertOffsetAt(zoneInfo, offsetFromSeconds(3600), afterTransitionTime);
    assertInDaylightTime(zoneInfo, afterTransitionTime, true);

    assertFalse("Doesn't use DST", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));
    assertRawOffset(zoneInfo, offsetFromSeconds(5400));
  }

  /**
   * Checks to make sure that rounding the time from milliseconds to seconds does not cause issues
   * around the boundary of positive transitions.
   */
  public void testReadTimeZone_PositiveTransition() throws Exception {
    long[][] transitions = {
        { 0, 0 },
        { 5, 1 },
        { 2000, 2 },
    };
    int[][] types = {
        { 1800, 0 },
        { 3600, 1 },
        { 5400, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types);

    Instant transitionTime = timeFromSeconds(5);

    // Even a millisecond before a transition means that the transition is not active.
    Instant beforeTransitionTime = transitionTime.minusMillis(1);
    assertOffsetAt(zoneInfo, offsetFromSeconds(1800), beforeTransitionTime);
    assertInDaylightTime(zoneInfo, beforeTransitionTime, false);

    // A time equal to the transition point activates the transition.
    assertOffsetAt(zoneInfo, offsetFromSeconds(3600), transitionTime);
    assertInDaylightTime(zoneInfo, transitionTime, true);

    // A time after the transition point but before the next activates the transition.
    Instant afterTransitionTime = transitionTime.plusMillis(1);
    assertOffsetAt(zoneInfo, offsetFromSeconds(3600), afterTransitionTime);
    assertInDaylightTime(zoneInfo, afterTransitionTime, true);

    assertFalse("Doesn't use DST", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));
    assertRawOffset(zoneInfo, offsetFromSeconds(5400));
  }

  /**
   * Checks that creating a {@link ZoneInfo} with future DST transitions but no past DST
   * transitions where the transition times are negative is not affected by rounding issues.
   */
  public void testReadTimeZone_HasFutureDST_NoPastDST_NegativeTransitions() throws Exception {
    long[][] transitions = {
        { -2000, 0 },
        { -500, 1 },
        { -100, 2 },
    };
    int[][] types = {
        { 1800, 0 },
        { 3600, 0 },
        { 5400, 1 }
    };
    // The expected DST savings is the difference between the DST offset (which includes the
    // raw offset) and the preceding non-DST offset (which should just be the raw offset).
    // Or in other words (5400 - 3600) * 1000
    Duration expectedDSTSavings = offsetFromSeconds(5400 - 3600);

    ZoneInfo zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(-700));

    assertTrue("Should use DST but doesn't", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, expectedDSTSavings);

    // Now create one a few milliseconds before the DST transition to make sure that rounding
    // errors don't cause a problem.
    zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(-100).minusMillis(5));

    assertTrue("Should use DST but doesn't", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, expectedDSTSavings);
  }

  /**
   * Checks that creating a {@link ZoneInfo} with future DST transitions but no past DST
   * transitions where the transition times are positive is not affected by rounding issues.
   */
  public void testReadTimeZone_HasFutureDST_NoPastDST_PositiveTransitions() throws Exception {
    long[][] transitions = {
        { 4000, 0 },
        { 5500, 1 },
        { 6000, 2 },
    };
    int[][] types = {
        { 1800, 0 },
        { 3600, 0 },
        { 7200, 1 }
    };
    // The expected DST savings is the difference between the DST offset (which includes the
    // raw offset) and the preceding non-DST offset (which should just be the raw offset).
    // Or in other words (7200 - 3600) * 1000
    Duration expectedDSTSavings = offsetFromSeconds(7200 - 3600);

    ZoneInfo zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(4500) /* currentTime */);

    assertTrue("Should use DST but doesn't", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, expectedDSTSavings);

    // Now create one a few milliseconds before the DST transition to make sure that rounding
    // errors don't cause a problem.
    zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(6000).minusMillis(5));

    assertTrue("Should use DST but doesn't", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, expectedDSTSavings);
  }

  /**
   * Checks that creating a {@link ZoneInfo} with past DST transitions but no future DST
   * transitions where the transition times are negative is not affected by rounding issues.
   */
  public void testReadTimeZone_HasPastDST_NoFutureDST_NegativeTransitions() throws Exception {
    long[][] transitions = {
        { -5000, 0 },
        { -2000, 1 },
        { -500, 0 },
        { 0, 2 },
    };
    int[][] types = {
        { 3600, 0 },
        { 1800, 1 },
        { 5400, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(-1) /* currentTime */);

    assertFalse("Shouldn't use DST but does", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));

    // Now create one a few milliseconds after the DST transition to make sure that rounding
    // errors don't cause a problem.
    zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(-2000).plusMillis(5));

    assertFalse("Shouldn't use DST but does", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));
  }

  /**
   * Checks that creating a {@link ZoneInfo} with past DST transitions but no future DST
   * transitions where the transition times are positive is not affected by rounding issues.
   */
  public void testReadTimeZone_HasPastDST_NoFutureDST_PositiveTransitions() throws Exception {
    long[][] transitions = {
        { 1000, 0 },
        { 4000, 1 },
        { 5500, 0 },
        { 6000, 2 },
    };
    int[][] types = {
        { 3600, 0 },
        { 1800, 1 },
        { 5400, 0 }
    };
    ZoneInfo zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(4700));

    assertFalse("Shouldn't use DST but does", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));

    // Now create one a few milliseconds after the DST transition to make sure that rounding
    // errors don't cause a problem.
    zoneInfo = createZoneInfo(transitions, types, timeFromSeconds(4000).plusMillis(5));

    assertFalse("Shouldn't use DST but does", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));
  }

  /**
   * TimeZone APIs use Java long times in millis.
   *
   * <p>Android has historically mishandled the lookup of offset for times before Integer.MIN_VALUE
   * seconds for various reasons. The logic has been corrected. This test would fail on versions of
   * Android <= P.
   */
  public void testReadTimeZone_Bug118835133_extraFirstTransition() throws Exception {
    // A time before the first representable time in seconds with a java int.
    Instant before32BitTime = timeFromSeconds(Integer.MIN_VALUE).minusMillis(1);

    // Times around the 32-bit seconds minimum.
    Instant[] earlyTimes = {
            timeFromSeconds(Integer.MIN_VALUE),
            timeFromSeconds(Integer.MIN_VALUE).plusMillis(1),
    };

    Instant firstRealTransitionTime = timeFromSeconds(1000);
    Instant afterFirstRealTransitionTime = firstRealTransitionTime.plusSeconds(1);
    Instant[] afterFirstRealTransitionTimes = {
            firstRealTransitionTime,
            afterFirstRealTransitionTime,
    };

    // A time to use as currentTime when building the TimeZone. Important for the getRawOffset()
    // calculation.
    Instant currentTime = afterFirstRealTransitionTime;

    Duration type0Offset = offsetFromSeconds(0);
    Duration type1Offset = offsetFromSeconds(1800);
    Duration type2Offset = offsetFromSeconds(3600);
    int[][] types = {
            { offsetToSeconds(type0Offset), 0 }, // 1st type, used before first known transition.
            { offsetToSeconds(type1Offset), 0 },
            { offsetToSeconds(type2Offset), 0 },
    };

    // Simulates a zone with a single transition.
    {
      long[][] transitions = {
              { timeToSeconds(firstRealTransitionTime), 2 /* type 2 */ },
      };
      ZoneInfo oldZoneInfo = createZoneInfo(transitions, types, currentTime);
      assertRawOffset(oldZoneInfo, type2Offset);

      // We use the first non-DST type for times before the first transition.
      assertOffsetAt(oldZoneInfo, type0Offset, before32BitTime);
      assertOffsetAt(oldZoneInfo, type0Offset, earlyTimes);

      // This is after the first transition, so type 2.
      assertOffsetAt(oldZoneInfo, type2Offset, afterFirstRealTransitionTimes);
    }

    // Simulation a zone where there is an explicit transition at Integer.MIN_VALUE seconds. This
    // used to be common when Android used 32-bit data from the TZif file.
    {
      long[][] transitions = {
              { Integer.MIN_VALUE, 1 /* type 1 */ },
              { timeToSeconds(firstRealTransitionTime), 2 /* type 2 */ },
      };
      ZoneInfo newZoneInfo = createZoneInfo(transitions, types, currentTime);
      assertRawOffset(newZoneInfo, type2Offset);

      // We use the first non-DST type for times before the first transition.
      assertOffsetAt(newZoneInfo, type0Offset, before32BitTime);

      // After the first transition, so type 1.
      assertOffsetAt(newZoneInfo, type1Offset, earlyTimes);

      // This is after the second transition, so type 2.
      assertOffsetAt(newZoneInfo, type2Offset, afterFirstRealTransitionTimes);
    }
  }

  /**
   * Newer versions of zic after 2014b sometime introduce an explicit transition at
   * Integer.MAX_VALUE seconds.
   */
  public void testReadTimeZone_Bug118835133_extraLastTransition() throws Exception {
    // An arbitrary time to use as currentTime. Not important for this test.
    Instant currentTime = timeFromSeconds(4000);

    // Offset before time 1000 should be consistent.
    Instant[] timesToCheck = {
            timeFromSeconds(2100), // arbitrary time > 2000
            timeFromSeconds(Integer.MAX_VALUE).minusMillis(1),
            timeFromSeconds(Integer.MAX_VALUE),
            timeFromSeconds(Integer.MAX_VALUE).plusMillis(1),
    };

    int latestOffsetSeconds = 3600;
    int[][] types = {
            { 1800, 0 },
            { latestOffsetSeconds, 0 },
    };
    Duration expectedLateOffset = offsetFromSeconds(latestOffsetSeconds);

    // Create a simulation of a zone where there is no explicit transition at Integer.MAX_VALUE
    // seconds.
    {
      long[][] transitions = {
              { 1000, 0 },
              { 2000, 1 },
      };
      ZoneInfo oldZoneInfo = createZoneInfo(transitions, types, currentTime);
      assertOffsetAt(oldZoneInfo, expectedLateOffset, timesToCheck);
    }

    // Create a simulation of a zone where there is an explicit transition at Integer.MAX_VALUE
    // seconds.
    {
      long[][] transitions = {
              { 1000, 0 },
              { 2000, 1 },
              { Integer.MAX_VALUE, 1}, // The extra transition.
      };
      ZoneInfo newZoneInfo = createZoneInfo(transitions, types, currentTime);
      assertOffsetAt(newZoneInfo, expectedLateOffset, timesToCheck);
    }
  }

  /**
   * Checks to make sure that ZoneInfo can handle up to 256 types.
   */
  public void testReadTimeZone_MaxTypeCount() throws Exception {
    long[][] transitions = {
        { -2000, 255 },
    };
    // Create 256 types, each with zero offset and without DST except the last, which is offset by
    // one hour but also without DST.
    int[][] types = new int[256][];
    Arrays.fill(types, new int[2]);
    types[255] = new int[] { 3600, 0 };

    ZoneInfo zoneInfo = createZoneInfo(getName(), transitions, types,
            timeFromSeconds(Integer.MIN_VALUE));

    assertFalse("Shouldn't use DST but does", zoneInfo.useDaylightTime());
    assertDSTSavings(zoneInfo, offsetFromSeconds(0));

    // Make sure that WallTime works properly with a ZoneInfo with 256 types.
    ZoneInfo.WallTime wallTime = new ZoneInfo.WallTime();
    wallTime.localtime(0, zoneInfo);
    wallTime.mktime(zoneInfo);
  }

  /**
   * Create an instance for every available time zone for which we have data to ensure that they
   * can all be handled correctly.
   *
   * <p>This is to ensure that ZoneInfo can read all time zone data without failing, it doesn't
   * check that it reads it correctly or that the data itself is correct. This is a sanity test
   * to ensure that any additional checks added to the code that reads the data source and
   * creates the {@link ZoneInfo} instances does not prevent any of the time zones being loaded.
   */
  public void testReadTimeZone_All() throws Exception {
    ZoneInfoDb instance = ZoneInfoDb.getInstance();
    String[] availableIDs = instance.getAvailableIDs();
    Arrays.sort(availableIDs);
    for (String id : availableIDs) {
      BufferIterator bufferIterator = instance.getBufferIterator(id);

      // Create a ZoneInfo at the earliest possible time to allow us to use the
      // useDaylightTime() method to check whether it ever has or ever will support daylight
      // savings time.
      ZoneInfo zoneInfo = ZoneInfo.readTimeZone(id, bufferIterator, Long.MIN_VALUE);
      assertNotNull("TimeZone " + id + " was not created", zoneInfo);
      assertEquals(id, zoneInfo.getID());
    }
  }

  public void testReadTimeZone_Valid() throws Exception {
    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid();
    assertNotNull(createZoneInfo(getName(), Instant.now(), builder.build()));
  }

  public void testReadTimeZone_BadMagic() {
    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setMagic(0xdeadbeef); // Bad magic.
    try {
      createZoneInfo(getName(), Instant.now(), builder.build());
      fail();
    } catch (IOException expected) {}
  }

  /**
   * Checks to make sure that ZoneInfo rejects more than 256 types.
   */
  public void testReadTimeZone_TooManyTypes() {
    int typeCount = 257; // Max types allowed is 256
    int transitionCount = 5;
    long[][] transitions = createTransitions(transitionCount, typeCount);
    int[][] types = createTypes(typeCount);
    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setTransitionsAndTypes(transitions, types);
    byte[] bytes = builder.build();
    try {
      createZoneInfo(getName(), Instant.now(), bytes);
      fail("Did not detect too many types");
    } catch (IOException expected) {
    }
  }

  /**
   * Checks to make sure that ZoneInfo rejects more than 2000 transitions.
   */
  public void testReadTimeZone_TooManyTransitions() {
    int typeCount = 5;
    int transitionCount = 2001; // Max transitions allowed is 2000.
    long[][] transitions = createTransitions(transitionCount, typeCount);
    int[][] types = createTypes(typeCount);
    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setTransitionsAndTypes(transitions, types);
    byte[] bytes = builder.build();
    try {
      createZoneInfo(getName(), Instant.now(), bytes);
      fail("Did not detect too many transitions");
    } catch (IOException expected) {
    }
  }

  public void testReadTimeZone_TransitionsNotSorted() {
    long[][] transitions = {
            { 1000, 0 },
            { 3000, 1 }, // Out of transition order.
            { 2000, 0 },
    };
    int[][] types = {
            { 3600, 0 },
            { 1800, 1 },
    };

    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setTransitionsAndTypes(transitions, types);

    byte[] bytes = builder.build();
    try {
      createZoneInfo(getName(), Instant.now(), bytes);
      fail();
    } catch (IOException expected) {
    }
  }

  public void testReadTimeZone_InvalidTypeIndex() {
    long[][] transitions = {
            { 1000, 0 },
            { 2000, 2 }, // Invalid type index - only 0 and 1 defined below.
            { 3000, 0 },
    };
    int[][] types = {
            { 3600, 0 },
            { 1800, 1 },
    };

    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setTransitionsAndTypes(transitions, types);

    byte[] bytes = builder.build();
    try {
      createZoneInfo(getName(), Instant.now(), bytes);
      fail();
    } catch (IOException expected) {
    }
  }

  public void testReadTimeZone_InvalidIsDst() {
    long[][] transitions = {
            { 1000, 0 },
            { 2000, 1 },
            { 3000, 0 },
    };
    int[][] types = {
            { 3600, 0 },
            { 1800, 2 }, // Invalid isDst - must be 0 or 1
    };

    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .initializeToValid()
                    .setTransitionsAndTypes(transitions, types);

    byte[] bytes = builder.build();
    try {
      createZoneInfo(getName(), Instant.now(), bytes);
      fail();
    } catch (IOException expected) {
    }
  }

  /**
   * Checks that we can read the serialized form of a {@link ZoneInfo} created in pre-OpenJDK
   * AOSP.
   *
   * <p>One minor difference is that in pre-OpenJDK {@link ZoneInfo#mDstSavings} can be non-zero
   * even if {@link ZoneInfo#mUseDst} was false. That was not visible externally (except through
   * the {@link ZoneInfo#toString()} method) as the {@link ZoneInfo#getDSTSavings()} would check
   * {@link ZoneInfo#mUseDst} and if it was false then would return 0. This checks to make sure
   * that is handled properly. See {@link ZoneInfo#readObject(ObjectInputStream)}.
   */
  public void testReadSerialized() throws Exception {
    ZoneInfo zoneInfoRead;
    try (InputStream is = getClass().getResourceAsStream("ZoneInfoTest_ZoneInfo.golden.ser");
         ObjectInputStream ois = new ObjectInputStream(is)) {
      Object object = ois.readObject();
      assertTrue("Not a ZoneInfo instance", object instanceof ZoneInfo);
      zoneInfoRead = (ZoneInfo) object;
    }

    long[][] transitions = {
        { -5000, 0 },
        { -2000, 1 },
        { -500, 0 },
        { 0, 2 },
    };
    int[][] types = {
        { 3600, 0 },
        { 1800, 1 },
        { 5400, 0 }
    };
    ZoneInfo zoneInfoCreated = createZoneInfo(
            "test", transitions, types, timeFromSeconds(-1));

    assertEquals("Read ZoneInfo does not match created one", zoneInfoCreated, zoneInfoRead);
    assertEquals("useDaylightTime() mismatch",
        zoneInfoCreated.useDaylightTime(), zoneInfoRead.useDaylightTime());
    assertEquals("getDSTSavings() mismatch",
        zoneInfoCreated.getDSTSavings(), zoneInfoRead.getDSTSavings());
  }

  private static void assertRawOffset(ZoneInfo zoneInfo, Duration expectedOffset) {
    assertEquals(expectedOffset.toMillis(), zoneInfo.getRawOffset());
  }

  private static void assertDSTSavings(ZoneInfo zoneInfo, Duration expectedDSTSavings) {
    assertEquals(expectedDSTSavings.toMillis(), zoneInfo.getDSTSavings());
  }

  private static void assertInDaylightTime(ZoneInfo zoneInfo, Instant time, boolean expectedValue) {
    assertEquals(expectedValue, zoneInfo.inDaylightTime(new Date(time.toEpochMilli())));
  }

  private static void assertOffsetAt(
          ZoneInfo zoneInfo, Duration expectedOffset, Instant... times) {
    for (Instant time : times) {
      assertEquals("Unexpected offset at " + time,
              expectedOffset.toMillis(), zoneInfo.getOffset(time.toEpochMilli()));
    }
  }

  private static Instant timeFromSeconds(long timeInSeconds) {
    return Instant.ofEpochSecond(timeInSeconds);
  }

  private static long timeToSeconds(Instant time) {
    return time.getEpochSecond();
  }

  private static Duration offsetFromSeconds(int offsetSeconds) {
    return Duration.ofSeconds(offsetSeconds);
  }

  private static int offsetToSeconds(Duration offset) {
    long seconds = offset.getSeconds();
    if (seconds < Integer.MIN_VALUE || seconds > Integer.MAX_VALUE) {
      fail("Offset out of seconds range: " + offset);
    }
    return (int) seconds;
  }

  private ZoneInfo createZoneInfo(long[][] transitions, int[][] types)
      throws Exception {
    return createZoneInfo(getName(), transitions, types, Instant.now());
  }

  private ZoneInfo createZoneInfo(long[][] transitions, int[][] types, Instant currentTime)
          throws Exception {
    return createZoneInfo(getName(), transitions, types, currentTime);
  }

  private ZoneInfo createZoneInfo(String name, long[][] transitions, int[][] types,
          Instant currentTime) throws Exception {

    ZoneInfoTestHelper.ZicDataBuilder builder =
            new ZoneInfoTestHelper.ZicDataBuilder()
                    .setTransitionsAndTypes(transitions, types);
    return createZoneInfo(name, currentTime, builder.build());
  }

  private static ZoneInfo createZoneInfo(String name, Instant currentTime, byte[] bytes)
          throws IOException {
    ByteBufferIterator bufferIterator = new ByteBufferIterator(ByteBuffer.wrap(bytes));
    return ZoneInfo.readTimeZone(
            "TimeZone for '" + name + "'", bufferIterator, currentTime.toEpochMilli());
  }

  /**
   * Creates {@code typeCount} "types" for use with
   * {@link ZoneInfoTestHelper.ZicDataBuilder#setTypes(int[][])} and related methods. Each type is
   * given an arbitrary offset and "isDst" value.
   */
  private static int[][] createTypes(int typeCount) {
    int[][] types = new int[typeCount][2];
    for (int i = 0; i < typeCount; i++) {
      // [0] holds the offset from UTC in seconds.
      types[i][0] = typeCount;
      // [1] holds isDst: 0 == STD, 1 == DST
      types[i][1] = typeCount % 2;
    }
    return types;
  }

  /**
   * Creates {@code transitionCount} "transition pairs" for use with
   * {@link ZoneInfoTestHelper.ZicDataBuilder#setTransitions(long[][])} and related methods. Each
   * transition is given an arbitrary (but increasing) time referencing an arbitrary type.
   */
  private static long[][] createTransitions(int transitionCount, int typeCount) {
    long[][] transitions = new long[transitionCount][2];
    for (int i = 0; i < transitionCount; i++) {
      // [0] holds the transition time.
      transitions[i][0] = (i * 3600) + 100;
      // [1] holds the type index to use. Must be > 0 and < typeCount to be valid.
      transitions[i][1] = i % typeCount;
    }
    return transitions;
  }

  /**
   * A {@link BufferIterator} that wraps a {@link ByteBuffer}.
   */
  private static class ByteBufferIterator extends BufferIterator {

    private final ByteBuffer buffer;

    public ByteBufferIterator(ByteBuffer buffer) {
      this.buffer = buffer;
    }

    @Override
    public void seek(int offset) {
      buffer.position(offset);
    }

    @Override
    public void skip(int byteCount) {
      buffer.position(buffer.position() + byteCount);
    }

    @Override
    public int pos() {
      return buffer.position();
    }

    @Override
    public void readByteArray(byte[] bytes, int arrayOffset, int byteCount) {
      buffer.get(bytes, arrayOffset, byteCount);
    }

    @Override
    public byte readByte() {
      return buffer.get();
    }

    @Override
    public int readInt() {
      int value = buffer.asIntBuffer().get();
      // Using a separate view does not update the position of this buffer so do it
      // explicitly.
      skip(Integer.BYTES);
      return value;
    }

    @Override
    public void readIntArray(int[] ints, int arrayOffset, int intCount) {
      buffer.asIntBuffer().get(ints, arrayOffset, intCount);
      // Using a separate view does not update the position of this buffer so do it
      // explicitly.
      skip(Integer.BYTES * intCount);
    }

    @Override
    public void readLongArray(long[] longs, int arrayOffset, int longCount) {
      buffer.asLongBuffer().get(longs, arrayOffset, longCount);
      // Using a separate view does not update the position of this buffer so do it
      // explicitly.
      skip(Long.BYTES * longCount);
    }

    @Override
    public short readShort() {
      short value = buffer.asShortBuffer().get();
      // Using a separate view does not update the position of this buffer so do it
      // explicitly.
      skip(Short.BYTES);
      return value;
    }
  }
}
