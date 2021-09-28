/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import libcore.timezone.TelephonyLookup;
import libcore.timezone.TelephonyNetwork;
import libcore.timezone.TelephonyNetworkFinder;

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
import java.util.Collection;
import java.util.List;

public class TelephonyLookupTest {

    private Path testDir;

    @Before
    public void setUp() throws Exception {
        testDir = Files.createTempDirectory("TelephonyLookupTest");
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
        String validXml1 = "<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n";
        TelephonyNetwork expectedTelephonyNetwork1 =
                TelephonyNetwork.create("123", "456", "gb");

        String validXml2 = "<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"234\" mnc=\"567\" country=\"fr\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n";
        TelephonyNetwork expectedTelephonyNetwork2 =
                TelephonyNetwork.create("234", "567", "fr");

        String invalidXml = "<foo></foo>\n";
        checkValidateThrowsParserException(invalidXml);

        String validFile1 = createFile(validXml1);
        String validFile2 = createFile(validXml2);
        String invalidFile = createFile(invalidXml);
        String missingFile = createMissingFile();

        TelephonyLookup file1ThenFile2 =
                TelephonyLookup.createInstanceWithFallback(validFile1, validFile2);
        assertEquals(list(expectedTelephonyNetwork1),
                file1ThenFile2.getTelephonyNetworkFinder().getAll());

        TelephonyLookup missingFileThenFile1 =
                TelephonyLookup.createInstanceWithFallback(missingFile, validFile1);
        assertEquals(list(expectedTelephonyNetwork1),
                missingFileThenFile1.getTelephonyNetworkFinder().getAll());

        TelephonyLookup file2ThenFile1 =
                TelephonyLookup.createInstanceWithFallback(validFile2, validFile1);
        assertEquals(list(expectedTelephonyNetwork2),
                file2ThenFile1.getTelephonyNetworkFinder().getAll());

        // We assume the file has been validated so an invalid file is not checked ahead of time.
        // We will find out when we look something up.
        TelephonyLookup invalidThenValid =
                TelephonyLookup.createInstanceWithFallback(invalidFile, validFile1);
        assertNull(invalidThenValid.getTelephonyNetworkFinder());

        // This is not a normal case: It would imply a device shipped without a file anywhere!
        TelephonyLookup missingFiles =
                TelephonyLookup.createInstanceWithFallback(missingFile, missingFile);
        assertEmpty(missingFiles.getTelephonyNetworkFinder().getAll());
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
    public void xmlParsing_missingNetworks() throws Exception {
        checkValidateThrowsParserException("<telephony_lookup></telephony_lookup>\n");
    }

    @Test
    public void xmlParsing_emptyNetworksOk() throws Exception {
        {
            TelephonyLookup telephonyLookup =
                    validate("<telephony_lookup>\n"
                            + "  <networks>\n"
                            + "  </networks>\n"
                            + "</telephony_lookup>\n");
            TelephonyNetworkFinder telephonyNetworkFinder = telephonyLookup
                    .getTelephonyNetworkFinder();
            assertEquals(list(), telephonyNetworkFinder.getAll());
        }
        {
            TelephonyLookup telephonyLookup =
                    validate("<telephony_lookup>\n"
                            + "  <networks/>\n"
                            + "</telephony_lookup>\n");
            TelephonyNetworkFinder telephonyNetworkFinder = telephonyLookup
                    .getTelephonyNetworkFinder();
            assertEquals(list(), telephonyNetworkFinder.getAll());
        }
    }

    @Test
    public void xmlParsing_unexpectedComments() throws Exception {
        TelephonyNetwork expectedTelephonyNetwork =
                TelephonyNetwork.create("123", "456", "gb");

        TelephonyLookup telephonyLookup = validate("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <!-- This is a comment -->"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertEquals(list(expectedTelephonyNetwork), telephonyLookup.getTelephonyNetworkFinder().getAll());
    }

    @Test
    public void xmlParsing_unexpectedElementsIgnored() throws Exception {
        TelephonyNetwork expectedTelephonyNetwork =
                TelephonyNetwork.create("123", "456", "gb");
        List<TelephonyNetwork> expectedNetworks = list(expectedTelephonyNetwork);

        String unexpectedElement = "<unexpected-element>\n<a /></unexpected-element>\n";

        // These tests are important because they ensure we can extend the format in future with
        // more information but could continue using the same file on older devices.
        TelephonyLookup telephonyLookup = validate("<telephony_lookup>\n"
                + " " + unexpectedElement
                + "  <networks>\n"
                + "    " + unexpectedElement
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "    " + unexpectedElement
                + "  </networks>\n"
                + "  " + unexpectedElement
                + "</telephony_lookup>\n");
        assertEquals(expectedNetworks, telephonyLookup.getTelephonyNetworkFinder().getAll());

        telephonyLookup = validate("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\">\n"
                + "    " + unexpectedElement
                + "    </network>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertEquals(expectedNetworks, telephonyLookup.getTelephonyNetworkFinder().getAll());

        expectedNetworks = list(expectedTelephonyNetwork,
                TelephonyNetwork.create("234", "567", "fr"));
        telephonyLookup = validate("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "    " + unexpectedElement
                + "    <network mcc=\"234\" mnc=\"567\" country=\"fr\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertEquals(expectedNetworks, telephonyLookup.getTelephonyNetworkFinder().getAll());
    }

    @Test
    public void xmlParsing_unexpectedTextIgnored() throws Exception {
        TelephonyNetwork expectedTelephonyNetwork =
                TelephonyNetwork.create("123", "456", "gb");
        List<TelephonyNetwork> expectedNetworks = list(expectedTelephonyNetwork);

        String unexpectedText = "unexpected-text";
        TelephonyLookup telephonyLookup = validate("<telephony_lookup>\n"
                + "  " + unexpectedText
                + "  <networks>\n"
                + "  " + unexpectedText
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "    " + unexpectedText
                + "  </networks>\n"
                + "  " + unexpectedText
                + "</telephony_lookup>\n");
        assertEquals(expectedNetworks, telephonyLookup.getTelephonyNetworkFinder().getAll());

        telephonyLookup = validate("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\">\n"
                + "      " + unexpectedText
                + "    </network>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertEquals(expectedNetworks, telephonyLookup.getTelephonyNetworkFinder().getAll());
    }

    @Test
    public void xmlParsing_truncatedInput() throws Exception {
        checkValidateThrowsParserException("<telephony_lookup>\n");

        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n");

        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n");

        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "  </networks>\n");
    }

    @Test
    public void validateDuplicateMccMnc() {
        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" countryCode=\"gb\"/>\n"
                + "    <network mcc=\"123\" mnc=\"456\" countryCode=\"fr\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
    }

    @Test
    public void validateCountryCodeLowerCase() {
        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" countryCode=\"GB\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
    }


    @Test
    public void getTelephonyNetworkFinder() throws Exception {
        TelephonyLookup telephonyLookup = TelephonyLookup.createInstanceForTests(
                "<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\" country=\"gb\"/>\n"
                + "    <network mcc=\"234\" mnc=\"567\" country=\"fr\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");

        TelephonyNetworkFinder telephonyNetworkFinder = telephonyLookup.getTelephonyNetworkFinder();
        TelephonyNetwork expectedNetwork1 = TelephonyNetwork.create("123", "456", "gb");
        TelephonyNetwork expectedNetwork2 = TelephonyNetwork.create("234", "567", "fr");
        assertEquals(list(expectedNetwork1, expectedNetwork2), telephonyNetworkFinder.getAll());
        assertEquals(expectedNetwork1, telephonyNetworkFinder.findNetworkByMccMnc("123", "456"));
        assertEquals(expectedNetwork2, telephonyNetworkFinder.findNetworkByMccMnc("234", "567"));
        assertNull(telephonyNetworkFinder.findNetworkByMccMnc("999", "999"));
    }

    @Test
    public void xmlParsing_missingMccAttribute() throws Exception {
        checkValidateThrowsParserException("<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mnc=\"456\" country=\"gb\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
    }

    @Test
    public void xmlParsing_missingMncAttribute() throws Exception {
        TelephonyLookup telephonyLookup = TelephonyLookup.createInstanceForTests(
                "<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" country=\"gb\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertNull(telephonyLookup.getTelephonyNetworkFinder());
    }

    @Test
    public void xmlParsing_missingCountryCodeAttribute() throws Exception {
        TelephonyLookup telephonyLookup = TelephonyLookup.createInstanceForTests(
                "<telephony_lookup>\n"
                + "  <networks>\n"
                + "    <network mcc=\"123\" mnc=\"456\"/>\n"
                + "  </networks>\n"
                + "</telephony_lookup>\n");
        assertNull(telephonyLookup.getTelephonyNetworkFinder());
    }

    private static void checkValidateThrowsParserException(String xml) {
        try {
            validate(xml);
            fail();
        } catch (IOException expected) {
        }
    }

    private static TelephonyLookup validate(String xml) throws IOException {
        TelephonyLookup telephonyLookup = TelephonyLookup.createInstanceForTests(xml);
        telephonyLookup.validate();
        return telephonyLookup;
    }

    private static void assertEmpty(Collection<?> collection) {
        assertTrue("Expected empty:" + collection, collection.isEmpty());
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
