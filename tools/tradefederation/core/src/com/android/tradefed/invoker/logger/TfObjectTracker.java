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

import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.postprocessor.IPostProcessor;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.testtype.IRemoteTest;

import com.google.common.collect.ImmutableSet;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/** A utility to track the usage of the different Trade Fedederation objects. */
public class TfObjectTracker {

    public static final String TF_OBJECTS_TRACKING_KEY = "tf_objects_tracking";
    private static final Set<Class<?>> TRACKED_CLASSES =
            ImmutableSet.of(
                    IBuildProvider.class,
                    IMetricCollector.class,
                    IMultiTargetPreparer.class,
                    IPostProcessor.class,
                    IRemoteTest.class,
                    ITargetPreparer.class);

    private TfObjectTracker() {}

    private static final Map<ThreadGroup, Map<String, Long>> mPerGroupUsage =
            new ConcurrentHashMap<ThreadGroup, Map<String, Long>>();

    /** Count the occurrence of a give class and its super classes until the Tradefed interface. */
    public static void countWithParents(Class<?> object) {
        if (!count(object)) {
            return;
        }
        // Track all the super class until not a TF interface to get a full picture.
        countWithParents(object.getSuperclass());
    }

    /**
     * Count explicitly one class and its occurrences
     *
     * @param className The object to track
     * @param occurrences current num of known occurrences
     */
    public static void directCount(String className, long occurrences) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        if (mPerGroupUsage.get(group) == null) {
            mPerGroupUsage.put(group, new ConcurrentHashMap<>());
        }
        Map<String, Long> countMap = mPerGroupUsage.get(group);
        long count = 0;
        if (countMap.get(className) != null) {
            count = countMap.get(className);
        }
        count += occurrences;
        countMap.put(className, count);
    }

    /**
     * Count the current occurrence only if it's part of the tracked objects.
     *
     * @param object The object to track
     * @return True if the object was tracked, false otherwise.
     */
    private static boolean count(Class<?> object) {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        String qualifiedName = object.getName();

        boolean tracked = false;
        for (Class<?> classTracked : TRACKED_CLASSES) {
            if (classTracked.isAssignableFrom(object)) {
                tracked = true;
                break;
            }
        }
        if (!tracked) {
            return false;
        }
        // Don't track internal classes for now but return true to track subclass if needed.
        if (qualifiedName.contains("$")) {
            return true;
        }
        if (mPerGroupUsage.get(group) == null) {
            mPerGroupUsage.put(group, new ConcurrentHashMap<>());
        }
        Map<String, Long> countMap = mPerGroupUsage.get(group);
        long count = 0;
        if (countMap.get(qualifiedName) != null) {
            count = countMap.get(qualifiedName);
        }
        count++;
        countMap.put(qualifiedName, count);
        return true;
    }

    /** Returns the usage of the tracked objects. */
    public static Map<String, Long> getUsage() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        if (mPerGroupUsage.get(group) == null) {
            mPerGroupUsage.put(group, new ConcurrentHashMap<>());
        }
        return new HashMap<>(mPerGroupUsage.get(group));
    }

    /** Stop tracking the current invocation. This is called automatically by the harness. */
    public static void clearTracking() {
        ThreadGroup group = Thread.currentThread().getThreadGroup();
        mPerGroupUsage.remove(group);
    }
}
