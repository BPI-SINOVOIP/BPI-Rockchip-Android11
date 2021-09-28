/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.platform.helpers;

import static java.lang.reflect.Modifier.isAbstract;
import static java.lang.reflect.Modifier.isInterface;

import android.app.Instrumentation;
import android.content.Context;
import android.platform.helpers.exceptions.MappedMultiException;
import android.platform.helpers.exceptions.TestHelperException;
import android.util.Log;

import dalvik.system.DexFile;
import dalvik.system.PathClassLoader;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

/**
 * The HelperManager class is used to remove any explicit or hard-coded dependencies on app helper
 * implementations. Instead, it provides a method for abstracting helper instantiation by specifying
 * a base class for which you require an implementation. The manager effectively searches for a
 * suitable implementation using runtime class loading.
 * <p>
 * The class provides two means for finding the necessary classes:
 * <ol>
 * <li> Static inclusion - if all the code is included in the final APK, the Context can be used to
 * generate a HelperManager and to instantiate implementations.
 * <li> Dexed file inclusion - if this manager and the helper implementations are bundled into dex
 * files and loaded from a single class loader, then the files can be used to generate a
 * HelperManager and to instantiate implementations. Use of this is discouraged.
 * </ol>
 * <p>
 * Including and using this strategy will prune the explicit dependency tree for the App Helper
 * Library and provide a more robust library for use across the Android source tree.
 */
public class HelperManager {
    public static final String NO_MATCH_ERROR_MESSAGE = "No matching implementations";

    private static final String LOG_TAG = HelperManager.class.getSimpleName();
    private static HelperManager sInstance;

    /**
     * Returns an instance of the HelperManager that searches the supplied application context for
     * classes to instantiate to helper implementations.
     *
     * @param context the application context to search
     * @param instr the active instrumentation
     * @return a new instance of the HelperManager class
     */
    public static HelperManager getInstance(Context context, Instrumentation instr) {
        if (sInstance == null) {
            // Input checks
            if (context == null) {
                throw new NullPointerException("Cannot pass in a null context.");
            }
            if (instr == null) {
                throw new NullPointerException(
                        String.format("Cannot pass in null instrumentation."));
            }
            // Instantiation
            List<String> paths = Arrays.asList(context.getPackageCodePath());
            sInstance = new HelperManager(paths, instr);
        }
        return sInstance;
    }

    /**
     * Returns an instance of the HelperManager that searches the supplied locations for classes to
     * instantiate to helper implementations.
     *
     * @param paths the dex files where the classes are included
     * @param instr the active instrumentation
     * @throws IllegalArgumentException if the path is not a valid file
     * @return a new instance of the HelperManager class
     */
    public static HelperManager getInstance(List<String> paths, Instrumentation instr) {
        if (sInstance == null) {
            // Input checks
            for (String path : paths) {
                if (!(new File(path)).exists()) {
                    throw new IllegalArgumentException(
                            String.format("No file found at path: %s.", path));
                }
            }
            if (instr == null) {
                throw new NullPointerException(
                        String.format("Cannot pass in null instrumentation."));
            }
            // Instantiation
            sInstance = new HelperManager(paths, instr);
        }
        return sInstance;
    }

    private Instrumentation mInstrumentation;
    private List<String> mClasses;
    private ClassLoader mLoader;

    private HelperManager(List<String> paths, Instrumentation instr) {
        mInstrumentation = instr;
        // Collect all of the available classes
        mClasses = new ArrayList<String>();
        try {
            for (String path : paths) {
                DexFile dex = new DexFile(path);
                mClasses.addAll(Collections.list(dex.entries()));
            }
            mLoader =
                    new PathClassLoader(
                            String.join(":", paths), HelperManager.class.getClassLoader());
        } catch (IOException e) {
            throw new TestHelperException("Failed to retrieve the dex file.");
        }
    }

    /**
     * Returns a concrete implementation of the helper interface supplied, if available.
     *
     * @param base the interface base class to find an implementation for
     * @throws TestHelperException if no implementation is found
     * @return a concrete implementation of base
     */
    public <T extends ITestHelper> T get(Class<T> base) {
        return get(base, "");
    }

    /**
     * Returns a concrete implementation of the helper interface supplied, if available.
     *
     * @param base the interface base class to find an implementation for
     * @param keyword a keyword for matching the helper implementation, if multiple exist
     * @throws TestHelperException if no implementation is found
     * @return a list of all concrete implementations we could find
     */
    public <T extends ITestHelper> T get(Class<T> base, String keyword) {
        List<T> matching = getAll(base, keyword);
        Log.i(
                LOG_TAG,
                String.format("Selecting implementation %s", matching.get(0).getClass().getName()));
        return matching.get(0);
    }

