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

/** Mobly yaml result 'Summary' element handler. */
public class MoblyYamlResultSummaryHandler implements IMoblyYamlResultHandler {

    private static final String EXECUTED = "Executed";

    @Override
    public Summary handle(Map<String, Object> docMap) {
        Summary.Builder builder = Summary.builder();
        builder.setExecuted(String.valueOf(docMap.get(EXECUTED)));
        return builder.build();
    }

    public static class Summary implements ITestResult {
        private int mExecuted;

        private Summary(String executed) {
            mExecuted = Integer.parseInt(executed);
        }

        @Override
        public MoblyYamlResultHandlerFactory.Type getType() {
            return MoblyYamlResultHandlerFactory.Type.SUMMARY;
        }

        public int getExecuted() {
            return mExecuted;
        }

        public static Builder builder() {
            return new Builder();
        }

        public static class Builder {
            private String mExecuted;

            public Builder setExecuted(String executed) {
                mExecuted = executed;
                return this;
            }

            public Summary build() {
                return new Summary(mExecuted);
            }
        }
    }
}
