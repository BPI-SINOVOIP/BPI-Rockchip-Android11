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

import java.util.Map;

/** Mobly yaml result 'User Data' element handler. */
public class MoblyYamlResultUserDataHandler implements IMoblyYamlResultHandler {

    private static final String TIME_STAMP = "timestamp";

    @Override
    public UserData handle(Map<String, Object> docMap) {
        UserData.Builder builder = UserData.builder();
        builder.setTimestamp(String.valueOf(docMap.get(TIME_STAMP)));
        return builder.build();
    }

    public static class UserData implements ITestResult {
        private final long mTimestamp;

        private UserData(String timestamp) {
            mTimestamp = Long.parseLong(timestamp);
        }

        @Override
        public MoblyYamlResultHandlerFactory.Type getType() {
            return MoblyYamlResultHandlerFactory.Type.USER_DATA;
        }

        public long getTimeStamp() {
            return mTimestamp;
        }

        public static Builder builder() {
            return new Builder();
        }

        public static class Builder {
            private String mTimestamp;

            public Builder setTimestamp(String timestamp) {
                mTimestamp = timestamp;
                return this;
            }

            public UserData build() {
                return new UserData(mTimestamp);
            }
        }
    }
}
