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

package com.android.vts.job;

import com.android.vts.entity.ApiCoverageExcludedEntity;
import com.android.vts.entity.DashboardEntity;
import com.google.api.client.auth.oauth2.Credential;
import com.google.api.client.extensions.appengine.datastore.AppEngineDataStoreFactory;
import com.google.api.client.extensions.java6.auth.oauth2.AuthorizationCodeInstalledApp;
import com.google.api.client.extensions.jetty.auth.oauth2.LocalServerReceiver;
import com.google.api.client.googleapis.auth.oauth2.GoogleAuthorizationCodeFlow;
import com.google.api.client.googleapis.auth.oauth2.GoogleClientSecrets;
import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.JsonFactory;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.services.sheets.v4.Sheets;
import com.google.api.services.sheets.v4.model.ValueRange;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.List;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;

/** Job to sync excluded API data in google spreadsheet with datastore's entity. */
public class VtsSpreadSheetSyncServlet extends BaseJobServlet {

    protected static final Logger logger =
            Logger.getLogger(VtsSpreadSheetSyncServlet.class.getName());

    private static final String APPLICATION_NAME = "VTS Dashboard";
    private static final JsonFactory JSON_FACTORY = JacksonFactory.getDefaultInstance();

    private String CREDENTIALS_KEY_FILE = "";

    /** GoogleClientSecrets for GoogleAuthorizationCodeFlow Builder */
    private GoogleClientSecrets clientSecrets;

    /** This is the ID of google spreadsheet. */
    private String SPREAD_SHEET_ID = "";

    /** This is the range to read of google spreadsheet. */
    private String SPREAD_SHEET_RANGE = "";

    @Override
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);

        try {
            CREDENTIALS_KEY_FILE = systemConfigProp.getProperty("api.coverage.keyFile");
            SPREAD_SHEET_ID = systemConfigProp.getProperty("api.coverage.spreadSheetId");
            SPREAD_SHEET_RANGE = systemConfigProp.getProperty("api.coverage.spreadSheetRange");

            InputStream keyFileInputStream =
                    this.getClass()
                            .getClassLoader()
                            .getResourceAsStream("keys/" + CREDENTIALS_KEY_FILE);
            InputStreamReader keyFileStreamReader = new InputStreamReader(keyFileInputStream);

            this.clientSecrets = GoogleClientSecrets.load(JSON_FACTORY, keyFileStreamReader);
        } catch (IOException ioe) {
            logger.log(Level.SEVERE, ioe.getMessage());
        } catch (Exception exception) {
            logger.log(Level.SEVERE, exception.getMessage());
        }
    }

    /**
     * Creates an authorized Credential object.
     *
     * @param HTTP_TRANSPORT The network HTTP Transport.
     * @param appEngineDataStoreFactory The credential will be persisted using the Google App Engine
     *     Data Store API.
     * @param SCOPES Scopes are strings that enable access to particular resources, such as user
     *     data.
     * @return An authorized Credential object.
     * @throws IOException If the credentials.json file cannot be found.
     */
    private Credential getCredentials(
            final NetHttpTransport HTTP_TRANSPORT,
            final AppEngineDataStoreFactory appEngineDataStoreFactory,
            final List<String> SCOPES)
            throws IOException {

        // Build flow and trigger user authorization request.
        GoogleAuthorizationCodeFlow flow =
                new GoogleAuthorizationCodeFlow.Builder(
                                HTTP_TRANSPORT, JSON_FACTORY, this.clientSecrets, SCOPES)
                        .setDataStoreFactory(appEngineDataStoreFactory)
                        .setAccessType("offline")
                        .build();
        LocalServerReceiver localServerReceiver = new LocalServerReceiver();
        return new AuthorizationCodeInstalledApp(flow, localServerReceiver).authorize("user");
    }

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {

        try {
            // Build a new authorized API client service.
            final NetHttpTransport HTTP_TRANSPORT = GoogleNetHttpTransport.newTrustedTransport();
            final AppEngineDataStoreFactory appEngineDataStoreFactory =
                    (AppEngineDataStoreFactory)
                            request.getServletContext().getAttribute("dataStoreFactory");
            final List<String> googleApiScopes =
                    (List<String>) request.getServletContext().getAttribute("googleApiScopes");

            Sheets service =
                    new Sheets.Builder(
                                    HTTP_TRANSPORT,
                                    JSON_FACTORY,
                                    getCredentials(
                                            HTTP_TRANSPORT,
                                            appEngineDataStoreFactory,
                                            googleApiScopes))
                            .setApplicationName(APPLICATION_NAME)
                            .build();

            ValueRange valueRange =
                    service.spreadsheets()
                            .values()
                            .get(SPREAD_SHEET_ID, SPREAD_SHEET_RANGE)
                            .execute();

            List<ApiCoverageExcludedEntity> apiCoverageExcludedEntities = new ArrayList<>();
            List<List<Object>> values = valueRange.getValues();
            if (values == null || values.isEmpty()) {
                logger.log(Level.WARNING, "No data found in google spreadsheet.");
            } else {
                for (List row : values) {
                    ApiCoverageExcludedEntity apiCoverageExcludedEntity =
                            new ApiCoverageExcludedEntity(
                                    row.get(0).toString(),
                                    row.get(1).toString(),
                                    row.get(2).toString(),
                                    row.get(3).toString(),
                                    row.get(4).toString());
                    apiCoverageExcludedEntities.add(apiCoverageExcludedEntity);
                }
            }

            DashboardEntity.saveAll(apiCoverageExcludedEntities, MAX_ENTITY_SIZE_PER_TRANSACTION);

        } catch (GeneralSecurityException gse) {
            logger.log(Level.SEVERE, gse.getMessage());
        }
    }
}
