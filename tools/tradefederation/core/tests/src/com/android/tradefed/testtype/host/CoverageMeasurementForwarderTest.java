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
package com.android.tradefed.testtype.host;

import static com.google.common.io.Files.getNameWithoutExtension;
import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;

import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link CoverageMeasurementForwarder}. */
@RunWith(JUnit4.class)
public final class CoverageMeasurementForwarderTest {

    private static final String ARTIFACT_NAME1 = "fooTest.exec";
    private static final String ARTIFACT_NAME2 = "barTest.exec";
    private static final String ARTIFACT_NAME3 = "nativeFoo.zip";
    private static final String ARTIFACT_NAME4 = "nativeBar.zip";
    private static final String NONEXISTANT_ARTIFACT1 = "missingArtifact.exec";
    private static final String NONEXISTANT_ARTIFACT2 = "missingArtifact.zip";

    private static final ByteString MEASUREMENT1 =
            ByteString.copyFromUtf8("Mi estas kovrado mezurado");
    private static final ByteString MEASUREMENT2 =
            ByteString.copyFromUtf8("Mi estas ankau kovrado mezurado");
    private static final ByteString MEASUREMENT3 =
            ByteString.copyFromUtf8("A native coverage measurement");
    private static final ByteString MEASUREMENT4 =
            ByteString.copyFromUtf8("Another native coverage measurement");

    @Mock IBuildInfo mMockBuildInfo;
    @Mock ITestInvocationListener mMockListener;
    private TestInformation mTestInfo;

    @Rule public TemporaryFolder mFolder = new TemporaryFolder();

    /** The {@link CoverageMeasurementForwarder} under test. */
    private CoverageMeasurementForwarder mForwarder;

    private Map<String, ByteString> mCapturedJavaLogs = new HashMap<>();
    private Map<String, ByteString> mCapturedNativeLogs = new HashMap<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();

        Answer<Void> javaLogCapturingListener =
                invocation -> {
                    Object[] args = invocation.getArguments();
                    try (InputStream stream = ((InputStreamSource) args[2]).createInputStream()) {
                        mCapturedJavaLogs.put((String) args[0], ByteString.readFrom(stream));
                    }
                    return null;
                };
        doAnswer(javaLogCapturingListener)
                .when(mMockListener)
                .testLog(anyString(), eq(LogDataType.COVERAGE), any(InputStreamSource.class));

        Answer<Void> nativeLogCapturingListener =
                invocation -> {
                    Object[] args = invocation.getArguments();
                    try (InputStream stream = ((InputStreamSource) args[2]).createInputStream()) {
                        mCapturedNativeLogs.put((String) args[0], ByteString.readFrom(stream));
                    }
                    return null;
                };
        doAnswer(nativeLogCapturingListener)
                .when(mMockListener)
                .testLog(
                        anyString(), eq(LogDataType.NATIVE_COVERAGE), any(InputStreamSource.class));

        doAnswer(invocation -> mockBuildArtifact(MEASUREMENT1))
                .when(mMockBuildInfo)
                .getFile(ARTIFACT_NAME1);
        doAnswer(invocation -> mockBuildArtifact(MEASUREMENT2))
                .when(mMockBuildInfo)
                .getFile(ARTIFACT_NAME2);
        doAnswer(invocation -> mockBuildArtifact(MEASUREMENT3))
                .when(mMockBuildInfo)
                .getFile(ARTIFACT_NAME3);
        doAnswer(invocation -> mockBuildArtifact(MEASUREMENT4))
                .when(mMockBuildInfo)
                .getFile(ARTIFACT_NAME4);

        mForwarder = new CoverageMeasurementForwarder();
    }

    @Test
    public void testForwardSingleJavaArtifact() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(ARTIFACT_NAME1));
        mForwarder.run(mTestInfo, mMockListener);

        assertThat(mCapturedJavaLogs)
                .containsExactly(getNameWithoutExtension(ARTIFACT_NAME1), MEASUREMENT1);
        assertThat(mCapturedNativeLogs).isEmpty();
    }

    @Test
    public void testForwardMultipleJavaArtifacts() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(ARTIFACT_NAME1, ARTIFACT_NAME2));
        mForwarder.run(mTestInfo, mMockListener);

        assertThat(mCapturedJavaLogs)
                .containsExactly(
                        getNameWithoutExtension(ARTIFACT_NAME1),
                        MEASUREMENT1,
                        getNameWithoutExtension(ARTIFACT_NAME2),
                        MEASUREMENT2);
        assertThat(mCapturedNativeLogs).isEmpty();
    }

    @Test
    public void testNoSuchJavaArtifact() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(NONEXISTANT_ARTIFACT1));
        try {
            mForwarder.run(mTestInfo, mMockListener);
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // Expected
        }
        assertThat(mCapturedJavaLogs).isEmpty();
        assertThat(mCapturedNativeLogs).isEmpty();
    }

    @Test
    public void testForwardSingleNativeArtifact() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(ARTIFACT_NAME3));
        mForwarder.setCoverageLogDataType(LogDataType.NATIVE_COVERAGE);
        mForwarder.run(mTestInfo, mMockListener);

        assertThat(mCapturedJavaLogs).isEmpty();
        assertThat(mCapturedNativeLogs)
                .containsExactly(getNameWithoutExtension(ARTIFACT_NAME3), MEASUREMENT3);
    }

    @Test
    public void testForwardMultipleNativeArtifacts() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(ARTIFACT_NAME3, ARTIFACT_NAME4));
        mForwarder.setCoverageLogDataType(LogDataType.NATIVE_COVERAGE);
        mForwarder.run(mTestInfo, mMockListener);

        assertThat(mCapturedJavaLogs).isEmpty();
        assertThat(mCapturedNativeLogs)
                .containsExactly(
                        getNameWithoutExtension(ARTIFACT_NAME3),
                        MEASUREMENT3,
                        getNameWithoutExtension(ARTIFACT_NAME4),
                        MEASUREMENT4);
    }

    @Test
    public void testNoSuchNativeArtifact() {
        mForwarder.setCoverageMeasurements(ImmutableList.of(NONEXISTANT_ARTIFACT2));
        mForwarder.setCoverageLogDataType(LogDataType.NATIVE_COVERAGE);
        try {
            mForwarder.run(mTestInfo, mMockListener);
            fail("Should have thrown an exception.");
        } catch (RuntimeException e) {
            // Expected
        }
        assertThat(mCapturedJavaLogs).isEmpty();
        assertThat(mCapturedNativeLogs).isEmpty();
    }

    private File mockBuildArtifact(ByteString contents) throws IOException {
        File ret = mFolder.newFile();
        try (OutputStream out = new FileOutputStream(ret)) {
            contents.writeTo(out);
        }
        return ret;
    }
}

