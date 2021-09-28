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

import com.android.SdkConstants;
import com.android.ide.common.resources.configuration.Configurable;
import com.android.ide.common.resources.configuration.FolderConfiguration;
import com.android.io.IAbstractFile;
import com.android.io.IAbstractFolder;
import com.android.resources.FolderTypeRelationship;
import com.android.resources.ResourceFolderType;
import com.android.resources.ResourceType;
import com.android.utils.SdkUtils;

import java.util.List;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public final class ResourceFolder implements Configurable {
    private final ResourceFolderType mType;
    final FolderConfiguration mConfiguration;
    IAbstractFolder mFolder;
    private final ResourceRepository mRepository;

    /**
     * Creates a new {@link com.android.ide.common.resources.deprecated.ResourceFolder}
     * @param type The type of the folder
     * @param config The configuration of the folder
     * @param folder The associated {@link IAbstractFolder} object.
     * @param repository The associated {@link ResourceRepository}
     */
    protected ResourceFolder(ResourceFolderType type, FolderConfiguration config,
            IAbstractFolder folder, ResourceRepository repository) {
        mType = type;
        mConfiguration = config;
        mFolder = folder;
        mRepository = repository;
    }

    /**
     * Processes a file and adds it to its parent folder resource.
     *
     * @param file the underlying resource file.
     * @param kind the file change kind.
     * @param context a context object with state for the current update, such
     *            as a place to stash errors encountered
     * @return the {@link ResourceFile} that was created.
     */
    public ResourceFile processFile(IAbstractFile file, ResourceDeltaKind kind,
            ScanningContext context) {
        // look for this file if it's already been created
        ResourceFile resFile = getFile(file, context);

        if (resFile == null) {
            if (kind != ResourceDeltaKind.REMOVED) {
                // create a ResourceFile for it.

                resFile = createResourceFile(file);
                resFile.load(context);
            }
        } else {
            if (kind != ResourceDeltaKind.REMOVED) {
                resFile.update(context);
            }
        }

        return resFile;
    }

    private ResourceFile createResourceFile(IAbstractFile file) {
        // check if that's a single or multi resource type folder. We have a special case
        // for ID generating resource types (layout/menu, and XML drawables, etc.).
        // MultiResourceFile handles the case when several resource types come from a single file
        // (values files).

        ResourceFile resFile;
        if (mType != ResourceFolderType.VALUES) {
            if (FolderTypeRelationship.isIdGeneratingFolderType(mType) &&
                SdkUtils.endsWithIgnoreCase(file.getName(), SdkConstants.DOT_XML)) {
                List<ResourceType> types = FolderTypeRelationship.getRelatedResourceTypes(mType);
                ResourceType primaryType = types.get(0);
                resFile = new IdGeneratingResourceFile(file, this, primaryType);
            } else {
                resFile = new SingleResourceFile(file, this);
            }
        } else {
            resFile = new MultiResourceFile(file, this);
        }
        return resFile;
    }

    /**
     * Returns the {@link ResourceFolderType} of this object.
     */
    public ResourceFolderType getType() {
        return mType;
    }

    public ResourceRepository getRepository() {
        return mRepository;
    }

    @Override
    public FolderConfiguration getConfiguration() {
        return mConfiguration;
    }

    /**
     * Returns the {@link ResourceFile} matching a {@link IAbstractFile} object.
     *
     * @param file The {@link IAbstractFile} object.
     * @param context a context object with state for the current update, such
     *            as a place to stash errors encountered
     * @return the {@link ResourceFile} or null if no match was found.
     */
    private ResourceFile getFile(IAbstractFile file, ScanningContext context) {
        assert mFolder.equals(file.getParentFolder());

        // If the file actually exists, the resource folder  may not have been
        // scanned yet; add it lazily
        if (file.exists()) {
            ResourceFile resFile = createResourceFile(file);
            resFile.load(context);
            return resFile;
        }

        return null;
    }

    @Override
    public String toString() {
        return mFolder.toString();
    }
}
