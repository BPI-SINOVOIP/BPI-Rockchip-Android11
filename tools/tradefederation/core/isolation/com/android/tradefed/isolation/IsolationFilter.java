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
package com.android.tradefed.isolation;

import org.junit.runner.Description;
import org.junit.runner.manipulation.Filter;

import java.lang.annotation.Annotation;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * Implements JUnit's Filter interface to allow TradeFed style filters to be evaluated during
 * execution of JUnit tests. Very similar to TradeFed's internal FilterHelper, but freed of the
 * dependencies that one has.
 */
final class IsolationFilter extends Filter {
    private Set<String> mIncludeFilters = new HashSet<>();
    private Set<String> mExcludeFilters = new HashSet<>();
    private Set<String> mIncludeAnnotations = new HashSet<>();
    private Set<String> mExcludeAnnotations = new HashSet<>();

    public IsolationFilter(
            Collection<String> includeFilters,
            Collection<String> excludeFilters,
            Collection<String> includeAnnotations,
            Collection<String> excludeAnnotations) {
        mIncludeFilters = new HashSet<String>(includeFilters);
        mExcludeFilters = new HashSet<String>(excludeFilters);
        mIncludeAnnotations = new HashSet<String>(includeAnnotations);
        mExcludeAnnotations = new HashSet<String>(excludeAnnotations);
    }

    public IsolationFilter(FilterSpec filter) {
        this(
                filter.getIncludeFiltersList(),
                filter.getExcludeFiltersList(),
                filter.getIncludeAnnotationsList(),
                filter.getExcludeAnnotationsList());
    }

    public IsolationFilter() {}

    @Override
    public String describe() {
        return "Tradefed filtering based on name and annotations.";
    }

    @Override
    public boolean shouldRun(Description desc) {
        return this.checkFilters(desc) && this.checkAnnotations(desc);
    }

    private boolean checkFilters(Description desc) {
        String packageName = getPackageName(desc);
        String className = desc.getClassName();
        String methodName = String.format("%s#%s", className, desc.getMethodName());
        return this.checkIncludeFilters(packageName, className, methodName)
                && this.checkExcludeFilters(packageName, className, methodName);
    }

    private boolean checkAnnotations(Description desc) {
        Collection<Annotation> annotations = desc.getAnnotations();
        return this.checkIncludeAnnotations(annotations)
                && this.checkExcludeAnnotations(annotations);
    }

    private boolean checkIncludeFilters(String packageName, String className, String methodName) {
        return mIncludeFilters.isEmpty()
                || mIncludeFilters.contains(methodName)
                || mIncludeFilters.contains(className)
                || mIncludeFilters.contains(packageName);
    }

    private boolean checkExcludeFilters(String packageName, String className, String methodName) {
        return !(mExcludeFilters.contains(methodName)
                || mExcludeFilters.contains(className)
                || mExcludeFilters.contains(packageName));
    }

    private boolean checkIncludeAnnotations(Collection<Annotation> annotationsList) {
        if (!mIncludeAnnotations.isEmpty()) {
            Set<String> neededAnnotation = new HashSet<String>();
            neededAnnotation.addAll(mIncludeAnnotations);
            for (Annotation a : annotationsList) {
                if (neededAnnotation.contains(a.annotationType().getName())) {
                    neededAnnotation.remove(a.annotationType().getName());
                }
            }
            if (!neededAnnotation.isEmpty()) {
                // The test needs to have all the include annotation to pass.
                return false;
            }
        }
        return true;
    }

    private boolean checkExcludeAnnotations(Collection<Annotation> annotationsList) {
        if (!mExcludeAnnotations.isEmpty()) {
            for (Annotation a : annotationsList) {
                if (mExcludeAnnotations.contains(a.annotationType().getName())) {
                    // If any of the method annotation match an ExcludeAnnotation, don't run it
                    return false;
                }
            }
        }
        return true;
    }

    private String getPackageName(Description desc) {
        Class<?> classObj = null;
        try {
            classObj = Class.forName(desc.getClassName());
        } catch (ClassNotFoundException e) {
            throw new IllegalArgumentException(
                    String.format("Could not load Test class %s", classObj), e);
        }
        return classObj.getPackage().getName();
    }
}