    /**
     * Returns a concrete implementation of the helper interface supplied, if available.
     *
     * @param base the interface base class to find an implementation for
     * @param regex a regular expression for matching the helper implementation, if multiple exist
     * @throws TestHelperException if no implementation is found
     * @return a list of all concrete implementations we could find
     */
    public <T extends ITestHelper> T get(Class<T> base, Pattern regex) {
        List<T> matching = getAll(base, regex);
        Log.i(
                LOG_TAG,
                String.format("Selecting implementation %s", matching.get(0).getClass().getName()));
        return matching.get(0);
    }

    /**
     * Returns a concrete implementation of the helper interface supplied, if available.
     *
     * @param base the interface base class to find an implementation for
     * @param keyword a keyword for matching the helper implementation, if multiple exist
     * @throws TestHelperException if no implementation is found
     * @return a concrete implementation of base
     */
    private <T extends ITestHelper> List<T> getAll(Class<T> base, String keyword) {
        Pattern p = Pattern.compile(".*\\Q" + keyword + "\\E.*");
        return getAll(base, p);
    }

    /**
     * Returns a concrete implementation of the helper interface supplied, if available.
     *
     * @param base the interface base class to find an implementation for
     * @param regex a regular expression for matching the helper implementation, if multiple exist
     * @throws TestHelperException if no implementation is found
     * @return a concrete implementation of base
     */
    private <T extends ITestHelper> List<T> getAll(Class<T> base, Pattern regex) {
        List<T> implementations = new ArrayList<>();
        Map<Object, Throwable> mappedExceptions = new HashMap<>();

        // Iterate and search for the implementation
        for (String className : mClasses) {
            Class<?> clazz = null;
            try {
                clazz = mLoader.loadClass(className);
                // Skip non-instantiable classes
                if (isAbstract(clazz.getModifiers()) || isInterface(clazz.getModifiers())) {
                    continue;
                }
            } catch (ClassNotFoundException e) {
                Log.w(LOG_TAG, String.format("Class not found: %s", className));
                continue;
            }
            if (base.isAssignableFrom(clazz)
                    && !clazz.equals(base)
                    && regex.matcher(className).matches()) {
                // Instantiate the implementation class and return
                try {
                    Constructor<?> constructor = clazz.getConstructor(Instrumentation.class);
                    implementations.add((T)constructor.newInstance(mInstrumentation));
                } catch (NoSuchMethodException e) {
                    mappedExceptions.put(
                            clazz,
                            wrapThrowable(
                                    String.format(
                                            "Failed to find a matching constructor for %s",
                                            className),
                                    e));
                } catch (IllegalAccessException e) {
                    mappedExceptions.put(
                            clazz,
                            wrapThrowable(
                                    String.format("Failed to access the constructor %s", className),
                                    e));
                } catch (InstantiationException e) {
                    mappedExceptions.put(
                            clazz,
                            wrapThrowable(String.format("Failed to instantiate %s", className), e));
                } catch (InvocationTargetException e) {
                    mappedExceptions.put(
                            clazz,
                            wrapThrowable(
                                    String.format(
                                            "Exception encountered instantiating %s", className),
                                    e));
                }
            }
        }

        if (implementations.isEmpty()) {
            if (mappedExceptions.isEmpty()) {
                throw new TestHelperException(
                        String.format(
                                "Could not find an implementation for %s. %s.",
                                base, NO_MATCH_ERROR_MESSAGE));
            }
            throw new MappedMultiException(
                    String.format(
                            "Could not find an implementation for %s. "
                                    + "Instantiation for all candidates failed. "
                                    + "Please look at the error messages below to determine why.",
                            base),
                    mappedExceptions);
        }

        Log.d(
                LOG_TAG,
                String.format(
                        "Found matching implementations: %s.",
                        implementations
                                .stream()
                                .map(i -> i.getClass().getName())
                                .collect(Collectors.toList())));

        return implementations;
    }

    /** Wrap the {@link Throwable} in a {@link TestHelperException} with a custom error message. */
    private TestHelperException wrapThrowable(String message, Throwable t) {
        Throwable causeIfPresent = getCauseIfPresent(t);
        // According to the documentation of RuntimeException the message for the causing Throwable
        // needs to be included explicitly.
        TestHelperException re =
                new TestHelperException(
                        String.format("%s:\n%s.", message, causeIfPresent.getMessage()),
                        causeIfPresent);
        return re;
    }

    /** Get the cause of a {@link Throwable}, or itself if one is not present. */
    private Throwable getCauseIfPresent(Throwable t) {
        return t.getCause() == null ? t : t.getCause();
    }
}
