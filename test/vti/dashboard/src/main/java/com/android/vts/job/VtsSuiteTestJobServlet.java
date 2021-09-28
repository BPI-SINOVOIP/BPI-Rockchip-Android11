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

package com.android.vts.job;

import com.android.vts.entity.TestSuiteFileEntity;
import com.android.vts.entity.TestSuiteResultEntity;
import com.android.vts.proto.TestSuiteResultMessageProto;
import com.android.vts.util.GcsHelper;
import com.android.vts.util.TaskQueueHelper;
import com.android.vts.util.TimeUtil;
import com.google.appengine.api.memcache.ErrorHandlers;
import com.google.appengine.api.memcache.MemcacheService;
import com.google.appengine.api.memcache.MemcacheServiceFactory;
import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions;
import com.google.cloud.storage.Blob;
import com.google.cloud.storage.Bucket;
import com.google.cloud.storage.Storage;
import com.googlecode.objectify.Key;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

import static com.googlecode.objectify.ObjectifyService.ofy;

/** Suite Test result file processing job. */
public class VtsSuiteTestJobServlet extends BaseJobServlet {

    private static final String SUITE_TEST_URL = "/cron/test_suite_report_gcs_monitor";

    private static final String SERVICE_NAME = "VTS Dashboard";

    private final Logger logger = Logger.getLogger(this.getClass().getName());

    /** Google Cloud Storage project's key file to access the storage */
    private static String GCS_KEY_FILE;
    /** Google Cloud Storage project's default bucket name for vtslab log files */
    private static String GCS_BUCKET_NAME;
    /** Google Cloud Storage project's default directory name for suite test result files */
    private static String GCS_SUITE_TEST_FOLDER_NAME;

    public static final String QUEUE = "suiteTestQueue";

    /**
     * This is the key file to access vtslab-gcs project. It will allow the dashboard to have a full
     * control of the bucket.
     */
    private InputStream keyFileInputStream;

    /** This is the instance of java google storage library */
    private Storage storage;

    /** This is the instance of App Engine memcache service java library */
    private MemcacheService syncCache = MemcacheServiceFactory.getMemcacheService();

