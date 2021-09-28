/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.tradefed.testtype.mobly;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto;

import com.android.tradefed.testtype.mobly.MoblyYamlResultControllerInfoHandler.ControllerInfo;
import com.android.tradefed.testtype.mobly.MoblyYamlResultRecordHandler.Record;
import com.android.tradefed.testtype.mobly.MoblyYamlResultSummaryHandler.Summary;
import com.android.tradefed.testtype.mobly.MoblyYamlResultUserDataHandler.UserData;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link MoblyYamlResultParser}. */
@RunWith(JUnit4.class)
public class MoblyYamlResultParserTest {
    private static final String DEFAULT_BEGIN_TIME = "1571681517464";
    private static final String DEFAULT_END_TIME = "1571681520407";
    private static final String DEFAULT_TEST_CLASS = "DefaultTestClass";
    private static final String DEFAULT_TEST_NAME = "default_test_name";
    private static final String SAMPLE_STACK_TRACE =
            "\"Traceback (most recent call last):\\n  "
                    + "File \\\"/usr/local/google/home/yourldap/temp/sanity_suite_host"
                    + ".par/google3/third_party/py/mobly/base_test.py\\\"\\\n"
                    + "    , line 354, in _teardown_class\\n    self.teardown_class()\\n  File "
                    + "\\\"/usr/local/google/home/yourldap/temp/sanity_suite_host"
                    + ".par/google3/javatests/com/google/android/apps/camera/functional/syshealth/sanity"
                    + ".py\\\"\\\n"
                    + "    , line 51, in teardown_class\\n    self._helper.dut.uia.press.home()\\n  File "
                    + "\\\"\\\n"
                    + "    /usr/local/google/home/yourldap/temp/sanity_suite_host"
                    + ".par/google3/third_party/py/mobly/controllers/android_device.py\\\"\\\n"
                    + "    , line 1091, in __getattr__\\n    return self.__getattribute__(name)"
                    + "\\nAttributeError:\\\n"
                    + "    \\ 'AndroidDevice' object has no attribute 'uia'\\n\"";
    private static final Map<String, Object> mRecordMap;

    static {
        mRecordMap = new HashMap<>();
        mRecordMap.put("Details", "test_details");
        mRecordMap.put("End Time", DEFAULT_END_TIME);
        mRecordMap.put("Begin Time", DEFAULT_BEGIN_TIME);
        mRecordMap.put("Extra Errors", "{}");
        mRecordMap.put("Extras", "null");
        mRecordMap.put("Result", "PASS");
        mRecordMap.put("Stacktrace", "null");
        mRecordMap.put("Test Class", DEFAULT_TEST_CLASS);
        mRecordMap.put("Test Name", DEFAULT_TEST_NAME);
        mRecordMap.put("Type", "Record");
        mRecordMap.put("UID", "null");
    }

    private static final String USER_DATA =
            "Test Name: test_imageintent_take_photo\n"
                    + "Type: UserData\n"
                    + "sponge_properties:\n"
                    + "    dut_build_info: {build_characteristics: nosdcard, build_id: MASTER, "
                    + "build_product: blueline,\n"
                    + "        build_type: userdebug, build_version_codename: R, build_version_sdk: '29',\n"
                    + "        debuggable: '1', hardware: blueline, product_name: blueline}\n"
                    + "    dut_model: blueline\n"
                    + "    dut_serial: 827X003PY\n"
                    + "    dut_user_added_info: {}\n"
                    + "timestamp: 1571681312605";
    private static final String CONTROLLER_INFO =
            "Controller Info:\n"
                    + "-   build_info: {build_characteristics: nosdcard, build_id: MASTER, build_product:"
                    + " blueline,\n"
                    + "        build_type: userdebug, build_version_codename: R, build_version_sdk: '29',\n"
                    + "        debuggable: '1', hardware: blueline, product_name: blueline}\n"
                    + "    model: blueline\n"
                    + "    serial: 827X003PY\n"
                    + "    user_added_info: {}\n"
                    + "Controller Name: AndroidDevice\n"
                    + "Test Class: ImageIntentSanity\n"
                    + "Timestamp: 1571681322.791003\n"
                    + "Type: ControllerInfo";
    private static final String SUMMARY =
            "{Error: 2, Executed: 3, Failed: 1, Passed: 1, Requested: 4, Skipped: 0, Type: Summary}";
    private static final String TESTNAME_LIST =
            "Requested Tests: [test_imageintent_cold_launch, "
                    + "test_imageintent_take_photo]\n"
                    + "Type: TestNameList";

