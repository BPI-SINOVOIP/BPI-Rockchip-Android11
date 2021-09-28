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
package com.android.tradefed.invoker.logger;

import com.android.tradefed.invoker.ExecutionFiles;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ActionInProgress;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.error.ErrorIdentifier;

import java.io.File;
import java.lang.StackWalker.Option;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

import javax.annotation.Nullable;

/**
 * A class that tracks and provides the current invocation information useful anywhere inside the
 * invocation.
 */
public class CurrentInvocation {

    /** Some special named key that we will always populate for the invocation. */
    public enum InvocationInfo {
        WORK_FOLDER("work_folder");

        private final String mKeyName;

        private InvocationInfo(String key) {
            mKeyName = key;
        }

        @Override
        public String toString() {
            return mKeyName;
        }
    }

    private CurrentInvocation() {}

    /** Internal storage of the invocation values. */
    private static class InternalInvocationTracking {
        public Map<InvocationInfo, File> mInvocationInfoFiles = new HashMap<>();
        public ExecutionFiles mExecutionFiles;
        public ActionInProgress mActionInProgress = ActionInProgress.UNSET;
    }

    /**
     * Track info per ThreadGroup as a proxy to invocation since an invocation run within one
     * threadgroup.
     */
    private static final Map<ThreadGroup, InternalInvocationTracking> mPerGroupInfo =
            new ConcurrentHashMap<ThreadGroup, CurrentInvocation.InternalInvocationTracking>();

    private static final Map<ThreadGroup, Map<InvocationLocal<?>, Optional<?>>> mInvocationLocals =
            new ConcurrentHashMap<>();

    /**
     * Add one key-value to be tracked at the invocation level.
     *
     * @param key The key under which the invocation info will be tracked.
     * @param value The value of the invocation metric.
     */
    public static void addInvocationInfo(InvocationInfo key, File value) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                mPerGroupInfo.put(group, new InternalInvocationTracking());
            }
            mPerGroupInfo.get(group).mInvocationInfoFiles.put(key, value);
        }
    }

    /** Returns the Map of invocation metrics for the invocation in progress. */
    public static File getInfo(InvocationInfo key) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                mPerGroupInfo.put(group, new InternalInvocationTracking());
            }
            return mPerGroupInfo.get(group).mInvocationInfoFiles.get(key);
        }
    }

    /** Clear the invocation info for an invocation. */
    public static void clearInvocationInfos() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            mPerGroupInfo.remove(group);
        }
        mInvocationLocals.remove(group);
    }

    /**
     * One-time registration of the {@link ExecutionFiles}. This is done by the Test Harness.
     *
     * @param invocFiles The registered {@link ExecutionFiles}.
     */
    public static void registerExecutionFiles(ExecutionFiles invocFiles) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                mPerGroupInfo.put(group, new InternalInvocationTracking());
            }
            if (mPerGroupInfo.get(group).mExecutionFiles == null) {
                mPerGroupInfo.get(group).mExecutionFiles = invocFiles;
            } else {
                CLog.w(
                        "CurrentInvocation#registerExecutionFiles should only be called once "
                                + "per invocation.");
            }
        }
    }

    /** Returns the {@link ExecutionFiles} for the invocation. */
    public static ExecutionFiles getInvocationFiles() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                mPerGroupInfo.put(group, new InternalInvocationTracking());
            }
            return mPerGroupInfo.get(group).mExecutionFiles;
        }
    }

    /** Sets the {@link ActionInProgress} for the invocation. */
    public static void setActionInProgress(ActionInProgress action) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                mPerGroupInfo.put(group, new InternalInvocationTracking());
            }
            mPerGroupInfo.get(group).mActionInProgress = action;
        }
    }

    /** Returns the current {@link ActionInProgress} for the invocation. Can be null. */
    public static @Nullable ActionInProgress getActionInProgress() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        synchronized (mPerGroupInfo) {
            if (mPerGroupInfo.get(group) == null) {
                return null;
            }
            return mPerGroupInfo.get(group).mActionInProgress;
        }
    }

    /**
     * Create a failure associated with the invocation action in progress. Convenience utility to
     * avoid calling {@link FailureDescription#setActionInProgress(ActionInProgress)}.
     */
    public static FailureDescription createFailure(
            String errorMessage, ErrorIdentifier errorIdentifier) {
        FailureDescription failure = FailureDescription.create(errorMessage);
        ActionInProgress action = getActionInProgress();
        if (action != null) {
            failure.setActionInProgress(action);
        }
        if (errorIdentifier != null) {
            failure.setErrorIdentifier(errorIdentifier);
        }
        // Automatically populate the origin
        Class<?> clazz = StackWalker.getInstance(Option.RETAIN_CLASS_REFERENCE).getCallerClass();
        failure.setOrigin(clazz.getCanonicalName());
        return failure;
    }

    static <T> T getLocal(InvocationLocal<T> local) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        Map<InvocationLocal<?>, Optional<?>> locals =
                mInvocationLocals.computeIfAbsent(group, unused -> new ConcurrentHashMap<>());

        // Note that ConcurrentHashMap guarantees that the function is atomic and called at-most
        // once.
        Optional<?> holder =
                locals.computeIfAbsent(local, unused -> Optional.ofNullable(local.initialValue()));

        @SuppressWarnings("unchecked")
        T value = (T) holder.orElse(null);
        return value;
    }
}
