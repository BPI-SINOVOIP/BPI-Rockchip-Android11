/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.tzdata.mts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.icu.text.TimeZoneNames;

import org.junit.Test;

import java.time.ZoneId;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TimeZone;
import java.util.concurrent.TimeUnit;

/**
 * Tests relating to time zone rules that could be changed by the time zone data module. These are
 * intended to prove that a time zone data module update hasn't broken behavior. Since time zone
 * rule mutate over time this test could be quite brittle, so it is suggested that only a few
 * examples are tested.
 */
public class TimeZoneRulesTest {

    @Test
    public void preHistoricInDaylightTime() {
        // A zone that lacks an explicit transition at Integer.MIN_VALUE with zic 2019a and 2019a
        // data.
        TimeZone tz = TimeZone.getTimeZone("CET");

        long firstTransitionTimeMillis = -1693706400000L; // Apr 30, 1916 22:00:00 GMT
        assertEquals(7200000L, tz.getOffset(firstTransitionTimeMillis));
        assertTrue(tz.inDaylightTime(new Date(firstTransitionTimeMillis)));

        long beforeFirstTransitionTimeMillis = firstTransitionTimeMillis - 1;
        assertEquals(3600000L, tz.getOffset(beforeFirstTransitionTimeMillis));
        assertFalse(tz.inDaylightTime(new Date(beforeFirstTransitionTimeMillis)));
    }

    @Test
    public void getDisplayNameShort_nonHourOffsets() {
        TimeZone iranTz = TimeZone.getTimeZone("Asia/Tehran");
        assertEquals("GMT+03:30", iranTz.getDisplayName(false, TimeZone.SHORT, Locale.UK));
        assertEquals("GMT+04:30", iranTz.getDisplayName(true, TimeZone.SHORT, Locale.UK));
    }

    @Test
    public void minimalTransitionZones() throws Exception {
        // Zones with minimal transitions, historical or future, seem ideal for testing.
        // UTC is also included, although it may be implemented differently from the others.
        String[] ids = new String[] { "Africa/Bujumbura", "Indian/Cocos", "Pacific/Wake", "UTC" };
        for (String id : ids) {
            TimeZone tz = TimeZone.getTimeZone(id);
            assertFalse(tz.useDaylightTime());
            assertFalse(tz.inDaylightTime(new Date(Integer.MIN_VALUE)));
            assertFalse(tz.inDaylightTime(new Date(0)));
            assertFalse(tz.inDaylightTime(new Date(Integer.MAX_VALUE)));
            int currentOffset = tz.getOffset(new Date(0).getTime());
            assertEquals(currentOffset, tz.getOffset(new Date(Integer.MIN_VALUE).getTime()));
            assertEquals(currentOffset, tz.getOffset(new Date(Integer.MAX_VALUE).getTime()));
        }
    }

    @Test
    public void getDSTSavings() throws Exception {
      assertEquals(0, TimeZone.getTimeZone("UTC").getDSTSavings());
      assertEquals(3600000, TimeZone.getTimeZone("America/Los_Angeles").getDSTSavings());
      assertEquals(1800000, TimeZone.getTimeZone("Australia/Lord_Howe").getDSTSavings());
    }

    // http://b/7955614 and http://b/8026776.
    @Test
    public void displayNames() throws Exception {
        checkDisplayNames(Locale.US);
    }

    @Test
    public void displayNames_nonUS() throws Exception {
        // run checkDisplayNames with an arbitrary set of Locales.
        checkDisplayNames(Locale.CHINESE);
        checkDisplayNames(Locale.FRENCH);
        checkDisplayNames(Locale.forLanguageTag("bn-BD"));
    }

    private static void checkDisplayNames(Locale locale) throws Exception {
        // Check that there are no time zones that use DST but have the same display name for
        // both standard and daylight time.
        StringBuilder failures = new StringBuilder();
        for (String id : TimeZone.getAvailableIDs()) {
            TimeZone tz = TimeZone.getTimeZone(id);
            String longDst = tz.getDisplayName(true, TimeZone.LONG, locale);
            String longStd = tz.getDisplayName(false, TimeZone.LONG, locale);
            String shortDst = tz.getDisplayName(true, TimeZone.SHORT, locale);
            String shortStd = tz.getDisplayName(false, TimeZone.SHORT, locale);

            if (tz.useDaylightTime()) {
                // The long std and dst strings must differ!
                if (longDst.equals(longStd)) {
                    failures.append(String.format("\n%20s: LD='%s' LS='%s'!",
                                                  id, longDst, longStd));
                }
                // The short std and dst strings must differ!
                if (shortDst.equals(shortStd)) {
                    failures.append(String.format("\n%20s: SD='%s' SS='%s'!",
                                                  id, shortDst, shortStd));
                }

                // If the short std matches the long dst, or the long std matches the short dst,
                // it probably means we have a time zone that icu4c doesn't believe has ever
                // observed dst.
                if (shortStd.equals(longDst)) {
                    failures.append(String.format("\n%20s: SS='%s' LD='%s'!",
                                                  id, shortStd, longDst));
                }
                if (longStd.equals(shortDst)) {
                    failures.append(String.format("\n%20s: LS='%s' SD='%s'!",
                                                  id, longStd, shortDst));
                }

                // The long and short dst strings must differ!
                if (longDst.equals(shortDst) && !longDst.startsWith("GMT")) {
                  failures.append(String.format("\n%20s: LD='%s' SD='%s'!",
                                                id, longDst, shortDst));
                }
            }

            // Sanity check that whenever a display name is just a GMT string that it's the
            // right GMT string.
            String gmtDst = formatGmtString(tz, true);
            String gmtStd = formatGmtString(tz, false);
            if (isGmtString(longDst) && !longDst.equals(gmtDst)) {
                failures.append(String.format("\n%s: LD %s", id, longDst));
            }
            if (isGmtString(longStd) && !longStd.equals(gmtStd)) {
                failures.append(String.format("\n%s: LS %s", id, longStd));
            }
            if (isGmtString(shortDst) && !shortDst.equals(gmtDst)) {
                failures.append(String.format("\n%s: SD %s", id, shortDst));
            }
            if (isGmtString(shortStd) && !shortStd.equals(gmtStd)) {
                failures.append(String.format("\n%s: SS %s", id, shortStd));
            }
        }
        assertEquals("", failures.toString());
    }

