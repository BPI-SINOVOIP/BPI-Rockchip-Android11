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

import static com.android.libcore.timezone.telephonylookup.TelephonyLookupProtoFileSupport.parseTelephonyLookupTextFile;

import com.android.libcore.timezone.telephonylookup.proto.TelephonyLookupProtoFile;
import com.android.libcore.timezone.util.Errors;

import com.ibm.icu.util.ULocale;

import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.stream.Collectors;

import javax.xml.stream.XMLStreamException;

/**
 * Generates the telephonylookup.xml file using the information from telephonylookup.txt.
 *
 * See {@link #main(String[])} for commandline information.
 */
public final class TelephonyLookupGenerator {

    private final String telephonyLookupProtoFile;
    private final String outputFile;

    /**
     * Executes the generator.
     *
     * Positional arguments:
     * 1: The telephonylookup.txt proto file
     * 2: the file to generate
     */
    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println(
                    "usage: java " + TelephonyLookupGenerator.class.getName()
                            + " <input proto file> <output xml file>");
            System.exit(0);
        }
        boolean success = new TelephonyLookupGenerator(args[0], args[1]).execute();
        System.exit(success ? 0 : 1);
    }

    TelephonyLookupGenerator(String telephonyLookupProtoFile, String outputFile) {
        this.telephonyLookupProtoFile = telephonyLookupProtoFile;
        this.outputFile = outputFile;
    }

    boolean execute() throws IOException {
        // Parse the countryzones input file.
        TelephonyLookupProtoFile.TelephonyLookup telephonyLookupIn;
        try {
            telephonyLookupIn = parseTelephonyLookupTextFile(telephonyLookupProtoFile);
        } catch (ParseException e) {
            logError("Unable to parse " + telephonyLookupProtoFile, e);
            return false;
        }

        List<TelephonyLookupProtoFile.Network> networksIn = telephonyLookupIn.getNetworksList();

        Errors processingErrors = new Errors();
        processingErrors.pushScope("Validation");
        validateNetworks(networksIn, processingErrors);
        processingErrors.popScope();

        // Validation failed, so stop.
        if (processingErrors.hasFatal()) {
            logInfo("Issues:\n" + processingErrors.asString());
            return false;
        }

        TelephonyLookupXmlFile.TelephonyLookup telephonyLookupOut =
                createOutputTelephonyLookup(networksIn);
        if (!processingErrors.hasError()) {
            // Write the output structure if there wasn't an error.
            logInfo("Writing " + outputFile);
            try {
                TelephonyLookupXmlFile.write(telephonyLookupOut, outputFile);
            } catch (XMLStreamException e) {
                e.printStackTrace(System.err);
                processingErrors.addFatal("Unable to write output file");
            }
        }

        // Report all warnings / errors
        if (!processingErrors.isEmpty()) {
            logInfo("Issues:\n" + processingErrors.asString());
        }

        return !processingErrors.hasError();
    }

    private static void validateNetworks(
            List<TelephonyLookupProtoFile.Network> networksIn, Errors processingErrors) {
        Set<String> knownIsoCountries = getLowerCaseCountryIsoCodes();
        Set<String> mccMncSet = new HashSet<>();
        for (TelephonyLookupProtoFile.Network networkIn : networksIn) {
            String mcc = networkIn.getMcc();
            if (mcc.length() != 3 || !isAsciiNumeric(mcc)) {
                processingErrors.addFatal("mcc=" + mcc + " must have 3 decimal digits");
            }

            String mnc = networkIn.getMnc();
            if (!(mnc.length() == 2 || mnc.length() == 3) || !isAsciiNumeric(mnc)) {
                processingErrors.addFatal("mnc=" + mnc + " must have 2 or 3 decimal digits");
            }

            String mccMnc = "" + mcc + mnc;
            if (!mccMncSet.add(mccMnc)) {
                processingErrors.addFatal("Duplicate entry for mcc=" + mcc + ", mnc=" + mnc);
            }

            String countryIsoCode = networkIn.getCountryIsoCode();
            String countryIsoCodeLower = countryIsoCode.toLowerCase(Locale.ROOT);
            if (!countryIsoCodeLower.equals(countryIsoCode)) {
                processingErrors.addFatal("Country code not lower case: " + countryIsoCode);
            }

            if (!knownIsoCountries.contains(countryIsoCodeLower)) {
                processingErrors.addFatal("Country code not known: " + countryIsoCode);
            }
        }
    }

    private static boolean isAsciiNumeric(String string) {
        for (int i = 0; i < string.length(); i++) {
            char character = string.charAt(i);
            if (character < '0' || character > '9') {
                return false;
            }
        }
        return true;
    }

    private static Set<String> getLowerCaseCountryIsoCodes() {
        // Use ICU4J's knowledge of ISO codes because we keep that up to date.
        List<String> knownIsoCountryCodes = Arrays.asList(ULocale.getISOCountries());
        knownIsoCountryCodes = knownIsoCountryCodes.stream()
                .map(x -> x.toLowerCase(Locale.ROOT))
                .collect(Collectors.toList());
        return new HashSet<>(knownIsoCountryCodes);
    }

    private static TelephonyLookupXmlFile.TelephonyLookup createOutputTelephonyLookup(
            List<TelephonyLookupProtoFile.Network> networksIn) {
        List<TelephonyLookupXmlFile.Network> networksOut = new ArrayList<>();
        for (TelephonyLookupProtoFile.Network networkIn : networksIn) {
            String mcc = networkIn.getMcc();
            String mnc = networkIn.getMnc();
            String countryIsoCode = networkIn.getCountryIsoCode();
            TelephonyLookupXmlFile.Network networkOut =
                    new TelephonyLookupXmlFile.Network(mcc, mnc, countryIsoCode);
            networksOut.add(networkOut);
        }
        return new TelephonyLookupXmlFile.TelephonyLookup(networksOut);
    }

    private static void logError(String msg) {
        System.err.println("E: " + msg);
    }

    private static void logError(String s, Throwable e) {
        logError(s);
        e.printStackTrace(System.err);
    }

    private static void logInfo(String msg) {
        System.err.println("I: " + msg);
    }
}
