/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.entity;

import com.android.vts.util.ObjectifyTestBase;
import com.googlecode.objectify.Key;
import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.List;

import static com.googlecode.objectify.ObjectifyService.factory;
import static com.googlecode.objectify.ObjectifyService.ofy;
import static org.junit.Assert.assertEquals;

public class CodeCoverageFileEntityTest extends ObjectifyTestBase {

    @Test
    public void saveTest() {

        factory().register(CodeCoverageFileEntity.class);

        Key testParentKey = Key.create(TestEntity.class, "test1");
        Key testRunParentKey = Key.create(testParentKey, TestRunEntity.class, 1);

        List<Long> lineCoverage = Arrays.asList(-1L, -1L, -1L, 4L, 2L, 0L);
        CodeCoverageFileEntity codeCoverageFileEntity = new CodeCoverageFileEntity(
                10000,
                testRunParentKey,
                "audio/12.0/DevicesFile.h",
                "",
                lineCoverage,
                10020,
                40030,
                "platform/hardware/interfaces",
                "e8d6e9385a64b742ad1952c6d9");
        codeCoverageFileEntity.save();

        CodeCoverageFileEntity loadCodeCoverageFileEntity = ofy().load().type(CodeCoverageFileEntity.class).first().now();

        assertEquals(loadCodeCoverageFileEntity, codeCoverageFileEntity);
    }

}