    private static boolean isGmtString(String s) {
        return s.startsWith("GMT+") || s.startsWith("GMT-");
    }

    private static String formatGmtString(TimeZone tz, boolean daylight) {
        int offset = tz.getRawOffset();
        if (daylight) {
            offset += tz.getDSTSavings();
        }
        offset /= 60000;
        char sign = '+';
        if (offset < 0) {
            sign = '-';
            offset = -offset;
        }
        return String.format("GMT%c%02d:%02d", sign, offset / 60, offset % 60);
    }

    /**
     * This test is to catch issues with the rules update process that could let the
     * "negative DST" scheme enter the Android data set for either java.util.TimeZone or
     * android.icu.util.TimeZone.
     */
    @Test
    public void dstMeansSummer() {
        // Ireland was the original example that caused the default IANA upstream tzdata to contain
        // a zone where DST is in the Winter (since tzdata 2018e, though it was tried in 2018a
        // first). This change was made to historical and future transitions.
        //
        // The upstream reasoning went like this: "Irish *Standard* Time" is summer, so the other
        // time must be the DST. So, DST is considered to be in the winter and the associated DST
        // adjustment is negative from the standard time. In the old scheme "Irish Standard Time" /
        // summer was just modeled as the DST in common with all other global time zones.
        //
        // Unfortunately, various users of formatting APIs assume standard and DST times are
        // consistent and (effectively) that "DST" means "summer". We likely cannot adopt the
        // concept of a winter DST without risking app compat issues.
        //
        // For example, getDisplayName(boolean daylight) has always returned the winter time for
        // false, and the summer time for true. If we change this then it should be changed on a
        // major release boundary, with improved APIs (e.g. a version of getDisplayName() that takes
        // a millis), existing API behavior made dependent on target API version, and after fixing
        // any platform code that makes incorrect assumptions about DST meaning "1 hour forward".

        final String timeZoneId = "Europe/Dublin";
        final Locale locale = Locale.UK;
        // 26 Oct 2015 01:00:00 GMT - one day after the start of "Greenwich Mean Time" in
        // Europe/Dublin in 2015. An arbitrary historical example of winter in Ireland.
        final long winterTimeMillis = 1445821200000L;
        final String winterTimeName = "Greenwich Mean Time";
        final int winterOffsetRawMillis = 0;
        final int winterOffsetDstMillis = 0;

        // 30 Mar 2015 01:00:00 GMT - one day after the start of "Irish Standard Time" in
        // Europe/Dublin in 2015. An arbitrary historical example of summer in Ireland.
        final long summerTimeMillis = 1427677200000L;
        final String summerTimeName = "Irish Standard Time";
        final int summerOffsetRawMillis = 0;
        final int summerOffsetDstMillis = (int) TimeUnit.HOURS.toMillis(1);

        // There is no common interface between java.util.TimeZone and android.icu.util.TimeZone
        // so the tests are for each are effectively duplicated.

        // java.util.TimeZone
        {
            java.util.TimeZone timeZone = java.util.TimeZone.getTimeZone(timeZoneId);
            assertTrue(timeZone.useDaylightTime());

            assertFalse(timeZone.inDaylightTime(new Date(winterTimeMillis)));
            assertTrue(timeZone.inDaylightTime(new Date(summerTimeMillis)));

            assertEquals(winterOffsetRawMillis + winterOffsetDstMillis,
                    timeZone.getOffset(winterTimeMillis));
            assertEquals(summerOffsetRawMillis + summerOffsetDstMillis,
                    timeZone.getOffset(summerTimeMillis));
            assertEquals(winterTimeName,
                    timeZone.getDisplayName(false /* daylight */, java.util.TimeZone.LONG,
                            locale));
            assertEquals(summerTimeName,
                    timeZone.getDisplayName(true /* daylight */, java.util.TimeZone.LONG,
                            locale));
        }

        // android.icu.util.TimeZone
        {
            android.icu.util.TimeZone timeZone = android.icu.util.TimeZone.getTimeZone(timeZoneId);
            assertTrue(timeZone.useDaylightTime());

            assertFalse(timeZone.inDaylightTime(new Date(winterTimeMillis)));
            assertTrue(timeZone.inDaylightTime(new Date(summerTimeMillis)));

            assertEquals(winterOffsetRawMillis + winterOffsetDstMillis,
                    timeZone.getOffset(winterTimeMillis));
            assertEquals(summerOffsetRawMillis + summerOffsetDstMillis,
                    timeZone.getOffset(summerTimeMillis));

            // These methods show the trouble we'd have if callers were to take the output from
            // inDaylightTime() and pass it to getDisplayName().
            assertEquals(winterTimeName,
                    timeZone.getDisplayName(false /* daylight */, android.icu.util.TimeZone.LONG,
                            locale));
            assertEquals(summerTimeName,
                    timeZone.getDisplayName(true /* daylight */, android.icu.util.TimeZone.LONG,
                            locale));

            // APIs not identical to java.util.TimeZone tested below.
            int[] offsets = new int[2];
            timeZone.getOffset(winterTimeMillis, false /* local */, offsets);
            assertEquals(winterOffsetRawMillis, offsets[0]);
            assertEquals(winterOffsetDstMillis, offsets[1]);

            timeZone.getOffset(summerTimeMillis, false /* local */, offsets);
            assertEquals(summerOffsetRawMillis, offsets[0]);
            assertEquals(summerOffsetDstMillis, offsets[1]);
        }

        // icu TimeZoneNames
        TimeZoneNames timeZoneNames = TimeZoneNames.getInstance(locale);
        // getDisplayName: date = winterTimeMillis
        assertEquals(winterTimeName, timeZoneNames.getDisplayName(
                timeZoneId, TimeZoneNames.NameType.LONG_STANDARD, winterTimeMillis));
        assertEquals(summerTimeName, timeZoneNames.getDisplayName(
                timeZoneId, TimeZoneNames.NameType.LONG_DAYLIGHT, winterTimeMillis));
        // getDisplayName: date = summerTimeMillis
        assertEquals(winterTimeName, timeZoneNames.getDisplayName(
                timeZoneId, TimeZoneNames.NameType.LONG_STANDARD, summerTimeMillis));
        assertEquals(summerTimeName, timeZoneNames.getDisplayName(
                timeZoneId, TimeZoneNames.NameType.LONG_DAYLIGHT, summerTimeMillis));
    }

