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
package android.cts.statsd.validation;

import android.cts.statsd.atom.BaseTestCase;

import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;

import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

import java.io.File;
import java.io.IOException;

public class ValidationTestUtil extends BaseTestCase {

    private static final String TAG = "Statsd.ValidationTestUtil";

    public StatsdConfig getConfig(String fileName) throws IOException {
        try {
            // TODO: Ideally, we should use real metrics that are also pushed to the fleet.
            File configFile = getBuildHelper().getTestFile(fileName);
            String configStr = FileUtil.readStringFromFile(configFile);
            StatsdConfig.Builder builder = StatsdConfig.newBuilder();
            TextFormat.merge(configStr, builder);
            return builder.build();
        } catch (ParseException e) {
            LogUtil.CLog.e(
                    "Failed to parse the config! line: " + e.getLine() + " col: " + e.getColumn(),
                    e);
        }
        return null;
    }
}
