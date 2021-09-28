/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.vts.proto.VtsReportMessage.DashboardPostMessage;

import com.google.api.client.auth.oauth2.Credential;
import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.http.javanet.NetHttpTransport;
import com.google.api.client.json.JsonFactory;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.common.base.Strings;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedList;
import java.util.List;
import java.util.NoSuchElementException;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Uploads the VTS test plan execution result to the web DB using a RESTful API and an OAuth2
 * credential kept in a json file.
 */
public class VtsDashboardUtil {
    private static final String PLUS_ME = "https://www.googleapis.com/auth/plus.me";
    private static final int BASE_TIMEOUT_MSECS = 1000 * 60;
    private static VtsVendorConfigFileUtil mConfigReader;
    private static final IRunUtil mRunUtil = new RunUtil();
    private static VtsDashboardApiTransport vtsDashboardApiTransport;

    public VtsDashboardUtil(VtsVendorConfigFileUtil configReader) {
        mConfigReader = configReader;
        try {
            String apiUrl = mConfigReader.GetVendorConfigVariable("dashboard_api_host_url");
            vtsDashboardApiTransport = new VtsDashboardApiTransport(new NetHttpTransport(), apiUrl);
        } catch (NoSuchElementException e) {
            CLog.w("Configure file not available.");
        }
    }

    /**
     * Returns an OAuth2 token string obtained using a service account json keyfile.
     *
     * Uses the service account keyfile located at config variable 'service_key_json_path'
     * to request an OAuth2 token.
     */
    private String GetToken() {
        String keyFilePath;
        try {
            keyFilePath = mConfigReader.GetVendorConfigVariable("service_key_json_path");
        } catch (NoSuchElementException e) {
            return null;
        }

        JsonFactory jsonFactory = JacksonFactory.getDefaultInstance();
        Credential credential = null;
        try {
            List<String> listStrings = new LinkedList<>();
            listStrings.add(PLUS_ME);
            credential = GoogleCredential.fromStream(new FileInputStream(keyFilePath))
                                 .createScoped(listStrings);
            credential.refreshToken();
            return credential.getAccessToken();
        } catch (FileNotFoundException e) {
            CLog.e(String.format("Service key file %s doesn't exist.", keyFilePath));
        } catch (IOException e) {
            CLog.e(String.format("Can't read the service key file, %s", keyFilePath));
        }
        return null;
    }

    /**
     * Uploads the given message to the web DB.
     *
     * @param message, DashboardPostMessage that keeps the result to upload.
     */
    public void Upload(DashboardPostMessage.Builder message) {
        String dashboardCurlCommand =
                mConfigReader.GetVendorConfigVariable("dashboard_use_curl_command");
        Optional<String> dashboardCurlCommandOpt = Optional.of(dashboardCurlCommand);
        Boolean curlCommandCheck = Boolean.parseBoolean(dashboardCurlCommandOpt.orElse("false"));
        String token = GetToken();
        if (token == null) {
            return;
        }
        message.setAccessToken(token);
        String messageFilePath = "";
        try {
            messageFilePath = WriteToTempFile(
                    Base64.getEncoder().encodeToString(message.build().toByteArray()).getBytes());
        } catch (IOException e) {
            CLog.e("Couldn't write a proto message to a temp file.");
        }

        if (Strings.isNullOrEmpty(messageFilePath)) {
            CLog.e("Couldn't get the MessageFilePath.");
        } else {
            if (curlCommandCheck) {
                CurlUpload(messageFilePath);
            } else {
                Upload(messageFilePath);
            }
        }
    }

    /**
     * Uploads the given message file path to the web DB using google http java api library.
     *
     * @param messageFilePath, DashboardPostMessage file path that keeps the result to upload.
     */
    public Boolean Upload(String messageFilePath) {
        try {
            String response = vtsDashboardApiTransport.postFile(
                    "/api/datastore", "application/octet-stream", messageFilePath);
            CLog.d(String.format("Upload Result : %s", response));
            return true;
        } catch (IOException e) {
            CLog.e("Error occurred on uploading dashboard message file!");
            CLog.e(e.getLocalizedMessage());
            return false;
        }
    }

    /**
     * Uploads the given message file path to the web DB using curl command.
     *
     * @param messageFilePath, DashboardPostMessage file path that keeps the result to upload.
     */
    @Deprecated
    public void CurlUpload(String messageFilePath) {
        try {
            String commandTemplate =
                    mConfigReader.GetVendorConfigVariable("dashboard_post_command");
            commandTemplate = commandTemplate.replace("{path}", messageFilePath);
            // removes ', while keeping any substrings quoted by "".
            commandTemplate = commandTemplate.replace("'", "");
            CLog.d(String.format("Upload command: %s", commandTemplate));
            List<String> commandList = new ArrayList<String>();
            Matcher matcher = Pattern.compile("([^\"]\\S*|\".+?\")\\s*").matcher(commandTemplate);
            while (matcher.find()) {
                commandList.add(matcher.group(1));
            }
            CommandResult c = mRunUtil.runTimedCmd(BASE_TIMEOUT_MSECS * 3,
                    (String[]) commandList.toArray(new String[commandList.size()]));
            if (c == null || c.getStatus() != CommandStatus.SUCCESS) {
                CLog.e("Uploading the test plan execution result to GAE DB faiied.");
                CLog.e("Stdout: %s", c.getStdout());
                CLog.e("Stderr: %s", c.getStderr());
            }
            FileUtil.deleteFile(new File(messageFilePath));
        } catch (NoSuchElementException e) {
            CLog.e("dashboard_post_command unspecified in vendor config.");
        }
    }

    /**
     * Simple wrapper to write data to a temp file.
     *
     * @param data, actual data to write to a file.
     * @throws IOException
     */
    private String WriteToTempFile(byte[] data) throws IOException {
        File tempFile = File.createTempFile("tempfile", ".tmp");
        String filePath = tempFile.getAbsolutePath();
        FileOutputStream out = new FileOutputStream(filePath);
        out.write(data);
        out.close();
        return filePath;
    }
}
