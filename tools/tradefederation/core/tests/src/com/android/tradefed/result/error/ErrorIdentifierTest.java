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
package com.android.tradefed.result.error;

import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Unit tests for {@link ErrorIdentifier} implementations. */
@RunWith(JUnit4.class)
public class ErrorIdentifierTest {

    @Test
    public void testUniqueErrors() {
        List<ErrorIdentifier> errors = new ArrayList<>();
        errors.addAll(Arrays.asList(InfraErrorIdentifier.values()));
        errors.addAll(Arrays.asList(DeviceErrorIdentifier.values()));

        List<String> names = errors.stream().map(e -> e.name()).collect(Collectors.toList());
        Set<String> uniques = new HashSet<>();
        Set<String> duplicates =
                names.stream().filter(e -> !uniques.add(e)).collect(Collectors.toSet());

        if (!duplicates.isEmpty()) {
            fail(
                    String.format(
                            "Found duplicates names %s across our error idenfitiers", duplicates));
        }

        List<String> codes =
                errors.stream().map(e -> Long.toString(e.code())).collect(Collectors.toList());
        Set<String> uniqueCodes = new HashSet<>();
        Set<String> duplicateCodes =
                codes.stream().filter(e -> !uniqueCodes.add(e)).collect(Collectors.toSet());

        if (!duplicateCodes.isEmpty()) {
            fail(
                    String.format(
                            "Found duplicates codes %s across our error idenfitiers",
                            duplicateCodes));
        }
    }
}