    private MoblyYamlResultParser mParser;
    private ITestInvocationListener mMockListener;
    private ImmutableList<ITestInvocationListener> mListeners;
    private String mRunName;
    private ArgumentCaptor<String> mRunNameCaptor;
    private ArgumentCaptor<Integer> mCountCaptor;
    private ArgumentCaptor<TestDescription> mStartedDescCaptor;
    private ArgumentCaptor<Long> mBeginTimeCaptor;
    private ArgumentCaptor<TestDescription> mFailedDescCaptor;
    private ArgumentCaptor<FailureDescription> mFailureDescriptionCaptor;
    private ArgumentCaptor<TestDescription> mEndDescCaptor;
    private ArgumentCaptor<Long> mEndTimeCaptor;
    private ArgumentCaptor<Long> mElapseTimeCaptor;

    @Before
    public void setUp() throws Exception {
        mMockListener = Mockito.mock(ITestInvocationListener.class);
        mListeners = ImmutableList.of(mMockListener);
        setUpArgumentCaptors();
    }

    private void setUpArgumentCaptors() {
        // Setup testRunStarted
        mRunNameCaptor = ArgumentCaptor.forClass(String.class);
        mCountCaptor = ArgumentCaptor.forClass(Integer.class);
        Mockito.doNothing()
                .when(mMockListener)
                .testRunStarted(mRunNameCaptor.capture(), mCountCaptor.capture());
        // Setup testStarted
        mStartedDescCaptor = ArgumentCaptor.forClass(TestDescription.class);
        mBeginTimeCaptor = ArgumentCaptor.forClass(Long.class);
        Mockito.doNothing()
                .when(mMockListener)
                .testStarted(mStartedDescCaptor.capture(), mBeginTimeCaptor.capture());
        // Setup testFailed
        mFailedDescCaptor = ArgumentCaptor.forClass(TestDescription.class);
        mFailureDescriptionCaptor = ArgumentCaptor.forClass(FailureDescription.class);
        Mockito.doNothing()
                .when(mMockListener)
                .testFailed(mFailedDescCaptor.capture(), mFailureDescriptionCaptor.capture());
        // Setup testEnded
        mEndDescCaptor = ArgumentCaptor.forClass(TestDescription.class);
        mEndTimeCaptor = ArgumentCaptor.forClass(Long.class);
        Mockito.doNothing()
                .when(mMockListener)
                .testEnded(mEndDescCaptor.capture(), mEndTimeCaptor.capture(), any(Map.class));
        // Setup testRunEnded
        mElapseTimeCaptor = ArgumentCaptor.forClass(Long.class);
        Mockito.doNothing()
                .when(mMockListener)
                .testRunEnded(mElapseTimeCaptor.capture(), any(Map.class));
    }

    @Test
    public void testReportToListenersPassRecord() {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        IMoblyYamlResultHandler.ITestResult passRecord =
                new Record.Builder()
                        .setTestName(DEFAULT_TEST_NAME)
                        .setTestClass(DEFAULT_TEST_CLASS)
                        .setBeginTime(DEFAULT_BEGIN_TIME)
                        .setEndTime(DEFAULT_END_TIME)
                        .setResult(MoblyYamlResultRecordHandler.RecordResult.PASS)
                        .build();
        List<IMoblyYamlResultHandler.ITestResult> resultCache = ImmutableList.of(passRecord);
        mParser.reportToListeners(mListeners, resultCache);

        assertEquals(mRunName, mRunNameCaptor.getValue());
        assertEquals(0, (int) mCountCaptor.getValue());
        assertEquals(DEFAULT_TEST_CLASS, mStartedDescCaptor.getValue().getClassName());
        assertEquals(DEFAULT_TEST_NAME, mStartedDescCaptor.getValue().getTestName());
        assertEquals(Long.parseLong(DEFAULT_BEGIN_TIME), (long) mBeginTimeCaptor.getValue());
        verify(mMockListener, never()).testFailed(any(), anyString());
        assertEquals(DEFAULT_TEST_CLASS, mEndDescCaptor.getValue().getClassName());
        assertEquals(DEFAULT_TEST_NAME, mEndDescCaptor.getValue().getTestName());
        assertEquals(Long.parseLong(DEFAULT_END_TIME), (long) mEndTimeCaptor.getValue());
        assertEquals(
                Long.parseLong(DEFAULT_END_TIME) - Long.parseLong(DEFAULT_BEGIN_TIME),
                (long) mElapseTimeCaptor.getValue());
    }