    @Override
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);

        GCS_KEY_FILE = systemConfigProp.getProperty("gcs.keyFile");
        GCS_BUCKET_NAME = systemConfigProp.getProperty("gcs.bucketName");
        GCS_SUITE_TEST_FOLDER_NAME = systemConfigProp.getProperty("gcs.suiteTestFolderName");

        this.keyFileInputStream =
                this.getClass().getClassLoader().getResourceAsStream("keys/" + GCS_KEY_FILE);

        Optional<Storage> optionalStorage = GcsHelper.getStorage(this.keyFileInputStream);
        if (optionalStorage.isPresent()) {
            this.storage = optionalStorage.get();
        } else {
            logger.log(Level.SEVERE, "Error on getting storage instance!");
            throw new ServletException("Creating storage instance exception!");
        }
        syncCache.setErrorHandler(ErrorHandlers.getConsistentLogAndContinue(Level.INFO));
    }

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws IOException {

        List<String> dateStringList = new ArrayList<>();

        long currentMicroSecond = TimeUnit.MILLISECONDS.toMicros(System.currentTimeMillis());

        ZonedDateTime checkZonedDateTime = TimeUtil.getZonedDateTime(currentMicroSecond);
        String checkDateString =
                DateTimeFormatter.ofPattern(TimeUtil.DATE_FORMAT)
                        .format(checkZonedDateTime.minusMinutes(5));
        String todayDateString = TimeUtil.getDateString(currentMicroSecond);
        if (!checkDateString.equals(todayDateString)) {
            dateStringList.add(checkDateString);
            logger.log(Level.INFO, "Yesterday is added to the process queue and processed!");
        }
        dateStringList.add(todayDateString);

        for (String dateString : dateStringList) {
            String[] dateArray = dateString.split("-");
            if (dateArray.length == 3) {

                Queue queue = QueueFactory.getQueue(QUEUE);

                List<TaskOptions> tasks = new ArrayList<>();

                String fileSeparator = FileSystems.getDefault().getSeparator();

                String year = dateArray[0];
                String month = dateArray[1];
                String day = dateArray[2];

                List<String> pathList = Arrays.asList(GCS_SUITE_TEST_FOLDER_NAME, year, month, day);
                Path pathInfo = Paths.get(String.join(fileSeparator, pathList));

                List<TestSuiteFileEntity> testSuiteFileEntityList =
                        ofy().load()
                                .type(TestSuiteFileEntity.class)
                                .filter("year", Integer.parseInt(year))
                                .filter("month", Integer.parseInt(month))
                                .filter("day", Integer.parseInt(day))
                                .list();

                List<String> filePathList =
                        testSuiteFileEntityList
                                .stream()
                                .map(testSuiteFile -> testSuiteFile.getFilePath())
                                .collect(Collectors.toList());

                Bucket vtsReportBucket = this.storage.get(GCS_BUCKET_NAME);

                Storage.BlobListOption[] listOptions =
                        new Storage.BlobListOption[] {
                            Storage.BlobListOption.prefix(pathInfo.toString() + fileSeparator)
                        };

                Iterable<Blob> blobIterable = vtsReportBucket.list(listOptions).iterateAll();
                Iterator<Blob> blobIterator = blobIterable.iterator();
                while (blobIterator.hasNext()) {
                    Blob blob = blobIterator.next();
                    if (blob.isDirectory()) {
                        logger.log(Level.INFO, blob.getName() + " directory will be skipped!");
                    } else {
                        if (filePathList.contains(blob.getName())) {
                            logger.log(Level.INFO, "filePathList contain => " + blob.getName());
                        } else if (blob.getName().endsWith(fileSeparator)) {
                            logger.log(Level.INFO, blob.getName() + " endswith slash!");
                        } else {
                            TaskOptions task =
                                    TaskOptions.Builder.withUrl(SUITE_TEST_URL)
                                            .param("filePath", blob.getName())
                                            .method(TaskOptions.Method.POST);
                            tasks.add(task);
                        }
                    }
                }
                TaskQueueHelper.addToQueue(queue, tasks);
            } else {
                throw new IllegalArgumentException(
                        todayDateString + " date string not in correct format");
            }
        }
    }

    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException {

        String filePath = request.getParameter("filePath");
        if (Objects.isNull(filePath)) {
            logger.log(Level.WARNING, "filePath parameter is null!");
        } else {
            logger.log(Level.INFO, "filePath parameter => " + filePath);

            Key<TestSuiteFileEntity> testSuiteFileEntityKey =
                    Key.create(TestSuiteFileEntity.class, filePath);
            TestSuiteFileEntity testSuiteFileEntity =
                    ofy().load()
                            .type(TestSuiteFileEntity.class)
                            .filterKey(testSuiteFileEntityKey)
                            .first()
                            .now();

            if (Objects.isNull(testSuiteFileEntity)) {
                Blob blobFile = (Blob) this.syncCache.get(filePath);
                if (Objects.isNull(blobFile)) {
                    Bucket vtsReportBucket = this.storage.get(GCS_BUCKET_NAME);
                    blobFile = vtsReportBucket.get(filePath);
                    this.syncCache.put(filePath, blobFile);
                }

                if (blobFile.exists()) {
                    TestSuiteFileEntity newTestSuiteFileEntity = new TestSuiteFileEntity(filePath);
                    try {
                        TestSuiteResultMessageProto.TestSuiteResultMessage testSuiteResultMessage =
                                TestSuiteResultMessageProto.TestSuiteResultMessage.parseFrom(
                                        blobFile.getContent());

                        TestSuiteResultEntity testSuiteResultEntity =
                                new TestSuiteResultEntity(
                                        testSuiteFileEntityKey,
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
                    } catch (IOException e) {
                        ofy().delete().type(TestSuiteFileEntity.class).id(filePath).now();
                        response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
                        logger.log(Level.WARNING, "Invalid proto: " + e.getLocalizedMessage());
                    }
                }
            } else {
                logger.log(Level.INFO, "suite test found in datastore!");
            }
        }
    }
}
