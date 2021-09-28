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

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import libcore.timezone.CountryTimeZones;
import libcore.timezone.CountryTimeZones.TimeZoneMapping;
import libcore.timezone.CountryZonesFinder;
import libcore.timezone.TimeZoneFinder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class TimeZoneFinderTest {

    private Path testDir;

    @Before
    public void setUp() throws Exception {
        testDir = Files.createTempDirectory("TimeZoneFinderTest");
    }

    @After
    public void tearDown() throws Exception {
        // Delete the testDir and all contents.
        Files.walkFileTree(testDir, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs)
                    throws IOException {
                Files.delete(file);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc)
                    throws IOException {
                Files.delete(dir);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    @Test
    public void createInstanceWithFallback() throws Exception {
        String validXml1 = "<timezones ianaversion=\"2017c\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n";
        CountryTimeZones expectedCountryTimeZones1 = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        String validXml2 = "<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/Paris\" everutc=\"n\">\n"
                + "      <id>Europe/Paris</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n";
        CountryTimeZones expectedCountryTimeZones2 = CountryTimeZones.createValidated(
                "gb", "Europe/Paris", false /* defaultTimeZoneBoost */, false /* everUsesUtc */,
                timeZoneMappings("Europe/Paris"), "test");

        String invalidXml = "<foo></foo>\n";
        checkValidateThrowsParserException(invalidXml);

        String validFile1 = createFile(validXml1);
        String validFile2 = createFile(validXml2);
        String invalidFile = createFile(invalidXml);
        String missingFile = createMissingFile();

        TimeZoneFinder file1ThenFile2 =
                TimeZoneFinder.createInstanceWithFallback(validFile1, validFile2);
        assertEquals("2017c", file1ThenFile2.getIanaVersion());
        assertEquals(expectedCountryTimeZones1, file1ThenFile2.lookupCountryTimeZones("gb"));

        TimeZoneFinder missingFileThenFile1 =
                TimeZoneFinder.createInstanceWithFallback(missingFile, validFile1);
        assertEquals("2017c", missingFileThenFile1.getIanaVersion());
        assertEquals(expectedCountryTimeZones1, missingFileThenFile1.lookupCountryTimeZones("gb"));

        TimeZoneFinder file2ThenFile1 =
                TimeZoneFinder.createInstanceWithFallback(validFile2, validFile1);
        assertEquals("2017b", file2ThenFile1.getIanaVersion());
        assertEquals(expectedCountryTimeZones2, file2ThenFile1.lookupCountryTimeZones("gb"));

        // We assume the file has been validated so an invalid file is not checked ahead of time.
        // We will find out when we look something up.
        TimeZoneFinder invalidThenValid =
                TimeZoneFinder.createInstanceWithFallback(invalidFile, validFile1);
        assertNull(invalidThenValid.getIanaVersion());
        assertNull(invalidThenValid.lookupCountryTimeZones("gb"));

        // This is not a normal case: It would imply a device shipped without a file anywhere!
        TimeZoneFinder missingFiles =
                TimeZoneFinder.createInstanceWithFallback(missingFile, missingFile);
        assertNull(missingFiles.getIanaVersion());
        assertNull(missingFiles.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_emptyFile() throws Exception {
        checkValidateThrowsParserException("");
    }

    @Test
    public void xmlParsing_unexpectedRootElement() throws Exception {
        checkValidateThrowsParserException("<foo></foo>\n");
    }

    @Test
    public void xmlParsing_missingCountryZones() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\"></timezones>\n");
    }

    @Test
    public void xmlParsing_noCountriesOk() throws Exception {
        validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_unexpectedComments() throws Exception {
        CountryTimeZones expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <!-- This is a comment -->"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        // This is a crazy comment, but also helps prove that TEXT nodes are coalesced by the
        // parser.
        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/<!-- Don't freak out! -->London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_unexpectedElementsIgnored() throws Exception {
        CountryTimeZones expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        String unexpectedElement = "<unexpected-element>\n<a /></unexpected-element>\n";
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  " + unexpectedElement
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    " + unexpectedElement
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      " + unexpectedElement
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "    " + unexpectedElement
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        // This test is important because it ensures we can extend the format in future with
        // more information.
        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "  " + unexpectedElement
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London", "Europe/Paris"), "test");
        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "      " + unexpectedElement
                + "      <id>Europe/Paris</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_unexpectedTextIgnored() throws Exception {
        CountryTimeZones expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        String unexpectedText = "unexpected-text";
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  " + unexpectedText
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    " + unexpectedText
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      " + unexpectedText
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));

        expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London", "Europe/Paris"), "test");
        finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "      " + unexpectedText
                + "      <id>Europe/Paris</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_truncatedInput() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n");

        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n");

        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n");

        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n");

        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n");

        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n");
    }

    @Test
    public void xmlParsing_unexpectedChildInTimeZoneIdThrows() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id><unexpected-element /></id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_unknownTimeZoneIdIgnored() throws Exception {
        CountryTimeZones expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Unknown_Id</id>\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_missingCountryCode() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_missingCountryEverUtc() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_badCountryEverUtc() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"occasionally\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_missingCountryDefault() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_badCountryDefaultBoost() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" defaultBoost=\"nope\""
                + "everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_countryDefaultBoost() throws Exception {
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" defaultBoost=\"y\""
                + "everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryTimeZones countryTimeZones = finder.lookupCountryTimeZones("gb");
        assertTrue(countryTimeZones.isDefaultTimeZoneBoosted());
    }

    @Test
    public void xmlParsing_badTimeZoneMappingPicker() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id picker=\"sometimes\">Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_timeZoneMappingPicker() throws Exception {
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"us\" default=\"America/New_York\" everutc=\"n\">\n"
                + "      <!-- Explicit picker=\"y\" -->\n"
                + "      <id picker=\"y\">America/New_York</id>\n"
                + "      <!-- Implicit picker=\"y\" -->\n"
                + "      <id>America/Los_Angeles</id>\n"
                + "      <!-- Explicit picker=\"n\" -->\n"
                + "      <id picker=\"n\">America/Indiana/Vincennes</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryTimeZones usTimeZones = finder.lookupCountryTimeZones("us");
        List<TimeZoneMapping> actualTimeZoneMappings = usTimeZones.getTimeZoneMappings();
        List<TimeZoneMapping> expectedTimeZoneMappings = list(
                TimeZoneMapping.createForTests(
                        "America/New_York", true /* shownInPicker */, null /* notUsedAfter */),
                TimeZoneMapping.createForTests(
                        "America/Los_Angeles", true /* shownInPicker */, null /* notUsedAfter */),
                TimeZoneMapping.createForTests(
                        "America/Indiana/Vincennes", false /* shownInPicker */,
                        null /* notUsedAfter */)
        );
        assertEquals(expectedTimeZoneMappings, actualTimeZoneMappings);
    }

    @Test
    public void xmlParsing_badTimeZoneMappingNotAfter() throws Exception {
        checkValidateThrowsParserException("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id notafter=\"sometimes\">Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
    }

    @Test
    public void xmlParsing_timeZoneMappingNotAfter() throws Exception {
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"us\" default=\"America/New_York\" everutc=\"n\">\n"
                + "      <!-- Explicit notafter -->\n"
                + "      <id notafter=\"1234\">America/New_York</id>\n"
                + "      <!-- Missing notafter -->\n"
                + "      <id>America/Indiana/Vincennes</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryTimeZones usTimeZones = finder.lookupCountryTimeZones("us");
        List<TimeZoneMapping> actualTimeZoneMappings = usTimeZones.getTimeZoneMappings();
        List<TimeZoneMapping> expectedTimeZoneMappings = list(
                TimeZoneMapping.createForTests(
                        "America/New_York", true /* shownInPicker */, 1234L /* notUsedAfter */),
                TimeZoneMapping.createForTests(
                        "America/Indiana/Vincennes", true /* shownInPicker */,
                        null /* notUsedAfter */)
        );
        assertEquals(expectedTimeZoneMappings, actualTimeZoneMappings);
    }

    @Test
    public void getCountryZonesFinder() throws Exception {
        TimeZoneFinder timeZoneFinder = TimeZoneFinder.createInstanceForTests(
                "<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "    <country code=\"fr\" default=\"Europe/Paris\" everutc=\"y\">\n"
                + "      <id>Europe/Paris</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryTimeZones expectedGb = CountryTimeZones.createValidated("gb", "Europe/London",
                false /* defaultTimeZoneBoost */, true, timeZoneMappings("Europe/London"), "test");
        CountryTimeZones expectedFr = CountryTimeZones.createValidated("fr", "Europe/Paris",
                false /* defaultTimeZoneBoost */, true, timeZoneMappings("Europe/Paris"), "test");
        CountryZonesFinder countryZonesFinder = timeZoneFinder.getCountryZonesFinder();
        assertEquals(list("gb", "fr"), countryZonesFinder.lookupAllCountryIsoCodes());
        assertEquals(expectedGb, countryZonesFinder.lookupCountryTimeZones("gb"));
        assertEquals(expectedFr, countryZonesFinder.lookupCountryTimeZones("fr"));
        assertNull(countryZonesFinder.lookupCountryTimeZones("DOES_NOT_EXIST"));
    }

    @Test
    public void getCountryZonesFinder_empty() throws Exception {
        TimeZoneFinder timeZoneFinder = TimeZoneFinder.createInstanceForTests(
                "<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryZonesFinder countryZonesFinder = timeZoneFinder.getCountryZonesFinder();
        assertEquals(list(), countryZonesFinder.lookupAllCountryIsoCodes());
    }

    @Test
    public void getCountryZonesFinder_invalid() throws Exception {
        TimeZoneFinder timeZoneFinder = TimeZoneFinder.createInstanceForTests(
                "<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "    <!-- Missing required attributes! -->\n"
                + "    <country code=\"fr\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertNull(timeZoneFinder.getCountryZonesFinder());
    }

    @Test
    public void lookupCountryTimeZones_caseInsensitive() throws Exception {
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        CountryTimeZones expectedCountryTimeZones = CountryTimeZones.createValidated(
                "gb", "Europe/London", false /* defaultTimeZoneBoost */, true /* everUsesUtc */,
                timeZoneMappings("Europe/London"), "test");

        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("gb"));
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("GB"));
        assertEquals(expectedCountryTimeZones, finder.lookupCountryTimeZones("Gb"));
    }

    @Test
    public void lookupCountryTimeZones_unknownCountryReturnsNull() throws Exception {
        TimeZoneFinder finder = validate("<timezones ianaversion=\"2017b\">\n"
                + "  <countryzones>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertNull(finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void xmlParsing_missingIanaVersionAttribute() throws Exception {
        // The <timezones> element will typically have an ianaversion attribute, but it's not
        // required for parsing.
        TimeZoneFinder finder = validate("<timezones>\n"
                + "  <countryzones>\n"
                + "    <country code=\"gb\" default=\"Europe/London\" everutc=\"y\">\n"
                + "      <id>Europe/London</id>\n"
                + "    </country>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertNull(finder.getIanaVersion());

        assertNotNull(finder.lookupCountryTimeZones("gb"));
    }

    @Test
    public void getIanaVersion() throws Exception {
        final String expectedIanaVersion = "2017b";

        TimeZoneFinder finder = validate("<timezones ianaversion=\"" + expectedIanaVersion + "\">\n"
                + "  <countryzones>\n"
                + "  </countryzones>\n"
                + "</timezones>\n");
        assertEquals(expectedIanaVersion, finder.getIanaVersion());
    }

    private static void checkValidateThrowsParserException(String xml) {
        try {
            validate(xml);
            fail();
        } catch (IOException expected) {
        }
    }

    private static TimeZoneFinder validate(String xml) throws IOException {
        TimeZoneFinder timeZoneFinder = TimeZoneFinder.createInstanceForTests(xml);
        timeZoneFinder.validate();
        return timeZoneFinder;
    }

    /**
     * Creates a list of default {@link TimeZoneMapping} objects with the specified time zone IDs.
     */
    private static List<TimeZoneMapping> timeZoneMappings(String... timeZoneIds) {
        return Arrays.stream(timeZoneIds)
                .map(x -> TimeZoneMapping.createForTests(
                        x, true /* showInPicker */, null /* notUsedAfter */))
                .collect(Collectors.toList());
    }

    private static <X> List<X> list(X... values) {
        return Arrays.asList(values);
    }

    private String createFile(String fileContent) throws IOException {
        Path filePath = Files.createTempFile(testDir, null, null);
        Files.write(filePath, fileContent.getBytes(StandardCharsets.UTF_8));
        return filePath.toString();
    }

    private String createMissingFile() throws IOException {
        Path filePath = Files.createTempFile(testDir, null, null);
        Files.delete(filePath);
        return filePath.toString();
    }
}
