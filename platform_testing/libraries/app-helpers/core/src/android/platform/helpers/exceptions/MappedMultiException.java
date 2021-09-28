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

package android.platform.helpers.exceptions;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * Custom exception that holds exceptions from multiple classes and print them all out upon
 * throwing.
 */
public class MappedMultiException extends TestHelperException {
    private final Map<Object, Throwable> mThrowables = new HashMap<>();
    private final String mMessage;

    public MappedMultiException(String message, Map<Object, Throwable> throwables) {
        super(message);
        mThrowables.putAll(throwables);
        mMessage = message;
    }

    /** Print a stack trace from a Throwable to a string. */
    private String stackTraceToString(Throwable t) {
        StringWriter stringWriter = new StringWriter();
        t.printStackTrace(new PrintWriter(stringWriter));
        // Indent the output by one level.
        return stringWriter.toString().replaceAll("(?m)^", "\t");
    }

    /** Print an entry in the exception map to a string. */
    private String entryToString(Map.Entry<Object, Throwable> entry) {
        return String.format("%s:\n%s", entry.getKey(), stackTraceToString(entry.getValue()));
    }

    /** Print out all the exceptions in this exception in its message, if any. */
    @Override
    public String getMessage() {
        if (mThrowables.isEmpty()) {
            return mMessage;
        }
        return String.format(
                "%s\nExceptions and their sources: \n%s",
                mMessage,
                String.join(
                        "\n,",
                        mThrowables
                                .entrySet()
                                .stream()
                                .map(entry -> entryToString(entry))
                                .collect(Collectors.toList())
                                .toArray(new String[mThrowables.size()])));
    }
}
