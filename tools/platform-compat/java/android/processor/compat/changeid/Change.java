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

package android.processor.compat.changeid;

/**
 * Simple data class that represents a change, built from the code annotations.
 */
final class Change {
    final Long id;
    final String name;
    final boolean disabled;
    final boolean loggingOnly;
    final Integer enabledAfter;
    final String description;
    /**
     * Package name that the change is defined in.
     */
    final String javaPackage;
    /**
     * Name of the class within it's package that the change is defined in.
     */
    final String className;
    /**
     * Fully qualified class name (including the package) that the change is defined in.
     */
    final String qualifiedClass;
    /**
     * Source position, in the form path/to/File.java:line
     */
    final String sourcePosition;

     Change(Long id, String name, boolean disabled, boolean loggingOnly, Integer enabledAfter,
            String description, String javaPackage, String className, String qualifiedClass,
            String sourcePosition) {
        this.id = id;
        this.name = name;
        this.disabled = disabled;
        this.loggingOnly = loggingOnly;
        this.enabledAfter = enabledAfter;
        this.description = description;
        this.javaPackage = javaPackage;
        this.className = className;
        this.qualifiedClass = qualifiedClass;
        this.sourcePosition = sourcePosition;
    }

    public static class Builder {
        Long id;
        String name;
        boolean disabled;
        boolean loggingOnly;
        Integer enabledAfter;
        String description;
        String javaPackage;
        String javaClass;
        String qualifiedClass;
        String sourcePosition;

        Builder() {
        }

        public Builder id(long id) {
            this.id = id;
            return this;
        }

        public Builder name(String name) {
            this.name = name;
            return this;
        }

        public Builder disabled() {
            this.disabled = true;
            return this;
        }

        public Builder loggingOnly() {
            this.loggingOnly = true;
            return this;
        }

        public Builder enabledAfter(int sdkVersion) {
            this.enabledAfter = sdkVersion;
            return this;
        }

        public Builder description(String description) {
            this.description = description;
            return this;
        }

        public Builder javaPackage(String javaPackage) {
            this.javaPackage = javaPackage;
            return this;
        }

        public Builder javaClass(String javaClass) {
            this.javaClass = javaClass;
            return this;
        }

        public Builder qualifiedClass(String className) {
            this.qualifiedClass = className;
            return this;
        }

        public Builder sourcePosition(String sourcePosition) {
            this.sourcePosition = sourcePosition;
            return this;
        }

        public Change build() {
            return new Change(id, name, disabled, loggingOnly, enabledAfter, description,
                    javaPackage, javaClass, qualifiedClass, sourcePosition);
        }
    }
}