    @Test
    public void testReportToListenersFailRecord() {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        IMoblyYamlResultHandler.ITestResult failRecord =
                new Record.Builder()
                        .setTestName(DEFAULT_TEST_NAME)
                        .setTestClass(DEFAULT_TEST_CLASS)
                        .setBeginTime(DEFAULT_BEGIN_TIME)
                        .setEndTime(DEFAULT_END_TIME)
                        .setResult(MoblyYamlResultRecordHandler.RecordResult.FAIL)
                        .setStackTrace(SAMPLE_STACK_TRACE)
                        .build();
        List<IMoblyYamlResultHandler.ITestResult> resultCache = ImmutableList.of(failRecord);
        mParser.reportToListeners(mListeners, resultCache);

        assertEquals(mRunName, mRunNameCaptor.getValue());
        assertEquals(0, (int) mCountCaptor.getValue());
        assertEquals(DEFAULT_TEST_CLASS, mStartedDescCaptor.getValue().getClassName());
        assertEquals(DEFAULT_TEST_NAME, mStartedDescCaptor.getValue().getTestName());
        assertEquals(Long.parseLong(DEFAULT_BEGIN_TIME), (long) mBeginTimeCaptor.getValue());
        assertEquals(DEFAULT_TEST_CLASS, mFailedDescCaptor.getValue().getClassName());
        assertEquals(DEFAULT_TEST_NAME, mFailedDescCaptor.getValue().getTestName());
        assertTrue(
                mFailureDescriptionCaptor
                        .getValue()
                        .getErrorMessage()
                        .contains("Traceback (most recent call last)"));
        assertTrue(
                mFailureDescriptionCaptor
                        .getValue()
                        .getFailureStatus()
                        .equals(TestRecordProto.FailureStatus.TEST_FAILURE));
        assertEquals(DEFAULT_TEST_CLASS, mEndDescCaptor.getValue().getClassName());
        assertEquals(DEFAULT_TEST_NAME, mEndDescCaptor.getValue().getTestName());
        assertEquals(Long.parseLong(DEFAULT_END_TIME), (long) mEndTimeCaptor.getValue());
        assertEquals(
                Long.parseLong(DEFAULT_END_TIME) - Long.parseLong(DEFAULT_BEGIN_TIME),
                (long) mElapseTimeCaptor.getValue());
    }

    @Test
    public void testReportToListenersUserData() {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        List<IMoblyYamlResultHandler.ITestResult> resultCache =
                ImmutableList.of(new UserData.Builder().setTimestamp(DEFAULT_BEGIN_TIME).build());
        mParser.reportToListeners(mListeners, resultCache);

        assertEquals(mRunName, mRunNameCaptor.getValue());
        assertEquals(0, (int) mCountCaptor.getValue());
        verify(mMockListener, never()).testStarted(any(), anyLong());
        verify(mMockListener, never()).testFailed(any(), anyString());
        assertEquals(0L - Long.parseLong(DEFAULT_BEGIN_TIME), (long) mElapseTimeCaptor.getValue());
    }

    @Test
    public void testReportToListenersControllerInfo() {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        List<IMoblyYamlResultHandler.ITestResult> resultCache =
                ImmutableList.of(
                        new ControllerInfo.Builder().setTimestamp("1571681322.791003").build());
        mParser.reportToListeners(mListeners, resultCache);

        assertEquals(mRunName, mRunNameCaptor.getValue());
        assertEquals(0, (int) mCountCaptor.getValue());
        verify(mMockListener, never()).testStarted(any(), anyLong());
        verify(mMockListener, never()).testFailed(any(), anyString());
        assertEquals(1571681322791L, (long) mElapseTimeCaptor.getValue());
    }

