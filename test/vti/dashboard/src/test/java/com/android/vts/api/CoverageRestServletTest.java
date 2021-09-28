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

package com.android.vts.api;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;
import static com.googlecode.objectify.ObjectifyService.factory;

import com.android.vts.entity.ApiCoverageEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.util.ObjectifyTestBase;
import com.google.gson.Gson;
import com.google.gson.internal.LinkedTreeMap;
import com.googlecode.objectify.Key;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

import java.util.Arrays;
import java.util.List;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.BeforeEach;
import org.mockito.Mock;

public class CoverageRestServletTest extends ObjectifyTestBase {

    private Gson gson;

    @Mock
    private HttpServletRequest request;

    @Mock
    private HttpServletResponse response;

    /** It be executed before each @Test method */
    @BeforeEach
    void setUpExtra() {
        gson = new Gson();

        /********
        System.getenv().forEach((k,v) -> {
            System.out.println("key => " + k);
            System.out.println("value => " + v);
        });
         *********/
    }

    @Test
    public void testApiData() throws IOException, ServletException {

        factory().register(ApiCoverageEntity.class);

        List<String> halApi = Arrays.asList("allocate", "dumpDebugInfo");
        List<String> coveredHalApi = Arrays.asList("allocate", "dumpDebugInfo");

        Key testParentKey = Key.create(TestEntity.class, "test1");
        Key testRunParentKey = Key.create(testParentKey, TestRunEntity.class, 1);
        ApiCoverageEntity apiCoverageEntity =
                new ApiCoverageEntity(
                        testRunParentKey,
                        "android.hardware.graphics.allocator",
                        4,
                        1,
                        "IAllocator",
                        halApi,
                        coveredHalApi);
        apiCoverageEntity.save();

        String key = apiCoverageEntity.getUrlSafeKey();

        when(request.getPathInfo()).thenReturn("/api/data");

        when(request.getParameter("key")).thenReturn(key);

        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);

        when(response.getWriter()).thenReturn(pw);

        CoverageRestServlet coverageRestServlet = new CoverageRestServlet();
        coverageRestServlet.doGet(request, response);
        String result = sw.getBuffer().toString().trim();

        LinkedTreeMap resultMap = gson.fromJson(result, LinkedTreeMap.class);

        assertEquals(resultMap.get("halInterfaceName"), "IAllocator");
        assertEquals(resultMap.get("halPackageName"), "android.hardware.graphics.allocator");
        assertEquals(resultMap.get("halApi"), Arrays.asList("allocate", "dumpDebugInfo"));
        assertEquals(resultMap.get("coveredHalApi"), Arrays.asList("allocate", "dumpDebugInfo"));

    }

}
