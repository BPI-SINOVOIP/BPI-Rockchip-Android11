/*
 * Copyright (c) 2018 Google Inc. All Rights Reserved.
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

import com.android.vts.entity.TestSuiteFileEntity;
import com.android.vts.entity.TestSuiteResultEntity;
import com.android.vts.proto.TestSuiteResultMessageProto;
import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.jackson.JacksonFactory;
import com.google.api.services.oauth2.Oauth2;
import com.google.api.services.oauth2.model.Tokeninfo;
import com.google.gson.Gson;
import com.googlecode.objectify.Key;
import org.apache.commons.codec.binary.Base64;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;

/** REST endpoint for posting test suite data to the Dashboard. */
public class TestSuiteResultRestServlet extends BaseApiServlet {
    private static String SERVICE_CLIENT_ID;
    private static final String SERVICE_NAME = "VTS Dashboard";
    private static final Logger logger =
            Logger.getLogger(TestSuiteResultRestServlet.class.getName());

    @Override
    public void init(ServletConfig cfg) throws ServletException {
        super.init(cfg);

        SERVICE_CLIENT_ID = systemConfigProp.getProperty("appengine.serviceClientID");
    }

    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        // Retrieve the params
        TestSuiteResultMessageProto.TestSuiteResultMessage testSuiteResultMessage;
        try {
            String payload = request.getReader().lines().collect(Collectors.joining());
            byte[] value = Base64.decodeBase64(payload);
            testSuiteResultMessage =
                    TestSuiteResultMessageProto.TestSuiteResultMessage.parseFrom(value);
        } catch (IOException e) {
            response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
            logger.log(Level.WARNING, "Invalid proto: " + e.getLocalizedMessage());
            return;
        }

        Map<String, Object> resultMap = new HashMap<>();
        // Verify service account access token.
        if (testSuiteResultMessage.hasAccessToken()) {
            String accessToken = testSuiteResultMessage.getAccessToken();
            logger.log(Level.INFO, "accessToken => " + accessToken);
            GoogleCredential credential = new GoogleCredential().setAccessToken(accessToken);
            Oauth2 oauth2 =
                    new Oauth2.Builder(new NetHttpTransport(), new JacksonFactory(), credential)
                            .setApplicationName(SERVICE_NAME)
                            .build();
            Tokeninfo tokenInfo = oauth2.tokeninfo().setAccessToken(accessToken).execute();

            if (tokenInfo.getIssuedTo().equals(SERVICE_CLIENT_ID)) {
                String filePath = "suite_result/2019/04/06/132343.bin";
                Key<TestSuiteFileEntity> testSuiteFileParent =
                        Key.create(TestSuiteFileEntity.class, filePath);
                TestSuiteFileEntity newTestSuiteFileEntity = new TestSuiteFileEntity(filePath);
                TestSuiteResultEntity testSuiteResultEntity =
                        new TestSuiteResultEntity(
                                testSuiteFileParent,
                                testSuiteResultMessage.getStartTime(),
                                testSuiteResultMessage.getEndTime(),
                                testSuiteResultMessage.getTestType(),
                                testSuiteResultMessage.getBootSuccess(),
                                testSuiteResultMessage.getResultPath(),
                                testSuiteResultMessage.getInfraLogPath(),
                                testSuiteResultMessage.getHostName(),
                                testSuiteResultMessage.getSuitePlan(),
                                testSuiteResultMessage.getSuiteVersion(),
                                testSuiteResultMessage.getSuiteName(),
                                testSuiteResultMessage.getSuiteBuildNumber(),
                                testSuiteResultMessage.getModulesDone(),
                                testSuiteResultMessage.getModulesTotal(),
                                testSuiteResultMessage.getBranch(),
                                testSuiteResultMessage.getTarget(),
                                testSuiteResultMessage.getBuildId(),
                                testSuiteResultMessage.getBuildSystemFingerprint(),
                                testSuiteResultMessage.getBuildVendorFingerprint(),
                                testSuiteResultMessage.getPassedTestCaseCount(),
                                testSuiteResultMessage.getFailedTestCaseCount());

                testSuiteResultEntity.save(newTestSuiteFileEntity);
                resultMap.put("result", "successfully saved!");
            } else {
                logger.log(Level.WARNING, "service_client_id didn't match!");
                logger.log(Level.INFO, "SERVICE_CLIENT_ID => " + tokenInfo.getIssuedTo());
                resultMap.put("result", "Wrong Service Client ID!");
            }
        } else {
            logger.log(Level.WARNING, "postMessage do not contain any accessToken!");
            resultMap.put("result", "Access Token Missing!");
        }
        String json = new Gson().toJson(resultMap);
        response.setStatus(HttpServletResponse.SC_OK);
        response.setContentType("application/json");
        response.setCharacterEncoding("UTF-8");
        response.getWriter().write(json);
    }
}
