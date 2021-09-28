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
 * limitations under the License
 */

package android.packagewatchdog.cts;

import static com.google.common.truth.Truth.assertThat;

import android.os.RemoteCallback;
import android.service.watchdog.ExplicitHealthCheckService;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.concurrent.CountDownLatch;

@RunWith(AndroidJUnit4.class)
public final class ExplicitHealthCheckServiceTest {

    private ExplicitHealthCheckService mExplicitHealthCheckService;
    private static final String PACKAGE_NAME = "com.test.package";
    private static final String EXTRA_HEALTH_CHECK_PASSED_PACKAGE =
            "android.service.watchdog.extra.health_check_passed_package";

    @Before
    public void setup() throws Exception {
        mExplicitHealthCheckService = new FakeExplicitHealthCheckService();
    }

    /**
     * Test to verify that the correct information is sent in the callback when a package has passed
     * an explicit health check.
     */
    @Test
    public void testNotifyHealthCheckPassed() throws Exception {
        CountDownLatch countDownLatch = new CountDownLatch(1);
        RemoteCallback callback = new RemoteCallback(result -> {
            assertThat(result.get(EXTRA_HEALTH_CHECK_PASSED_PACKAGE)).isEqualTo(PACKAGE_NAME);
            countDownLatch.countDown();
        });
        mExplicitHealthCheckService.setCallback(callback);
        mExplicitHealthCheckService.notifyHealthCheckPassed(PACKAGE_NAME);
        countDownLatch.await();
    }

    private static class FakeExplicitHealthCheckService extends ExplicitHealthCheckService {
        @Override
        public void onRequestHealthCheck(String packageName) {
            // do nothing
        }

        @Override
        public void onCancelHealthCheck(String packageName) {
            // do nothing
        }

        @Override
        public List<PackageConfig> onGetSupportedPackages() {
            // do nothing
            return null;
        }

        @Override
        public List<String> onGetRequestedPackages() {
            // do nothing
            return null;
        }
    }
}
