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

import static com.googlecode.objectify.ObjectifyService.factory;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

public class ApiCoverageExcludedEntityTest extends ObjectifyTestBase {

    @Test
    public void saveTest() {
        factory().register(ApiCoverageExcludedEntity.class);

        String packageName = "android.hardware.audio";
        String apiName = "createTestPatch";
        String version = "2.1";
        ApiCoverageExcludedEntity apiCoverageExcludedEntity =
                new ApiCoverageExcludedEntity(
                        packageName, version, "IDevice", apiName, "not testable");
        apiCoverageExcludedEntity.save();

        assertEquals(apiCoverageExcludedEntity.getPackageName(), packageName);
        assertEquals(apiCoverageExcludedEntity.getApiName(), apiName);
        assertEquals(apiCoverageExcludedEntity.getMajorVersion(), 2);
        assertEquals(apiCoverageExcludedEntity.getMinorVersion(), 1);
    }

    @Test
    public void getUrlSafeKeyTest() {
        factory().register(CodeCoverageEntity.class);
        factory().register(ApiCoverageExcludedEntity.class);

        Key testParentKey = Key.create(TestEntity.class, "test1");
        Key testRunParentKey = Key.create(testParentKey, TestRunEntity.class, 1);

        CodeCoverageEntity codeCoverageEntity =
                new CodeCoverageEntity(testRunParentKey, 1000, 3500);
        codeCoverageEntity.save();

        String urlKey =
                "kind%3A+%22Test%22%0A++name%3A+%22test1%22%0A%7D%0Apath+%7B%0A++kind%3A+%22TestRun%22%0A++id%3A+1%0A%7D%0Apath+%7B%0A++kind%3A+%22CodeCoverage%22%0A++id%3A+1%0A%7D%0A";
        assertTrue(codeCoverageEntity.getUrlSafeKey().endsWith(urlKey));
    }
}
