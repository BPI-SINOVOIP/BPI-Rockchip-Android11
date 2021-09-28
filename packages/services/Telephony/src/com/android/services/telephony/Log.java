/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.services.telephony;

/**
 * Manages logging for the entire module.
 */
final public class Log {

    // Generic tag for all In Call logging
    private static final String TAG = "Telephony";

    public static final boolean FORCE_LOGGING = false; /* STOP SHIP if true */
    public static final boolean DEBUG = isLoggable(android.util.Log.DEBUG);
    public static final boolean INFO = isLoggable(android.util.Log.INFO);
    public static final boolean VERBOSE = isLoggable(android.util.Log.VERBOSE);
    public static final boolean WARN = isLoggable(android.util.Log.WARN);
    public static final boolean ERROR = isLoggable(android.util.Log.ERROR);

    private Log() {}

    public static boolean isLoggable(int level) {
        return FORCE_LOGGING || android.util.Log.isLoggable(TAG, level);
    }

    // Relay log messages to Telecom
    // TODO: Redo namespace of Telephony to use these methods directly.

    public static void d(String prefix, String format, Object... args) {
        android.util.Log.d(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args));
    }

    public static void d(Object objectPrefix, String format, Object... args) {
        android.util.Log.d(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args));
    }

    public static void i(String prefix, String format, Object... args) {
        android.util.Log.i(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args));
    }

    public static void i(Object objectPrefix, String format, Object... args) {
        android.util.Log.i(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args));
    }

    public static void v(String prefix, String format, Object... args) {
        android.util.Log.v(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args));
    }

    public static void v(Object objectPrefix, String format, Object... args) {
        android.util.Log.v(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args));
    }

    public static void w(String prefix, String format, Object... args) {
        android.util.Log.w(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args));
    }

    public static void w(Object objectPrefix, String format, Object... args) {
        android.util.Log.w(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args));
    }

    public static void e(String prefix, Throwable tr, String format, Object... args) {
        android.util.Log.e(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args), tr);
    }

    public static void e(Object objectPrefix, Throwable tr, String format, Object... args) {
        android.util.Log.e(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args), tr);
    }

    public static void wtf(String prefix, Throwable tr, String format, Object... args) {
        android.util.Log.wtf(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args), tr);
    }

    public static void wtf(Object objectPrefix, Throwable tr, String format, Object... args) {
        android.util.Log.wtf(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args), tr);
    }

    public static void wtf(String prefix, String format, Object... args) {
        android.util.Log.wtf(TAG, (args == null || args.length == 0) ? format :
                String.format(prefix + ": " + format, args));
    }

    public static void wtf(Object objectPrefix, String format, Object... args) {
        android.util.Log.wtf(TAG, (args == null || args.length == 0) ? format :
                String.format(getPrefixFromObject(objectPrefix) + ": " + format, args));
    }

    private static String getPrefixFromObject(Object obj) {
        return obj == null ? "<null>" : obj.getClass().getSimpleName();
    }
}