    /**
     * ICU's time zone IDs may be a superset of IDs available via other APIs.
     */
    @Test
    public void timeZoneIdsKnown() {
        // java.util
        List<String> zoneInfoDbAvailableIds = Arrays.asList(java.util.TimeZone.getAvailableIDs());
        checkZoneIdsAreKnownToIcu(zoneInfoDbAvailableIds);

        // java.time
        checkZoneIdsAreKnownToIcu(ZoneId.getAvailableZoneIds());
    }

    private static void checkZoneIdsAreKnownToIcu(Collection<String> zoneInfoDbAvailableIds) {
        // ICU has a known set of IDs. We want ANY because we don't want to filter to ICU's
        // canonical IDs only.
        Set<String> icuAvailableIds = android.icu.util.TimeZone.getAvailableIDs(
                android.icu.util.TimeZone.SystemTimeZoneType.ANY, null /* region */,
                null /* rawOffset */);

        List<String> nonIcuAvailableIds = new ArrayList<>();
        List<String> creationFailureIds = new ArrayList<>();
        List<String> noCanonicalLookupIds = new ArrayList<>();
        List<String> nonSystemIds = new ArrayList<>();
        for (String zoneInfoDbId : zoneInfoDbAvailableIds) {
            if (!icuAvailableIds.contains(zoneInfoDbId)) {
                nonIcuAvailableIds.add(zoneInfoDbId);
            }

            boolean[] isSystemId = new boolean[1];
            String canonicalId = android.icu.util.TimeZone.getCanonicalID(zoneInfoDbId, isSystemId);
            if (canonicalId == null) {
                noCanonicalLookupIds.add(zoneInfoDbId);
            }
            if (!isSystemId[0]) {
                nonSystemIds.add(zoneInfoDbId);
            }

            android.icu.util.TimeZone icuTimeZone =
                    android.icu.util.TimeZone.getTimeZone(zoneInfoDbId);
            if (icuTimeZone.getID().equals(android.icu.util.TimeZone.UNKNOWN_ZONE_ID)) {
                creationFailureIds.add(zoneInfoDbId);
            }
        }
        assertTrue("Non-ICU available IDs: " + nonIcuAvailableIds
                        + ", creation failed IDs: " + creationFailureIds
                        + ", non-system IDs: " + nonSystemIds
                        + ", ids without canonical IDs: " + noCanonicalLookupIds,
                nonIcuAvailableIds.isEmpty()
                        && creationFailureIds.isEmpty()
                        && nonSystemIds.isEmpty()
                        && noCanonicalLookupIds.isEmpty());
    }
}
