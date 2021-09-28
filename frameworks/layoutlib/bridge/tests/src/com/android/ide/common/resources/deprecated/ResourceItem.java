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

package com.android.ide.common.resources.deprecated;

import com.android.ide.common.rendering.api.ResourceValue;
import com.android.ide.common.resources.configuration.FolderConfiguration;
import com.android.resources.ResourceType;

import java.util.ArrayList;
import java.util.List;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public class ResourceItem implements Comparable<ResourceItem> {
    private final String mName;

    /**
     * List of files generating this ResourceItem.
     */
    private final List<ResourceFile> mFiles = new ArrayList<>();

    /**
     * Constructs a new ResourceItem.
     * @param name the name of the resource as it appears in the XML and R.java files.
     */
    public ResourceItem(String name) {
        mName = name;
    }

    /**
     * Returns the name of the resource.
     */
    public final String getName() {
        return mName;
    }

    /**
     * Compares the {@link com.android.ide.common.resources.deprecated.ResourceItem} to another.
     * @param other the ResourceItem to be compared to.
     */
    @Override
    public int compareTo(com.android.ide.common.resources.deprecated.ResourceItem other) {
        return mName.compareTo(other.mName);
    }

    /**
     * Returns a {@link ResourceValue} for this item based on the given configuration.
     * If the ResourceItem has several source files, one will be selected based on the config.
     * @param type the type of the resource. This is necessary because ResourceItem doesn't embed
     *     its type, but ResourceValue does.
     * @param referenceConfig the config of the resource item.
     * @return a ResourceValue or null if none match the config.
     */
    public ResourceValue getResourceValue(ResourceType type, FolderConfiguration referenceConfig) {
        // look for the best match for the given configuration
        // the match has to be of type ResourceFile since that's what the input list contains
        ResourceFile match = referenceConfig.findMatchingConfigurable(mFiles);

        if (match != null) {
            // get the value of this configured resource.
            return match.getValue(type, mName);
        }

        return null;
    }

    /**
     * Adds a new source file.
     * @param file the source file.
     */
    protected void add(ResourceFile file) {
        mFiles.add(file);
    }

    /**
     * Removes a file from the list of source files.
     * @param file the file to remove
     */
    protected void removeFile(ResourceFile file) {
        mFiles.remove(file);
    }

    /**
     * Returns {@code true} if the item has no source file.
     * @return true if the item has no source file.
     */
    protected boolean hasNoSourceFile() {
        return mFiles.isEmpty();
    }
}