    @Test
    public void testParseDocumentMapRecordPass() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> detailMap = new HashMap<>();
        detailMap.put("Result", "PASS");
        detailMap.put("Stacktrace", "null");
        Map<String, Object> docMap = buildTestRecordDocMap(detailMap);
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof Record);
        assertEquals(MoblyYamlResultRecordHandler.RecordResult.PASS, ((Record) result).getResult());
        assertEquals(Long.parseLong(DEFAULT_BEGIN_TIME), ((Record) result).getBeginTime());
        assertEquals(Long.parseLong(DEFAULT_END_TIME), ((Record) result).getEndTime());
        assertEquals(DEFAULT_TEST_CLASS, ((Record) result).getTestClass());
        assertEquals(DEFAULT_TEST_NAME, ((Record) result).getTestName());
    }

    @Test
    public void testParseDocumentMapRecordFail() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> detailMap = new HashMap<>();
        detailMap.put("Stacktrace", SAMPLE_STACK_TRACE);
        detailMap.put("Result", "FAIL");
        Map<String, Object> docMap = buildTestRecordDocMap(detailMap);
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof Record);
        assertEquals(MoblyYamlResultRecordHandler.RecordResult.FAIL, ((Record) result).getResult());
        assertEquals(Long.parseLong(DEFAULT_BEGIN_TIME), ((Record) result).getBeginTime());
        assertEquals(Long.parseLong(DEFAULT_END_TIME), ((Record) result).getEndTime());
        assertEquals(DEFAULT_TEST_CLASS, ((Record) result).getTestClass());
        assertEquals(DEFAULT_TEST_NAME, ((Record) result).getTestName());
        assertTrue(((Record) result).getStackTrace().contains("Traceback (most recent call last)"));
    }

    @Test
    public void testParseDocumentMapSummary() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> docMap = new HashMap<>();
        docMap.put("Type", "Summary");
        docMap.put("Executed", "10");
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof Summary);
        assertEquals(10, ((Summary) result).getExecuted());
    }

    @Test
    public void testParseDocumentMapControllerInfo() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> docMap = new HashMap<>();
        docMap.put("Type", "ControllerInfo");
        docMap.put("Timestamp", "1571681322.791003");
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof ControllerInfo);
        long expTimestamp = Math.round(Double.parseDouble("1571681322.791003") * 1000);
        assertEquals(expTimestamp, ((ControllerInfo) result).getTimeStamp());
    }

    @Test
    public void testParseDocumentMapUserData() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> docMap = new HashMap<>();
        docMap.put("Type", "UserData");
        docMap.put("timestamp", DEFAULT_BEGIN_TIME);
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof UserData);
        assertEquals(Long.parseLong(DEFAULT_BEGIN_TIME), ((UserData) result).getTimeStamp());
    }

    @Test
    public void testParseDocumentMapTestNameList() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        Map<String, Object> docMap = new HashMap<>();
        docMap.put("Type", "TestNameList");
        IMoblyYamlResultHandler.ITestResult result = mParser.parseDocumentMap(docMap);
        assertTrue(result instanceof MoblyYamlResultTestNameListHandler.TestNameList);
    }

    @Test
    public void testParse() throws Exception {
        mRunName = new Object() {}.getClass().getEnclosingMethod().getName();
        mParser = new MoblyYamlResultParser(mMockListener, mRunName);
        MoblyYamlResultParser spyParser = Mockito.spy(mParser);

        String passRecord = buildTestRecordString(new HashMap<>());
        Map<String, Object> failMap = new HashMap<>();
        failMap.put("Result", "FAIL");
        failMap.put("Stracktrace", SAMPLE_STACK_TRACE);
        String failRecord = buildTestRecordString(failMap);

        StringBuilder strBuilder = new StringBuilder();
        strBuilder.append("---\n");
        strBuilder.append(TESTNAME_LIST);
        strBuilder.append("\n---\n");
        strBuilder.append(USER_DATA);
        strBuilder.append("\n---\n");
        strBuilder.append(passRecord);
        strBuilder.append("\n---\n");
        strBuilder.append(failRecord);
        strBuilder.append("\n---\n");
        strBuilder.append(CONTROLLER_INFO);
        strBuilder.append("\n--- ");
        strBuilder.append(SUMMARY);
        InputStream inputStream = new ByteArrayInputStream(strBuilder.toString().getBytes());

        spyParser.parse(inputStream);
        verify(spyParser, times(6)).parseDocumentMap(any(Map.class));
        verify(spyParser, times(1)).reportToListeners(any(), any());
    }

    private ImmutableMap<String, Object> buildTestRecordDocMap(Map<String, Object> propertyMap) {
        Map<String, Object> map = new HashMap<>();
        map.putAll(mRecordMap);
        map.putAll(propertyMap);
        return ImmutableMap.copyOf(map);
    }

    private String buildTestRecordString(Map<String, Object> detailMap) {
        StringBuilder builder = new StringBuilder();
        for (Iterator<Map.Entry<String, Object>> iterator =
                        buildTestRecordDocMap(detailMap).entrySet().iterator();
                iterator.hasNext(); ) {
            Map.Entry<String, Object> entry = iterator.next();
            builder.append(String.format("%s: %s", entry.getKey(), entry.getValue()));
            if (iterator.hasNext()) {
                builder.append("\n");
            }
        }
        return builder.toString();
    }
}
