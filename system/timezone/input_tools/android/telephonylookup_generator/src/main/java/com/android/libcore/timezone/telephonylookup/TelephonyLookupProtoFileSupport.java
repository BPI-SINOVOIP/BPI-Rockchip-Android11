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

import com.android.libcore.timezone.telephonylookup.proto.TelephonyLookupProtoFile;
import com.google.protobuf.TextFormat;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.text.ParseException;

/**
 * A class containing utility methods for dealing with TelephonyLookupProtoFile objects.
 */
public final class TelephonyLookupProtoFileSupport {

    private TelephonyLookupProtoFileSupport() {}

    public static TelephonyLookupProtoFile.TelephonyLookup parseTelephonyLookupTextFile(String file)
            throws IOException, ParseException {
        try (BufferedReader fileReader = new BufferedReader(new FileReader(file))) {
            TelephonyLookupProtoFile.TelephonyLookup.Builder builder =
                    TelephonyLookupProtoFile.TelephonyLookup.newBuilder();
            TextFormat.getParser().merge(fileReader, builder);
            return builder.build();
        } catch (TextFormat.ParseException e) {
            ParseException e2 = new ParseException("Error reading proto file: " + file, 0);
            e2.initCause(e);
            throw e2;
        }
    }
}
