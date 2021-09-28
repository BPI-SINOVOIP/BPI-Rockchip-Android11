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
package android.signature.cts;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;
import java.text.ParseException;

/**
 * Parses an API definition given as a text file with DEX signatures of class
 * members. Constructs a {@link DexApiDocumentParser.DexMember} for every class
 * member.
 *
 * <p>The definition file is converted into a {@link Stream} of
 * {@link DexApiDocumentParser.DexMember}.
 */
public class DexApiDocumentParser {

    // Regex patterns which match DEX signatures of methods and fields.
    // See comment by next() for more details.
    private static final Pattern REGEX_FIELD = Pattern.compile("^(L[^>]*;)->(.*):(.*)$");
    private static final Pattern REGEX_METHOD =
            Pattern.compile("^(L[^>]*;)->(.*)(\\(.*\\).*)$");

    // Estimate of the length of a line.
    private static final int LINE_LENGTH_ESTIMATE = 100;

    // Converter from String to DexMember. Cached here.
    private static final Function<String, DexMember> DEX_MEMBER_CONVERTER = str -> {
        try {
            return parseLine(str, /* lineNum= */ -1); // No line info available.
        } catch (ParseException e) {
            throw new RuntimeException(e);
        }
    };
    private static final BiFunction<String, Integer, DexMember> DEX_MEMBER_LINE_NUM_CONVERTER = (
            str, lineNum) -> {
        try {
            return parseLine(str, lineNum);
        } catch (ParseException e) {
            throw new RuntimeException(e);
        }
    };

    public Stream<DexMember> parseAsStream(Object o) {
        if (o instanceof ByteBuffer) {
            return parseAsStream((ByteBuffer) o);
        } else {
            return parseAsStream((InputStream) o);
        }
    }

    public Stream<DexMember> parseAsStream(InputStream inputStream) {
        BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
        return StreamSupport.stream(
                new BufferedReaderLineSpliterator<DexMember>(reader, DEX_MEMBER_LINE_NUM_CONVERTER),
                false);
    }

    public Stream<DexMember> parseAsStream(ByteBuffer buffer) {
        return parseAsStream(buffer, LINE_LENGTH_ESTIMATE);
    }
    public Stream<DexMember> parseAsStream(ByteBuffer buffer, int lineLengthEstimate) {
        // TODO: Ensurance that the input conforms to ByteBufferLineSpliterator requirements.
        return StreamSupport.stream(new ByteBufferLineSpliterator<DexMember>(buffer,
                lineLengthEstimate, DEX_MEMBER_CONVERTER), true);
    }

    public static DexMember parseLine(String line, int lineNum) throws ParseException {
        // Split the CSV line.
        String[] splitLine = line.split(",");
        String signature = splitLine[0];
        String[] flags = Arrays.copyOfRange(splitLine, 1, splitLine.length);

        // Match line against regex patterns.
        Matcher matchField = REGEX_FIELD.matcher(signature);
        Matcher matchMethod = REGEX_METHOD.matcher(signature);

        // Check that *exactly* one pattern matches.
        int matchCount = (matchField.matches() ? 1 : 0) + (matchMethod.matches() ? 1 : 0);
        if (matchCount == 0) {
            throw new ParseException("Could not parse: \"" + line + "\"", lineNum);
        } else if (matchCount > 1) {
            throw new ParseException("Ambiguous parse: \"" + line + "\"", lineNum);
        }

        // Extract information from the signature.
        if (matchField.matches()) {
            return new DexField(
                    matchField.group(1), matchField.group(2), matchField.group(3), flags);
        } else if (matchMethod.matches()) {
            return new DexMethod(
                    matchMethod.group(1),matchMethod.group(2), matchMethod.group(3), flags);
        }
        throw new IllegalStateException();
    }
}
