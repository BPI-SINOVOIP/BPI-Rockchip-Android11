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

package com.android.libcore.timezone.telephonylookup;

import static com.android.libcore.timezone.testing.TestUtils.assertContains;
import static com.android.libcore.timezone.testing.TestUtils.createFile;
import static junit.framework.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.libcore.timezone.telephonylookup.proto.TelephonyLookupProtoFile;
import com.android.libcore.timezone.testing.TestUtils;
import com.google.protobuf.TextFormat;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class TelephonyLookupGeneratorTest {

    private Path tempDir;

    @Before
    public void setUp() throws Exception {
        tempDir = Files.createTempDirectory("TelephonyLookupGeneratorTest");
    }

    @After
    public void tearDown() throws Exception {
        TestUtils.deleteDir(tempDir);
    }

    @Test
    public void invalidTelephonyLookupFile() throws Exception {
        String telephonyLookupFile = createFile(tempDir, "THIS IS NOT A VALID FILE");
        String outputFile = Files.createTempFile(tempDir, "out", null /* suffix */).toString();

        TelephonyLookupGenerator telephonyLookupGenerator =
                new TelephonyLookupGenerator(telephonyLookupFile, outputFile);
        assertFalse(telephonyLookupGenerator.execute());

        assertFileIsEmpty(outputFile);
    }

    @Test
    public void upperCaseCountryIsoCodeIsRejected() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("123", "456", "GB");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void unknownCountryIsoCodeIsRejected() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("123", "456", "zx");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMccIsRejected_nonNumeric() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("XXX", "456", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMccIsRejected_tooShort() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("12", "456", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMccIsRejected_tooLong() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("1234", "567", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMncIsRejected_nonNumeric() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("123", "XXX", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMncIsRejected_tooShort() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("123", "4", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void badMncIsRejected_tooLong() throws Exception {
        TelephonyLookupProtoFile.Network network = createNetwork("123", "4567", "gb");
        checkGenerationFails(createTelephonyLookup(network));
    }

    @Test
    public void duplicateMccMncComboIsRejected() throws Exception {
        TelephonyLookupProtoFile.Network network1 = createNetwork("123", "456", "gb");
        TelephonyLookupProtoFile.Network network2 = createNetwork("123", "456", "us");
        checkGenerationFails(createTelephonyLookup(network1, network2));
    }

    @Test
    public void validDataCreatesFile() throws Exception {
        TelephonyLookupProtoFile.Network network1 = createNetwork("123", "456", "gb");
        TelephonyLookupProtoFile.Network network2 = createNetwork("123", "56", "us");
        TelephonyLookupProtoFile.TelephonyLookup telephonyLookupProto =
                createTelephonyLookup(network1, network2);

        String telephonyLookupXml = generateTelephonyLookupXml(telephonyLookupProto);
        assertContains(telephonyLookupXml,
                "<network mcc=\"123\" mnc=\"456\" country=\"gb\"/>",
                "<network mcc=\"123\" mnc=\"56\" country=\"us\"/>"
        );

    }


    private void checkGenerationFails(TelephonyLookupProtoFile.TelephonyLookup telephonyLookup2)
            throws Exception {
        TelephonyLookupProtoFile.TelephonyLookup telephonyLookup =
                telephonyLookup2;
        String telephonyLookupFile = createTelephonyLookupFile(telephonyLookup);
        String outputFile = Files.createTempFile(tempDir, "out", null /* suffix */).toString();

        TelephonyLookupGenerator telephonyLookupGenerator =
                new TelephonyLookupGenerator(telephonyLookupFile, outputFile);
        assertFalse(telephonyLookupGenerator.execute());

        assertFileIsEmpty(outputFile);
    }

    private String createTelephonyLookupFile(
            TelephonyLookupProtoFile.TelephonyLookup telephonyLookup) throws Exception {
        return TestUtils.createFile(tempDir, TextFormat.printToString(telephonyLookup));
    }

    private String generateTelephonyLookupXml(
            TelephonyLookupProtoFile.TelephonyLookup telephonyLookup) throws Exception {

        String telephonyLookupFile = createTelephonyLookupFile(telephonyLookup);

        String outputFile = Files.createTempFile(tempDir, "out", null /* suffix */).toString();

        TelephonyLookupGenerator telephonyLookupGenerator =
                new TelephonyLookupGenerator(telephonyLookupFile, outputFile);
        assertTrue(telephonyLookupGenerator.execute());

        Path outputFilePath = Paths.get(outputFile);
        assertTrue(Files.exists(outputFilePath));

        return readFileToString(outputFilePath);
    }

    private static String readFileToString(Path file) throws IOException {
        return new String(Files.readAllBytes(file), StandardCharsets.UTF_8);
    }

    private static TelephonyLookupProtoFile.Network createNetwork(String mcc, String mnc,
            String isoCountryCode) {
        return TelephonyLookupProtoFile.Network.newBuilder()
                .setMcc(mcc)
                .setMnc(mnc)
                .setCountryIsoCode(isoCountryCode)
                .build();
    }

    private static TelephonyLookupProtoFile.TelephonyLookup createTelephonyLookup(
            TelephonyLookupProtoFile.Network... networks) {
        TelephonyLookupProtoFile.TelephonyLookup.Builder builder =
                TelephonyLookupProtoFile.TelephonyLookup.newBuilder();
        for (TelephonyLookupProtoFile.Network network : networks) {
            builder.addNetworks(network);
        }
        return builder.build();
    }

    private static void assertFileIsEmpty(String outputFile) throws IOException {
        Path outputFilePath = Paths.get(outputFile);
        assertEquals(0, Files.size(outputFilePath));
    }
}
