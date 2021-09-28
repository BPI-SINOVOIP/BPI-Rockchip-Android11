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
package com.android.tradefed.invoker;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;

import com.google.common.collect.ImmutableMap;

import java.io.File;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.ConcurrentSkipListSet;

/**
 * Files dependencies generated during the execution of a test or invocation that need to be carried
 * for testing. This object is shared by all the invocation (tests, modules, etc.).
 */
public class ExecutionFiles {

    /** Enumeration of known standard key for the map. */
    public static enum FilesKey {
        ADB_BINARY,
        // Describes the directory containing the build target tests artifacts.
        TESTS_DIRECTORY,
        // Sub-directory of TESTS_DIRECTORY that contains target artifacts
        TARGET_TESTS_DIRECTORY,
        // Sub-directory of TESTS_DIRECTORY that contains host-side artifacts
        HOST_TESTS_DIRECTORY
    }

    private final ConcurrentMap<String, File> mFiles = new ConcurrentHashMap<>();
    private final ConcurrentSkipListSet<String> mShouldNotDelete = new ConcurrentSkipListSet<>();

    // Package private to avoid new instantiation.
    ExecutionFiles() {}

    /**
     * Associates the specified value with the specified key in this map.
     *
     * @param key key with which the specified value is to be associated
     * @param value value to be associated with the specified key
     * @return the previous value associated with {@code key}, or {@code null} if there was no
     *     mapping for {@code key}.
     * @see ConcurrentMap
     */
    public File put(String key, File value) {
        return mFiles.put(key, value);
    }

    /**
     * Variation of {@link #put(String, File)} with a known key.
     *
     * @param key key with which the specified value is to be associated
     * @param value value to be associated with the specified key
     * @return the previous value associated with {@code key}, or {@code null} if there was no
     *     mapping for {@code key}.
     */
    public File put(FilesKey key, File value) {
        return mFiles.put(key.toString(), value);
    }

    /**
     * Variation of {@link #put(FilesKey, File)} with option to prevent the file from being deleted
     * at the end of the invocation.
     *
     * @param key key with which the specified value is to be associated
     * @param value value to be associated with the specified key
     * @param shouldNotDelete prevent the file from being deleted at the end of invocation.
     * @return the previous value associated with {@code key}, or {@code null} if there was no
     *     mapping for {@code key}.
     */
    public File put(FilesKey key, File value, boolean shouldNotDelete) {
        File f = mFiles.put(key.toString(), value);
        if (shouldNotDelete) {
            mShouldNotDelete.add(key.toString());
        } else {
            mShouldNotDelete.remove(key.toString());
        }
        if (f != null) {
            CLog.w("Replaced key '%s' with value '%s' by '%s'", key, f, value);
        }
        return f;
    }

    /**
     * If the specified key is not already associated with a value, associates it with the given
     * value.
     *
     * @param key key with which the specified value is to be associated
     * @param value value to be associated with the specified key
     * @return the previous value associated with the specified key, or {@code null} if there was no
     *     mapping for the key.
     */
    public File putIfAbsent(String key, File value) {
        return mFiles.putIfAbsent(key, value);
    }

    /**
     * Copies all of the mappings from the specified map to this map.
     *
     * @param properties mappings to be stored in this map
     * @return The final mapping
     */
    public ExecutionFiles putAll(Map<String, File> properties) {
        mFiles.putAll(properties);
        return this;
    }

    /**
     * Copies all of the mappings from the specified map to this map.
     *
     * @param copyFrom original {@link ExecutionFiles} to copy from.
     * @return The final mapping
     */
    public ExecutionFiles putAll(ExecutionFiles copyFrom) {
        mFiles.putAll(copyFrom.getAll());
        mShouldNotDelete.addAll(copyFrom.mShouldNotDelete);
        return this;
    }

    /** Returns whether or not the map of properties is empty. */
    public boolean isEmpty() {
        return mFiles.isEmpty();
    }

    /**
     * Returns the value to which the specified key is mapped, or {@code null} if this map contains
     * no mapping for the key.
     *
     * @param key the key whose associated value is to be returned
     * @return the value to which the specified key is mapped, or {@code null} if this map contains
     *     no mapping for the key
     */
    public File get(String key) {
        return mFiles.get(key);
    }

    /**
     * Variation of {@link #get(String)} with a known key.
     *
     * @param key the key whose associated value is to be returned
     * @return the value to which the specified key is mapped, or {@code null} if this map contains
     *     no mapping for the key
     */
    public File get(FilesKey key) {
        return mFiles.get(key.toString());
    }

    /**
     * Returns {@code true} if this map contains a mapping for the specified key.
     *
     * @param key key whose presence in this map is to be tested
     * @return {@code true} if this map contains a mapping for the specified key
     */
    public boolean containsKey(String key) {
        return mFiles.containsKey(key);
    }

    /** Returns all the properties in a copy of the map */
    public ImmutableMap<String, File> getAll() {
        return ImmutableMap.copyOf(mFiles);
    }

    /**
     * Removes the mapping for a key from this map if it is present (optional operation).
     *
     * @param key key whose mapping is to be removed from the map
     * @return the previous value associated with {@code key}, or {@code null} if there was no
     *     mapping for {@code key}.
     */
    public File remove(String key) {
        return mFiles.remove(key);
    }

    /** Delete all the files that are tracked and not marked as 'should not delete'. */
    public void clearFiles() {
        for (String key : mFiles.keySet()) {
            if (mShouldNotDelete.contains(key)) {
                continue;
            }
            FileUtil.recursiveDelete(mFiles.get(key));
            mFiles.remove(key);
        }
    }
}
