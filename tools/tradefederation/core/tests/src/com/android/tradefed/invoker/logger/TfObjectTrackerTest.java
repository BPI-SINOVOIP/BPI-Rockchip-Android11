/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.invoker.logger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.TestAppInstallSetup;
import com.android.tradefed.targetprep.suite.SuiteApkInstaller;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Map;
import java.util.UUID;

/** Unit tests for {@link TfObjectTracker}. */
@RunWith(JUnit4.class)
public class TfObjectTrackerTest {

    @Test
    public void testNotTracking() throws Exception {
        String uuid = UUID.randomUUID().toString();
        String group = "unit-test-group-" + uuid;
        ThreadGroup testGroup = new ThreadGroup(group);
        Map<String, Long> metrics = logMetric(testGroup, true, CommandOptions.class);
        assertTrue(metrics.isEmpty());
    }

    @Test
    public void testTrackingWithParents() throws Exception {
        String uuid = UUID.randomUUID().toString();
        String group = "unit-test-group-" + uuid;
        ThreadGroup testGroup = new ThreadGroup(group);
        Map<String, Long> metrics = logMetric(testGroup, true, SuiteApkInstaller.class);
        assertEquals(3, metrics.size());
        assertEquals(1, metrics.get(SuiteApkInstaller.class.getName()).longValue());
        assertEquals(1, metrics.get(TestAppInstallSetup.class.getName()).longValue());
        assertEquals(1, metrics.get(BaseTargetPreparer.class.getName()).longValue());
        // Add a second time
        metrics = logMetric(testGroup, true, SuiteApkInstaller.class);
        assertEquals(3, metrics.size());
        assertEquals(2, metrics.get(SuiteApkInstaller.class.getName()).longValue());
        assertEquals(2, metrics.get(TestAppInstallSetup.class.getName()).longValue());
        assertEquals(2, metrics.get(BaseTargetPreparer.class.getName()).longValue());
    }

    @Test
    public void testTracking() throws Exception {
        String uuid = UUID.randomUUID().toString();
        String group = "unit-test-group-" + uuid;
        ThreadGroup testGroup = new ThreadGroup(group);
        Map<String, Long> metrics = logMetric(testGroup, false, SuiteApkInstaller.class);
        assertEquals(1, metrics.size());
        assertEquals(1, metrics.get(SuiteApkInstaller.class.getName()).longValue());
        assertNull(metrics.get(TestAppInstallSetup.class.getName()));
        assertNull(metrics.get(BaseTargetPreparer.class.getName()));
        // Add a second time
        metrics = logMetric(testGroup, false, SuiteApkInstaller.class);
        assertEquals(1, metrics.size());
        assertEquals(2, metrics.get(SuiteApkInstaller.class.getName()).longValue());
        assertNull(metrics.get(TestAppInstallSetup.class.getName()));
        assertNull(metrics.get(BaseTargetPreparer.class.getName()));
    }

    private class TestObject extends BaseTargetPreparer {}

    @Test
    public void testInternalClass() throws Exception {
        String uuid = UUID.randomUUID().toString();
        String group = "unit-test-group-" + uuid;
        ThreadGroup testGroup = new ThreadGroup(group);
        Map<String, Long> metrics = logMetric(testGroup, true, TestObject.class);
        assertEquals(1, metrics.size());
        // The class itself is not tracked but the sub-classes are.
        assertEquals(1, metrics.get(BaseTargetPreparer.class.getName()).longValue());
    }

    private Map<String, Long> logMetric(
            ThreadGroup testGroup, boolean trackWithParents, Class<?> toTrack) throws Exception {
        TestRunnable runnable = new TestRunnable(toTrack, trackWithParents);
        Thread testThread = new Thread(testGroup, runnable);
        testThread.setName("TfObjectTrackerTest-test-thread");
        testThread.setDaemon(true);
        testThread.start();
        testThread.join(10000);
        return runnable.getUsageMap();
    }

    private class TestRunnable implements Runnable {

        private Class<?> mToTrack;
        private Map<String, Long> mResultMap;
        private boolean mTrackWithParents;

        public TestRunnable(Class<?> toTrack, boolean trackWithParents) {
            mToTrack = toTrack;
            mTrackWithParents = trackWithParents;
        }

        @Override
        public void run() {
            if (mTrackWithParents) {
                TfObjectTracker.countWithParents(mToTrack);
            } else {
                TfObjectTracker.directCount(mToTrack.getName(), 1);
            }
            mResultMap = TfObjectTracker.getUsage();
        }

        public Map<String, Long> getUsageMap() {
            return mResultMap;
        }
    }
}
