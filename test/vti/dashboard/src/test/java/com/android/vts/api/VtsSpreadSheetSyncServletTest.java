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

import com.android.vts.entity.ApiCoverageExcludedEntity;
import com.android.vts.job.VtsSpreadSheetSyncServlet;
import com.android.vts.util.ObjectifyTestBase;
import com.google.api.client.extensions.appengine.datastore.AppEngineDataStoreFactory;
import com.google.api.services.sheets.v4.SheetsScopes;
import com.google.gson.Gson;
import lombok.extern.slf4j.Slf4j;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.mockito.Mock;
import org.mockito.Spy;

import javax.servlet.ServletConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import static com.googlecode.objectify.ObjectifyService.factory;
import static com.googlecode.objectify.ObjectifyService.ofy;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

@Slf4j
public class VtsSpreadSheetSyncServletTest extends ObjectifyTestBase {

    private Gson gson;

    @Spy private VtsSpreadSheetSyncServlet servlet;

    @Mock private HttpServletRequest request;

    @Mock private HttpServletResponse response;

    @Mock private ServletContext context;

    @Mock private ServletConfig servletConfig;

    @Mock private ServletOutputStream outputStream;

    private final AppEngineDataStoreFactory DATA_STORE_FACTORY = new AppEngineDataStoreFactory();

    private final List<String> GOOGLE_API_SCOPES =
            Collections.singletonList(SheetsScopes.SPREADSHEETS_READONLY);

    /** It be executed before each @Test method */
    @BeforeEach
    void setUpExtra() {

        factory().register(ApiCoverageExcludedEntity.class);

        gson = new Gson();

        Properties systemConfigProp = new Properties();

        InputStream defaultInputStream =
                VtsSpreadSheetSyncServletTest.class
                        .getClassLoader()
                        .getResourceAsStream("config.properties");

        try {
            systemConfigProp.load(defaultInputStream);
        } catch (FileNotFoundException e) {
            log.error(e.getMessage());
        } catch (IOException e) {
            log.error(e.getMessage());
        }

        when(request.getServletContext()).thenReturn(context);
        when(request.getServletContext().getAttribute("dataStoreFactory"))
                .thenReturn(DATA_STORE_FACTORY);
        when(request.getServletContext().getAttribute("googleApiScopes"))
                .thenReturn(GOOGLE_API_SCOPES);

        when(servletConfig.getServletContext()).thenReturn(context);
        when(servletConfig.getServletContext().getAttribute("systemConfigProp"))
                .thenReturn(systemConfigProp);

    }

    @Test
    public void testSyncServletJob() throws IOException, ServletException {

        when(request.getPathInfo()).thenReturn("/cron/vts_spreadsheet_sync_job");

        when(servlet.getServletConfig()).thenReturn(servletConfig);
        when(response.getOutputStream()).thenReturn(outputStream);

        servlet.init(servletConfig);
        servlet.doGet(request, response);
        String result = outputStream.toString().trim();

        List<ApiCoverageExcludedEntity> apiCoverageExcludedEntityList =
                ofy().load().type(ApiCoverageExcludedEntity.class).list();

        assertEquals(apiCoverageExcludedEntityList.size(), 2);
        assertEquals(apiCoverageExcludedEntityList.get(0).getApiName(), "getMasterMuteTest");
        assertEquals(
                apiCoverageExcludedEntityList.get(0).getPackageName(),
                "android.hardware.audio.test");
        assertEquals(apiCoverageExcludedEntityList.get(1).getApiName(), "getMasterVolumeTest");
        assertEquals(
                apiCoverageExcludedEntityList.get(1).getPackageName(),
                "android.hardware.video.test");
    }
}
