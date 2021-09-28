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
import org.junit.runners.JUnit4;

import android.icu.testsharding.MainTestShard;
import android.icu.text.Transliterator;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

/**
 * Ensure that Android devices are compatible and provides a consistent set
 * of translieration ids. Thus, Android app can use the transliteration ids
 * across all Android devices.
 */
@MainTestShard
@RunWith(JUnit4.class)
public class AndroidTransliteratorAvailableIdsTest {

    @Test
    public void testGetAvailableIDs_containingAllExpectedIds() throws IOException {
        assertContainingAllExpectedIds("expected_transliteration_id_list.txt",
                Collections.emptyList());
    }

    /**
     * Test that the current set of transliteration ids is backwards compatible, i.e.
     * contains all ids when Transliteration API is first exposed as public Android API
     * in Android Q.
     * If a breaking change has to be made, i.e. a missing transliteration id is expected,
     * please add the id into the exclusion list.
     */
    @Test
    public void testGetAvailableIDs_containingAllExpectedIdsInFirstRelease() throws IOException {
        assertContainingAllExpectedIds("expected_transliteration_id_list_release_1.txt",
                Arrays.asList(
                        /* Deliberately empty: No missing Id. */
                ));
    }

    private void assertContainingAllExpectedIds(String resourceFileName,
            List<String> excludedIds) throws IOException {
        List<String> expectedIds = new ArrayList<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                AndroidIcuVersionTest.class.getResourceAsStream(resourceFileName)))) {
            String line;
            while((line = reader.readLine()) != null) {
                if (!line.isEmpty()) {
                    expectedIds.add(line);
                }
            }
        }

        expectedIds.removeAll(excludedIds);

        // Sanity check. The list size shouldn't be 0.
        assertNotEquals("At least one transliteration id is expected",
                0, expectedIds.size());

        Set<String> actualIds = getActualIds();
        for (String expectedId : expectedIds) {
            assertTrue("The list returned by Transliterator.getAvailableIDs() has "
                    + actualIds.size() + " elements, but does not contain Id: " + expectedId,
                    actualIds.contains(expectedId));
        }
    }

    private static Set<String> getActualIds() {
        Enumeration<String> enumerationOfids = Transliterator.getAvailableIDs();
        Set<String> actualIds = new HashSet<>();
        actualIds.addAll(Collections.list(enumerationOfids));
        return actualIds;
    }
}
