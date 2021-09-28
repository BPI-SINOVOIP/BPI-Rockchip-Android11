/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tradefed.device.metric;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.VtsPythonRunnerHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
/**
 * Unit tests for {@link VtsCoverageCollector}.
 */
@RunWith(JUnit4.class)
public class VtsCoverageCollectorTest {
    private Map<String, String> mBuildAttributes = new HashMap<>();
    private List<IBuildInfo> mBuildInfos = new ArrayList<IBuildInfo>();
    private List<ITestDevice> mDevices = new ArrayList<ITestDevice>();

    @Mock ITestInvocationListener mMockListener;
    @Mock IInvocationContext mMockContext;
    @Mock IBuildInfo mBuildInfo;
    @Mock ITestDevice mDevice;
    @Mock VtsPythonRunnerHelper mMockRunner;
    @InjectMocks VtsCoverageCollector mCollector = new testCollector();

    private class testCollector extends VtsCoverageCollector {
        @Override
        public String getRunName() {
            return "device1 testRun";
        }
        @Override
        String getGcoveResrouceDir(IBuildInfo buildInfo, ITestDevice device) {
            return "/tmp/test-coverage";
        }
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mCollector.init(mMockContext, mMockListener);
        mCollector.setPythonRunnerHelper(mMockRunner);
        mDevices.add(mDevice);
        mBuildInfos.add(mBuildInfo);
        doReturn(mDevices).when(mMockContext).getDevices();
        doReturn(mBuildInfos).when(mMockContext).getBuildInfos();
    }

    @Test
    public void testOnTestRunEndBasic() throws Exception {
        mBuildAttributes.put("coverage_report_path", "/tmp/test-coverage/");
        doReturn("1234").when(mDevice).getSerialNumber();
        doReturn(mBuildAttributes).when(mBuildInfo).getBuildAttributes();

        String expectedCmdStr = "python -m vts.utils.python.coverage.coverage_utils "
                + "get_coverage --serial 1234 --gcov_rescource_path /tmp/test-coverage "
                + "--report_path /tmp/test-coverage/device1_testRun "
                + "--report_prefix device1_testRun";
        String[] expected_cmd = expectedCmdStr.split("\\s+");

        doReturn(null)
                .when(mMockRunner)
                .runPythonRunner(Mockito.eq(expected_cmd), Mockito.any(), Mockito.anyLong());

        mCollector.onTestRunEnd(null, null);

        verify(mMockRunner).runPythonRunner(Mockito.any(), Mockito.any(), Mockito.anyLong());
    }

    @Test
    public void testOnTestRunEndReportDirMissing() throws Exception {
        mBuildAttributes.remove("coverage_report_path");
        doReturn("1234").when(mDevice).getSerialNumber();
        doReturn(mBuildAttributes).when(mBuildInfo).getBuildAttributes();
        mCollector.onTestRunEnd(null, null);
        // Ensure the properties was requested
        verify(mBuildInfo).getBuildAttributes();
    }
}
