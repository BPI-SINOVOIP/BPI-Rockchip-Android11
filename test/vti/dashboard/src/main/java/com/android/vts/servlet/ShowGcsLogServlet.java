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

package com.android.vts.servlet;

import com.android.vts.util.GcsHelper;
import com.google.appengine.api.memcache.ErrorHandlers;
import com.google.appengine.api.memcache.MemcacheService;
import com.google.appengine.api.memcache.MemcacheServiceFactory;
import com.google.auth.oauth2.ServiceAccountCredentials;
import com.google.cloud.storage.Blob;
import com.google.cloud.storage.Bucket;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.Storage.BlobListOption;
import com.google.cloud.storage.StorageOptions;
import com.google.gson.Gson;
import org.apache.commons.io.IOUtils;

import javax.servlet.RequestDispatcher;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.logging.Level;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * A GCS log servlet read log zip file from Google Cloud Storage bucket and show the content in it
 * from the zip file by unarchiving it
 */
@SuppressWarnings("serial")
public class ShowGcsLogServlet extends BaseServlet {

    private static final String GCS_LOG_JSP = "WEB-INF/jsp/show_gcs_log.jsp";

    /** Google Cloud Storage project's key file to access the storage */
    private static String GCS_KEY_FILE;
    /** Google Cloud Storage project's default bucket name for vtslab test result files */
    private static String GCS_BUCKET_NAME;
    /** Google Cloud Storage project's default bucket name for vtslab infra log files */
    private static String GCS_INFRA_LOG_BUCKET_NAME;

    /**
     * This is the key file to access vtslab-gcs project. It will allow the dashboard to have a full
     * control of the bucket.
     */
    private InputStream keyFileInputStream;

    /** This is the instance of java google storage library */
    private Storage storage;

    /** This is the instance of App Engine memcache service java library */
    private MemcacheService syncCache = MemcacheServiceFactory.getMemcacheService();

    /** GCS Test Report Bucket instance */
    private Bucket vtsReportBucket;

    /** GCS Infra Log Bucket instance */
    private Bucket vtsInfraLogBucket;

    @Override
    public void init(ServletConfig cfg) throws ServletException {
        super.init(cfg);

        GCS_KEY_FILE = systemConfigProp.getProperty("gcs.keyFile");
        GCS_BUCKET_NAME = systemConfigProp.getProperty("gcs.bucketName");
        GCS_INFRA_LOG_BUCKET_NAME = systemConfigProp.getProperty("gcs.infraLogBucketName");

        String keyFilePath = "keys/" + GCS_KEY_FILE;

        byte[] keyFileByteArray = new byte[0];
        try {
            keyFileByteArray =
                    IOUtils.toByteArray(
                            this.getClass().getClassLoader().getResourceAsStream(keyFilePath));
        } catch (IOException e) {
            e.printStackTrace();
        }
        this.keyFileInputStream = new ByteArrayInputStream(keyFileByteArray);
        InputStream vtsReportInputStream = new ByteArrayInputStream(keyFileByteArray);
        InputStream vtsInfraInputStream = new ByteArrayInputStream(keyFileByteArray);

        Optional<Storage> optionalVtsReportStorage = GcsHelper.getStorage(vtsReportInputStream);
        if (optionalVtsReportStorage.isPresent()) {
            this.storage = optionalVtsReportStorage.get();
            this.vtsReportBucket = storage.get(GCS_BUCKET_NAME);
        } else {
            logger.log(Level.SEVERE, "Error on getting storage instance!");
            throw new ServletException("Creating storage instance exception!");
        }

        Optional<Storage> optionalVtsInfraStorage = GcsHelper.getStorage(vtsInfraInputStream);
        if (optionalVtsInfraStorage.isPresent()) {
            this.storage = optionalVtsInfraStorage.get();
            this.vtsInfraLogBucket = storage.get(GCS_INFRA_LOG_BUCKET_NAME);
        } else {
            logger.log(Level.SEVERE, "Error on getting storage instance!");
            throw new ServletException("Creating storage instance exception!");
        }
        syncCache.setErrorHandler(ErrorHandlers.getConsistentLogAndContinue(Level.INFO));
    }

    @Override
    public PageType getNavParentType() {
        return PageType.TOT;
    }

    @Override
    public List<Page> getBreadcrumbLinks(HttpServletRequest request) {
        return null;
    }

    @Override
    public void doGetHandler(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        if (keyFileInputStream == null) {
            request.setAttribute("error_title", "GCS Key file Error");
            request.setAttribute("error_message", "The GCS Key file is not existed!");
            RequestDispatcher dispatcher = request.getRequestDispatcher(ERROR_MESSAGE_JSP);
            try {
                dispatcher.forward(request, response);
            } catch (ServletException e) {
                logger.log(Level.SEVERE, "Servlet Excpetion caught : ", e);
            }
        } else {
            String pathInfo = request.getPathInfo();
            if (Objects.nonNull(pathInfo)) {
                if (pathInfo.equalsIgnoreCase("/download")) {
                    downloadHandler(request, response);
                } else {
                    logger.log(Level.INFO, "Path Info => " + pathInfo);
                    logger.log(Level.WARNING, "Unknown path access!");
                }
            } else {
                defaultHandler(request, response);
            }
        }
    }

