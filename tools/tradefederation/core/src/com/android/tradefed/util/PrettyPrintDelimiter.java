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
package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;

/** Helper utility that helps to print delimited message that stands out. */
public final class PrettyPrintDelimiter {

    /** Prints a delimited message given a base String. */
    public static void printStageDelimiter(String message) {
        String limit = "";
        for (int i = 0; i < message.length(); i++) {
            limit += "=";
        }
        String finalFormat = String.format("\n%s\n%s\n%s", limit, message, limit);
        CLog.d(finalFormat);
    }
}
