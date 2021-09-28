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
package com.android.tradefed.config.proxy;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.result.proto.StreamProtoResultReporter;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link AutomatedReporters}. */
@RunWith(JUnit4.class)
public class AutomatedReportersTest {

    private AutomatedReporters mReporter =
            new AutomatedReporters() {
                @Override
                protected String getEnv(String key) {
                    if (key.equals("PROTO_REPORTING_PORT")) {
                        return "8888";
                    }
                    return null;
                }
            };

    @Test
    public void testReporting() {
        IConfiguration config = new Configuration("name", "test");
        assertEquals(1, config.getTestInvocationListeners().size());
        mReporter.applyAutomatedReporters(config);
        assertEquals(2, config.getTestInvocationListeners().size());
        assertTrue(config.getTestInvocationListeners().get(1) instanceof StreamProtoResultReporter);
        StreamProtoResultReporter protoReporter =
                (StreamProtoResultReporter) config.getTestInvocationListeners().get(1);
        assertEquals(Integer.valueOf(8888), protoReporter.getProtoReportPort());
    }
}
