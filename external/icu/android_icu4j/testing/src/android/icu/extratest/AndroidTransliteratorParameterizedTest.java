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
package android.icu.extratest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import android.icu.testsharding.MainTestShard;
import android.icu.text.Transliterator;
import android.icu.text.UnicodeSet;

import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.stream.Collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

/**
 * Sanity test to ensure that {@link Transliterator#transliterate(String)} transliterate
 * some strings successfully.
 */
@MainTestShard
@RunWith(Parameterized.class)
public class AndroidTransliteratorParameterizedTest {
    private final String id;

    public AndroidTransliteratorParameterizedTest(String id) {
        this.id = id;
    }

    @Parameterized.Parameters
    public static List<String> getIds() {
        return Collections.list(Transliterator.getAvailableIDs())
                .stream()
                // "Any-Null" is special. Empty source set and its inverse is itself.
                .filter((s) -> !"Any-Null".equals(s))
                .collect(Collectors.toList());
    }

    @Test
    public void testTranslierateASourceCharacter() {
        String testId = id;
        Transliterator transliterator = Transliterator.getInstance(id);
        UnicodeSet sourceSet = transliterator.getSourceSet();

        if (sourceSet.size() == 0) {
            // Example id: ASCII-Latin

            // If the source set is empty, transliteration should have no effect on output String
            String input = "abc123";
            assertEquals("The transliterated should not change", input,
                    transliterator.transliterate(input));

            // Continue the test with its inverse
            // If the source set is empty, the source set of its inverse should not be empty.
            // There should be no null-null transliterator in the system.
            transliterator = transliterator.getInverse();
            testId = transliterator.getID();
            sourceSet = transliterator.getSourceSet();
            assertNotEquals("[" + id + "] The source set of the inverse is empty.",
                    0, sourceSet.size());
        }
        // Ensure the result is the union of the source string and target set.
        int exampleCodePoint = sourceSet.charAt(0);
        String sourceString = new String(Character.toChars(exampleCodePoint));
        String targetString = transliterator.transform(sourceString);
        UnicodeSet resultUnicodeSet = new UnicodeSet()
                .addAll(transliterator.getTargetSet())
                .add(exampleCodePoint);
        assertTrue("[" + id + "] Target set does not contain all characters in the "
                        + "target string: " + targetString
                        + "\nSourceString: " + sourceString
                        + "\nid: " + testId,
                resultUnicodeSet.containsAll(targetString));

    }
}
