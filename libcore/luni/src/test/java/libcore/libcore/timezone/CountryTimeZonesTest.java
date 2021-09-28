/*
 * Copyright (C) 2017 The Android Open Source Project
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

package libcore.libcore.timezone;

import org.junit.Test;

import android.icu.util.TimeZone;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import java.util.function.Function;
import java.util.stream.Collectors;
import libcore.timezone.CountryTimeZones;
import libcore.timezone.CountryTimeZones.OffsetResult;
import libcore.timezone.CountryTimeZones.TimeZoneMapping;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class CountryTimeZonesTest {

    private static final int HOUR_MILLIS = 60 * 60 * 1000;

    private static final String INVALID_TZ_ID = "Moon/Tranquility_Base";

    // Zones used in the tests. NY_TZ and LON_TZ chosen because they never overlap but both have
    // DST.
    private static final TimeZone NY_TZ = TimeZone.getTimeZone("America/New_York");
    private static final TimeZone LON_TZ = TimeZone.getTimeZone("Europe/London");
    // A zone that matches LON_TZ for WHEN_NO_DST. It does not have DST so differs for WHEN_DST.
    private static final TimeZone REYK_TZ = TimeZone.getTimeZone("Atlantic/Reykjavik");
    // Another zone that matches LON_TZ for WHEN_NO_DST. It does not have DST so differs for
    // WHEN_DST.
    private static final TimeZone UTC_TZ = TimeZone.getTimeZone("Etc/UTC");

    // 22nd July 2017, 13:14:15 UTC (DST time in all the timezones used in these tests that observe
    // DST).
    private static final long WHEN_DST = 1500729255000L;
    // 22nd January 2018, 13:14:15 UTC (non-DST time in all timezones used in these tests).
    private static final long WHEN_NO_DST = 1516626855000L;

    // The offset applied to most zones during DST.
    private static final int NORMAL_DST_ADJUSTMENT = HOUR_MILLIS;

    private static final int LON_NO_DST_TOTAL_OFFSET = 0;
    private static final int LON_DST_TOTAL_OFFSET = LON_NO_DST_TOTAL_OFFSET
            + NORMAL_DST_ADJUSTMENT;

    private static final int NY_NO_DST_TOTAL_OFFSET = -5 * HOUR_MILLIS;
    private static final int NY_DST_TOTAL_OFFSET = NY_NO_DST_TOTAL_OFFSET
            + NORMAL_DST_ADJUSTMENT;

    @Test
    public void createValidated() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */,
                true /* everUsesUtc */, timeZoneMappings("Europe/London"), "test");
        assertTrue(countryTimeZones.isForCountryCode("gb"));
        assertEquals("Europe/London", countryTimeZones.getDefaultTimeZoneId());
        assertZoneEquals(zone("Europe/London"), countryTimeZones.getDefaultTimeZone());
        assertEquals(timeZoneMappings("Europe/London"), countryTimeZones.getTimeZoneMappings());
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0 /* whenMillis */));
    }

    @Test
    public void createValidated_nullDefault() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", null, false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");
        assertNull(countryTimeZones.getDefaultTimeZoneId());
        assertNull(countryTimeZones.getDefaultTimeZone());
    }

    @Test
    public void createValidated_invalidDefault() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", INVALID_TZ_ID, false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London", INVALID_TZ_ID), "test");
        assertNull(countryTimeZones.getDefaultTimeZoneId());
        assertNull(countryTimeZones.getDefaultTimeZone());
        assertEquals(timeZoneMappings("Europe/London"), countryTimeZones.getTimeZoneMappings());
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0 /* whenMillis */));
    }

    @Test
    public void createValidated_unknownTimeZoneIdIgnored() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Unknown_Id", "Europe/London"), "test");
        assertEquals(timeZoneMappings("Europe/London"), countryTimeZones.getTimeZoneMappings());
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0 /* whenMillis */));
    }

    @Test
    public void isForCountryCode() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");
        assertTrue(countryTimeZones.isForCountryCode("GB"));
        assertTrue(countryTimeZones.isForCountryCode("Gb"));
        assertTrue(countryTimeZones.isForCountryCode("gB"));
    }

    @Test
    public void structuresAreImmutable() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        assertImmutableTimeZone(countryTimeZones.getDefaultTimeZone());

        List<TimeZoneMapping> timeZoneMappings = countryTimeZones.getTimeZoneMappings();
        assertEquals(1, timeZoneMappings.size());
        assertImmutableList(timeZoneMappings);

        List<TimeZoneMapping> effectiveTimeZoneMappings =
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0 /* whenMillis */);
        assertEquals(1, effectiveTimeZoneMappings.size());
        assertImmutableList(effectiveTimeZoneMappings);
    }

    @Test
    public void lookupByOffsetWithBias_oneCandidate() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        OffsetResult lonMatch = new OffsetResult(LON_TZ, true /* oneMatch */);

        // Placeholder constants to improve test case readability.
        final Boolean isDst = true;
        final Boolean notDst = false;
        final Boolean unkIsDst = null;
        final TimeZone noBias = null;
        final OffsetResult noMatch = null;

        Object[][] testCases = new Object[][] {
                // totalOffsetMillis, isDst, whenMillis, bias, expectedMatch

                // The parameters match the zone: total offset and time.
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, noBias, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, noBias, lonMatch },

                // The parameters match the zone: total offset, isDst and time.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, noBias, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, notDst, WHEN_NO_DST, noBias, lonMatch },

                // Some lookup failure cases where the total offset, isDst and time do not match the
                // zone.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, noBias, noMatch },
                { LON_DST_TOTAL_OFFSET, notDst, WHEN_NO_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, isDst, WHEN_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, noBias, noMatch },
                { LON_DST_TOTAL_OFFSET, notDst, WHEN_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, notDst, WHEN_DST, noBias, noMatch },

                // Some bias cases below.

                // The bias is irrelevant here: it matches what would be returned anyway.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, LON_TZ, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, notDst, WHEN_NO_DST, LON_TZ, lonMatch },

                // A sample of a non-matching case with bias.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, LON_TZ, noMatch },

                // The bias should be ignored: it doesn't match any of the country's zones.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, NY_TZ, lonMatch },

                // The bias should still be ignored even though it matches the offset information
                // given it doesn't match any of the country's zones.
                { NY_DST_TOTAL_OFFSET, isDst, WHEN_DST, NY_TZ, noMatch },
        };
        executeLookupByOffsetWithBiasTestCases(countryTimeZones, testCases);
    }

    @Test
    public void lookupByOffsetWithBias_multipleNonOverlappingCandidates() throws Exception {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("America/New_York", "Europe/London"), "test");

        OffsetResult lonMatch = new OffsetResult(LON_TZ, true /* oneMatch */);
        OffsetResult nyMatch = new OffsetResult(NY_TZ, true /* oneMatch */);

        // Placeholder constants to improve test case readability.
        final Boolean isDst = true;
        final Boolean notDst = false;
        final Boolean unkIsDst = null;
        final TimeZone noBias = null;
        final OffsetResult noMatch = null;

        Object[][] testCases = new Object[][] {
                // totalOffsetMillis, isDst, dstOffsetMillis, whenMillis, bias, expectedMatch

                // The parameters match the zone: total offset and time.
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, noBias, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, noBias, lonMatch },
                { NY_NO_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, noBias, nyMatch },
                { NY_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, noBias, nyMatch },

                // The parameters match the zone: total offset, isDst and time.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, noBias, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, notDst, WHEN_NO_DST, noBias, lonMatch },
                { NY_DST_TOTAL_OFFSET, isDst, WHEN_DST, noBias, nyMatch },
                { NY_NO_DST_TOTAL_OFFSET, notDst, WHEN_NO_DST, noBias, nyMatch },

                // Some lookup failure cases where the total offset, isDst and time do not match the
                // zone. This is a sample, not complete.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, noBias, noMatch },
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, noBias, noMatch },
                { LON_DST_TOTAL_OFFSET, notDst, WHEN_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, isDst, WHEN_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, noBias, noMatch },
                { LON_NO_DST_TOTAL_OFFSET, notDst, WHEN_DST, noBias, noMatch },

                // Some bias cases below.

                // The bias is irrelevant here: it matches what would be returned anyway.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, LON_TZ, lonMatch },
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, LON_TZ, lonMatch },
                { LON_NO_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, LON_TZ, lonMatch },

                // A sample of non-matching cases with bias.
                { LON_NO_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, LON_TZ, noMatch },
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_NO_DST, LON_TZ, noMatch },
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_NO_DST, LON_TZ, noMatch },

                // The bias should be ignored: it matches a zone, but the offset is wrong so
                // should not be considered a match.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, NY_TZ, lonMatch },
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, NY_TZ, lonMatch },
        };
        executeLookupByOffsetWithBiasTestCases(countryTimeZones, testCases);
    }

    // This is an artificial case very similar to America/Denver and America/Phoenix in the US: both
    // have the same offset for 6 months of the year but diverge. Australia/Lord_Howe too.
    @Test
    public void lookupByOffsetWithBias_multipleOverlappingCandidates() throws Exception {
        // Three zones that have the same offset for some of the year. Europe/London changes
        // offset WHEN_DST, the others do not.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Atlantic/Reykjavik", "Europe/London", "Etc/UTC"), "test");

        // Placeholder constants to improve test case readability.
        final Boolean isDst = true;
        final Boolean notDst = false;
        final Boolean unkIsDst = null;
        final TimeZone noBias = null;
        final OffsetResult noMatch = null;

        // This is the no-DST offset for LON_TZ, REYK_TZ. UTC_TZ.
        final int noDstTotalOffset = LON_NO_DST_TOTAL_OFFSET;
        // This is the DST offset for LON_TZ.
        final int dstTotalOffset = LON_DST_TOTAL_OFFSET;

        OffsetResult lonOnlyMatch = new OffsetResult(LON_TZ, true /* oneMatch */);
        OffsetResult lonBestMatch = new OffsetResult(LON_TZ, false /* oneMatch */);
        OffsetResult reykBestMatch = new OffsetResult(REYK_TZ, false /* oneMatch */);
        OffsetResult utcBestMatch = new OffsetResult(UTC_TZ, false /* oneMatch */);

        Object[][] testCases = new Object[][] {
                // totalOffsetMillis, isDst, dstOffsetMillis, whenMillis, bias, expectedMatch

                // The parameters match one zone: total offset and time.
                { dstTotalOffset, unkIsDst, WHEN_DST, noBias, lonOnlyMatch },
                { dstTotalOffset, unkIsDst, WHEN_DST, noBias, lonOnlyMatch },

                // The parameters match several zones: total offset and time.
                { noDstTotalOffset, unkIsDst, WHEN_NO_DST, noBias, reykBestMatch },
                { noDstTotalOffset, unkIsDst, WHEN_DST, noBias, reykBestMatch },

                // The parameters match one zone: total offset, isDst and time.
                { dstTotalOffset, isDst, WHEN_DST, noBias, lonOnlyMatch },
                { dstTotalOffset, isDst, WHEN_DST, noBias, lonOnlyMatch },

                { noDstTotalOffset, notDst, WHEN_NO_DST, noBias, reykBestMatch },
                { noDstTotalOffset, notDst, WHEN_DST, noBias, reykBestMatch },

                // Some lookup failure cases where the total offset, isDst and time do not match any
                // zone.
                { dstTotalOffset, isDst, WHEN_NO_DST, noBias, noMatch },
                { dstTotalOffset, unkIsDst, WHEN_NO_DST, noBias, noMatch },
                { noDstTotalOffset, isDst, WHEN_NO_DST, noBias, noMatch },
                { noDstTotalOffset, isDst, WHEN_DST, noBias, noMatch },

                // Some bias cases below.

                // Multiple zones match but Reykjavik is the bias.
                { noDstTotalOffset, notDst, WHEN_NO_DST, REYK_TZ, reykBestMatch },

                // Multiple zones match but London is the bias.
                { noDstTotalOffset, notDst, WHEN_NO_DST, LON_TZ, lonBestMatch },

                // Multiple zones match but UTC is the bias.
                { noDstTotalOffset, notDst, WHEN_NO_DST, UTC_TZ, utcBestMatch },

                // The bias should be ignored: it matches a zone, but the offset is wrong so
                // should not be considered a match.
                { LON_DST_TOTAL_OFFSET, isDst, WHEN_DST, REYK_TZ, lonOnlyMatch },
                { LON_DST_TOTAL_OFFSET, unkIsDst, WHEN_DST, REYK_TZ, lonOnlyMatch },
        };
        executeLookupByOffsetWithBiasTestCases(countryTimeZones, testCases);
    }

    private static void executeLookupByOffsetWithBiasTestCases(
            CountryTimeZones countryTimeZones, Object[][] testCases) {

        List<String> failures = new ArrayList<>();
        for (int i = 0; i < testCases.length; i++) {
            Object[] testCase = testCases[i];
            int totalOffsetMillis = (int) testCase[0];
            Boolean isDst = (Boolean) testCase[1];
            long whenMillis = (Long) testCase[2];
            TimeZone bias = (TimeZone) testCase[3];
            OffsetResult expectedMatch = (OffsetResult) testCase[4];

            OffsetResult actualMatch;
            if (isDst == null) {
                actualMatch = countryTimeZones.lookupByOffsetWithBias(
                        whenMillis, bias, totalOffsetMillis);
            } else {
                actualMatch = countryTimeZones.lookupByOffsetWithBias(
                        whenMillis, bias, totalOffsetMillis, isDst);
            }

            if (!offsetResultEquals(expectedMatch, actualMatch)) {
                Function<TimeZone, String> timeZoneFormatter =
                        x -> x == null ? "null" : x.getID();
                Function<OffsetResult, String> offsetResultFormatter =
                        x -> x == null ? "null"
                                : "{" + x.getTimeZone().getID() + ", " + x.isOnlyMatch() + "}";
                failures.add("Fail: case=" + i
                        + ", totalOffsetMillis=" + totalOffsetMillis
                        + ", isDst=" + isDst
                        + ", whenMillis=" + whenMillis
                        + ", bias=" + timeZoneFormatter.apply(bias)
                        + ", expectedMatch=" + offsetResultFormatter.apply(expectedMatch)
                        + ", actualMatch=" + offsetResultFormatter.apply(actualMatch)
                        + "\n");
            }
        }
        if (!failures.isEmpty()) {
            fail("Failed:\n" + failures);
        }
    }

    @Test
    public void getEffectiveTimeZonesAt_noZones() {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings(), "test");
        assertEquals(timeZoneMappings(),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0 /* whenMillis */));
        assertEquals(timeZoneMappings(),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MIN_VALUE));
        assertEquals(timeZoneMappings(),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MAX_VALUE));
    }

    @Test
    public void getEffectiveTimeZonesAt_oneZone() {
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(0));
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MIN_VALUE));
        assertEquals(timeZoneMappings("Europe/London"),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MAX_VALUE));
    }

    @Test
    public void getEffectiveTimeZonesAt_filtering() {
        TimeZoneMapping alwaysUsed = timeZoneMapping("Europe/London", null /* notUsedAfter */);

        long mappingNotUsedAfterMillis = 0L;
        TimeZoneMapping notAlwaysUsed = timeZoneMapping("Europe/Paris",
                mappingNotUsedAfterMillis /* notUsedAfter */);

        List<TimeZoneMapping> timeZoneMappings = list(alwaysUsed, notAlwaysUsed);
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings, "test");

        // Before and at mappingNotUsedAfterMillis, both mappings are "effective".
        assertEquals(list(alwaysUsed, notAlwaysUsed),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MIN_VALUE));
        assertEquals(list(alwaysUsed, notAlwaysUsed),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(mappingNotUsedAfterMillis));

        // The following should filter the second mapping because it's not "effective" after
        // mappingNotUsedAfterMillis.
        assertEquals(list(alwaysUsed),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(mappingNotUsedAfterMillis + 1));
        assertEquals(list(alwaysUsed),
                countryTimeZones.getEffectiveTimeZoneMappingsAt(Long.MAX_VALUE));
    }

    @Test
    public void hasUtcZone_everUseUtcHintOverridesZoneInformation() {
        // The country has a single zone. Europe/London uses UTC in Winter.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Etc/UTC", false /* defaultTimeZoneBoost */, false /* everUsesUtc */,
                timeZoneMappings("Etc/UTC"), "test");
        assertFalse(countryTimeZones.hasUtcZone(WHEN_DST));
        assertFalse(countryTimeZones.hasUtcZone(WHEN_NO_DST));
    }

    @Test
    public void hasUtcZone_singleZone() {
        // The country has a single zone. Europe/London uses UTC in Winter.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");
        assertFalse(countryTimeZones.hasUtcZone(WHEN_DST));
        assertTrue(countryTimeZones.hasUtcZone(WHEN_NO_DST));
    }

    @Test
    public void hasUtcZone_multipleZonesWithUtc() {
        // The country has multiple zones. Europe/London uses UTC in Winter.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "America/Los_Angeles", false /* defaultTimeZoneBoost */,
                true /* everUsesUtc */,
                timeZoneMappings("America/Los_Angeles", "America/New_York", "Europe/London"),
                "test");
        assertFalse(countryTimeZones.hasUtcZone(WHEN_DST));
        assertTrue(countryTimeZones.hasUtcZone(WHEN_NO_DST));
    }

    @Test
    public void hasUtcZone_multipleZonesWithoutUtc() {
        // The country has multiple zones, none of which use UTC.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", "Europe/Paris", false /* defaultTimeZoneBoost */, false /* everUsesUtc */,
                timeZoneMappings("America/Los_Angeles", "America/New_York", "Europe/Paris"),
                "test");
        assertFalse(countryTimeZones.hasUtcZone(WHEN_DST));
        assertFalse(countryTimeZones.hasUtcZone(WHEN_NO_DST));
    }

    @Test
    public void hasUtcZone_emptyZones() {
        // The country has no valid zones.
        CountryTimeZones countryTimeZones = CountryTimeZones.createValidated(
                "xx", INVALID_TZ_ID, false /* defaultTimeZoneBoost */, false /* everUsesUtc */,
                timeZoneMappings(INVALID_TZ_ID), "test");
        assertTrue(countryTimeZones.getTimeZoneMappings().isEmpty());
        assertFalse(countryTimeZones.hasUtcZone(WHEN_DST));
        assertFalse(countryTimeZones.hasUtcZone(WHEN_NO_DST));
    }

    @Test
    public void timeZoneMapping_getTimeZone_badZoneId() {
        TimeZoneMapping timeZoneMapping =
                TimeZoneMapping.createForTests("DOES_NOT_EXIST", true, 1234L);
        try {
            timeZoneMapping.getTimeZone();
            fail();
        } catch (RuntimeException expected) {
        }
    }

    @Test
    public void timeZoneMapping_getTimeZone_validZoneId() {
        TimeZoneMapping timeZoneMapping =
                TimeZoneMapping.createForTests("Europe/London", true, 1234L);
        TimeZone timeZone = timeZoneMapping.getTimeZone();
        assertTrue(timeZone.isFrozen());
        assertEquals("Europe/London", timeZone.getID());
    }

    private void assertImmutableTimeZone(TimeZone timeZone) {
        try {
            timeZone.setRawOffset(1000);
            fail();
        } catch (UnsupportedOperationException expected) {
        }
    }

    private static <X> void assertImmutableList(List<X> list) {
        try {
            list.add(null);
            fail();
        } catch (UnsupportedOperationException expected) {
        }
    }

    private static void assertZoneEquals(TimeZone expected, TimeZone actual) {
        // TimeZone.equals() only checks the ID, but that's ok for these tests.
        assertEquals(expected, actual);
    }

    private static boolean offsetResultEquals(OffsetResult expected, OffsetResult actual) {
        return expected == actual
                || (expected != null && actual != null
                && Objects.equals(expected.getTimeZone().getID(), actual.getTimeZone().getID())
                && expected.isOnlyMatch() == actual.isOnlyMatch());
    }

    /**
     * Creates a list of default {@link TimeZoneMapping} objects with the specified time zone IDs.
     */
    private static TimeZoneMapping timeZoneMapping(String timeZoneId, Long notUsedAfterMillis) {
        return TimeZoneMapping.createForTests(
                        timeZoneId, true /* picker */, notUsedAfterMillis);
    }

    /**
     * Creates a list of default {@link TimeZoneMapping} objects with the specified time zone IDs.
     */
    private static List<TimeZoneMapping> timeZoneMappings(String... timeZoneIds) {
        return Arrays.stream(timeZoneIds)
                .map(x -> TimeZoneMapping.createForTests(
                        x, true /* picker */, null /* notUsedAfter */))
                .collect(Collectors.toList());
    }

    private static TimeZone zone(String id) {
        return TimeZone.getFrozenTimeZone(id);
    }

    private static <X> List<X> list(X... xes) {
        return Arrays.asList(xes);
    }
}