    private void downloadHandler(HttpServletRequest request, HttpServletResponse response)
            throws IOException {
        String file = request.getParameter("file") == null ? "/" : request.getParameter("file");
        Path filePathInfo = Paths.get(file);

        Blob blobFile = vtsInfraLogBucket.get(filePathInfo.toString());

        if (blobFile.exists()) {
            response.setContentType("application/octet-stream");
            response.setContentLength(blobFile.getSize().intValue());
            response.setHeader(
                    "Content-Disposition",
                    "attachment; filename=\"" + filePathInfo.getFileName() + "\"");

            response.getOutputStream().write(blobFile.getContent());
        } else {
            request.setAttribute("error_title", "Infra Log File Not Found");
            request.setAttribute("error_message", "Please contact the administrator!");
            RequestDispatcher dispatcher = request.getRequestDispatcher(ERROR_MESSAGE_JSP);
            try {
                dispatcher.forward(request, response);
            } catch (ServletException e) {
                logger.log(Level.SEVERE, "Servlet Excpetion caught : ", e);
            }
        }
    }

    private void defaultHandler(HttpServletRequest request, HttpServletResponse response)
            throws IOException {

        String action =
                request.getParameter("action") == null ? "read" : request.getParameter("action");
        String path = request.getParameter("path") == null ? "/" : request.getParameter("path");
        String entry = request.getParameter("entry") == null ? "" : request.getParameter("entry");
        Path pathInfo = Paths.get(path);

        List<String> dirList = new ArrayList<>();
        List<String> fileList = new ArrayList<>();
        List<String> entryList = new ArrayList<>();
        Map<String, Object> resultMap = new HashMap<>();
        String entryContent = "";

        if (pathInfo.toString().endsWith(".zip")) {

            Blob blobFile = (Blob) this.syncCache.get(path.toString());
            if (blobFile == null) {
                blobFile = vtsReportBucket.get(path);
                this.syncCache.put(path.toString(), blobFile);
            }

            if (action.equalsIgnoreCase("read")) {
                InputStream blobInputStream = new ByteArrayInputStream(blobFile.getContent());
                ZipInputStream zipInputStream = new ZipInputStream(blobInputStream);

                ZipEntry zipEntry;
                while ((zipEntry = zipInputStream.getNextEntry()) != null) {
                    if (zipEntry.isDirectory()) {

                    } else {
                        if (entry.length() > 0) {
                            logger.log(Level.INFO, "param entry => " + entry);
                            if (zipEntry.getName().equals(entry)) {
                                logger.log(Level.INFO, "matched !!!! " + zipEntry.getName());
                                entryContent =
                                        IOUtils.toString(
                                                zipInputStream, StandardCharsets.UTF_8.name());
                            }
                        } else {
                            entryList.add(zipEntry.getName());
                        }
                    }
                }
                resultMap.put("entryList", entryList);
                resultMap.put("entryContent", entryContent);

                String json = new Gson().toJson(resultMap);
                response.setContentType("application/json");
                response.setCharacterEncoding("UTF-8");
                response.getWriter().write(json);
            } else {
                response.setContentType("application/octet-stream");
                response.setContentLength(blobFile.getSize().intValue());
                response.setHeader(
                        "Content-Disposition",
                        "attachment; filename=\"" + pathInfo.getFileName() + "\"");

                response.getOutputStream().write(blobFile.getContent());
            }

        } else {

            logger.log(Level.INFO, "path info => " + pathInfo);
            logger.log(Level.INFO, "path name count => " + pathInfo.getNameCount());

            BlobListOption[] listOptions;
            if (pathInfo.getNameCount() == 0) {
                listOptions = new BlobListOption[] {BlobListOption.currentDirectory()};
            } else {
                String prefixPathString = path.endsWith("/") ? path : path.concat("/");
                if (pathInfo.getNameCount() <= 1) {
                    dirList.add("/");
                } else {
                    dirList.add(getParentDirPath(prefixPathString));
                }

                listOptions =
                        new BlobListOption[] {
                            BlobListOption.currentDirectory(),
                            BlobListOption.prefix(prefixPathString)
                        };
            }

            Iterable<Blob> blobIterable = vtsReportBucket.list(listOptions).iterateAll();
            Iterator<Blob> blobIterator = blobIterable.iterator();
            while (blobIterator.hasNext()) {
                Blob blob = blobIterator.next();
                logger.log(Level.INFO, "blob name => " + blob);
                if (blob.isDirectory()) {
                    logger.log(Level.INFO, "directory name => " + blob.getName());
                    dirList.add(blob.getName());
                } else {
                    logger.log(Level.INFO, "file name => " + blob.getName());
                    fileList.add(Paths.get(blob.getName()).getFileName().toString());
                }
            }

            response.setStatus(HttpServletResponse.SC_OK);
            request.setAttribute("entryList", entryList);
            request.setAttribute("dirList", dirList);
            request.setAttribute("fileList", fileList);
            request.setAttribute("path", path);
            RequestDispatcher dispatcher = request.getRequestDispatcher(GCS_LOG_JSP);
            try {
                dispatcher.forward(request, response);
            } catch (ServletException e) {
                logger.log(Level.SEVERE, "Servlet Excpetion caught : ", e);
            }
        }
    }

    private String getParentDirPath(String fileOrDirPath) {
        boolean endsWithSlashCheck = fileOrDirPath.endsWith(File.separator);
        return fileOrDirPath.substring(
                0,
                fileOrDirPath.lastIndexOf(
                        File.separatorChar,
                        endsWithSlashCheck
                                ? fileOrDirPath.length() - 2
                                : fileOrDirPath.length() - 1));
    }
}
