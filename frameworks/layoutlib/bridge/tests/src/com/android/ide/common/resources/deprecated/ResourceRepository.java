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
import com.android.ide.common.rendering.api.ResourceValue;
import com.android.ide.common.resources.ResourceValueMap;
import com.android.ide.common.resources.configuration.FolderConfiguration;
import com.android.io.IAbstractFile;
import com.android.io.IAbstractFolder;
import com.android.io.IAbstractResource;
import com.android.resources.ResourceFolderType;
import com.android.resources.ResourceType;
import com.android.tools.layoutlib.annotations.NotNull;
import com.android.tools.layoutlib.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public abstract class ResourceRepository {
    private final IAbstractFolder mResourceFolder;

    private Map<ResourceFolderType, List<ResourceFolder>> mFolderMap =
            new EnumMap<>(ResourceFolderType.class);

    protected Map<ResourceType, Map<String, ResourceItem>> mResourceMap =
            new EnumMap<>(ResourceType.class);

    private final boolean mFrameworkRepository;
    private boolean mCleared = true;
    private boolean mInitializing;

    /**
     * Makes a resource repository.
     *
     * @param resFolder the resource folder of the repository.
     * @param isFrameworkRepository whether the repository is for framework resources.
     */
    protected ResourceRepository(@NotNull IAbstractFolder resFolder,
            boolean isFrameworkRepository) {
        mResourceFolder = resFolder;
        mFrameworkRepository = isFrameworkRepository;
    }

    public IAbstractFolder getResFolder() {
        return mResourceFolder;
    }

    public boolean isFrameworkRepository() {
        return mFrameworkRepository;
    }

    private synchronized void clear() {
        mCleared = true;
        mFolderMap = new EnumMap<>(ResourceFolderType.class);
        mResourceMap = new EnumMap<>(ResourceType.class);
    }

    /**
     * Ensures that the repository has been initialized again after a call to
     * {@link com.android.ide.common.resources.deprecated.ResourceRepository#clear()}.
     *
     * @return true if the repository was just re-initialized.
     */
    private synchronized boolean ensureInitialized() {
        if (mCleared && !mInitializing) {
            ScanningContext context = new ScanningContext();
            mInitializing = true;

            IAbstractResource[] resources = mResourceFolder.listMembers();

            for (IAbstractResource res : resources) {
                if (res instanceof IAbstractFolder) {
                    IAbstractFolder folder = (IAbstractFolder)res;
                    ResourceFolder resFolder = processFolder(folder);

                    if (resFolder != null) {
                        // now we process the content of the folder
                        IAbstractResource[] files = folder.listMembers();

                        for (IAbstractResource fileRes : files) {
                            if (fileRes instanceof IAbstractFile) {
                                IAbstractFile file = (IAbstractFile)fileRes;

                                resFolder.processFile(file, ResourceDeltaKind.ADDED, context);
                            }
                        }
                    }
                }
            }

            mInitializing = false;
            mCleared = false;
            return true;
        }

        return false;
    }

    /**
     * Adds a Folder Configuration to the project.
     *
     * @param type The resource type.
     * @param config The resource configuration.
     * @param folder The workspace folder object.
     * @return the {@link ResourceFolder} object associated to this folder.
     */
    private ResourceFolder add(
            @NotNull ResourceFolderType type,
            @NotNull FolderConfiguration config,
            @NotNull IAbstractFolder folder) {
        // get the list for the resource type
        List<ResourceFolder> list = mFolderMap.get(type);

        if (list == null) {
            list = new ArrayList<>();

            ResourceFolder cf = new ResourceFolder(type, config, folder, this);
            list.add(cf);

            mFolderMap.put(type, list);

            return cf;
        }

        // look for an already existing folder configuration.
        for (ResourceFolder cFolder : list) {
            if (cFolder.mConfiguration.equals(config)) {
                // config already exist. Nothing to be done really, besides making sure
                // the IAbstractFolder object is up to date.
                cFolder.mFolder = folder;
                return cFolder;
            }
        }

        // If we arrive here, this means we didn't find a matching configuration.
        // So we add one.
        ResourceFolder cf = new ResourceFolder(type, config, folder, this);
        list.add(cf);

        return cf;
    }

    /**
     * Returns a {@link ResourceItem} matching the given {@link ResourceType} and name. If none
     * exist, it creates one.
     *
     * @param type the resource type
     * @param name the name of the resource.
     * @return A resource item matching the type and name.
     */
    @NotNull
    public ResourceItem getResourceItem(@NotNull ResourceType type, @NotNull String name) {
        ensureInitialized();

        // looking for an existing ResourceItem with this type and name
        ResourceItem item = findDeclaredResourceItem(type, name);

        // create one if there isn't one already, or if the existing one is inlined, since
        // clearly we need a non inlined one (the inline one is removed too)
        if (item == null) {
            item = createResourceItem(name);

            Map<String, ResourceItem> map = mResourceMap.get(type);

            if (map == null) {
                if (isFrameworkRepository()) {
                    // Pick initial size for the maps. Also change the load factor to 1.0
                    // to avoid rehashing the whole table when we (as expected) get near
                    // the known rough size of each resource type map.
                    int size;
                    switch (type) {
                        // Based on counts in API 16. Going back to API 10, the counts
                        // are roughly 25-50% smaller (e.g. compared to the top 5 types below
                        // the fractions are 1107 vs 1734, 831 vs 1508, 895 vs 1255,
                        // 733 vs 1064 and 171 vs 783.
                        case PUBLIC:           size = 1734; break;
                        case DRAWABLE:         size = 1508; break;
                        case STRING:           size = 1255; break;
                        case ATTR:             size = 1064; break;
                        case STYLE:             size = 783; break;
                        case ID:                size = 347; break;
                        case STYLEABLE:
                            size = 210;
                            break;
                        case LAYOUT:            size = 187; break;
                        case COLOR:             size = 120; break;
                        case ANIM:               size = 95; break;
                        case DIMEN:              size = 81; break;
                        case BOOL:               size = 54; break;
                        case INTEGER:            size = 52; break;
                        case ARRAY:              size = 51; break;
                        case PLURALS:            size = 20; break;
                        case XML:                size = 14; break;
                        case INTERPOLATOR :      size = 13; break;
                        case ANIMATOR:            size = 8; break;
                        case RAW:                 size = 4; break;
                        case MENU:                size = 2; break;
                        case MIPMAP:              size = 2; break;
                        case FRACTION:            size = 1; break;
                        default:
                            size = 2;
                    }
                    map = new HashMap<>(size, 1.0f);
                } else {
                    map = new HashMap<>();
                }
                mResourceMap.put(type, map);
            }

            map.put(item.getName(), item);
        }

        return item;
    }

    /**
     * Creates a resource item with the given name.
     * @param name the name of the resource
     * @return a new ResourceItem (or child class) instance.
     */
    @NotNull
    protected abstract ResourceItem createResourceItem(@NotNull String name);

    /**
     * Processes a folder and adds it to the list of existing folders.
     * @param folder the folder to process
     * @return the ResourceFolder created from this folder, or null if the process failed.
     */
    @Nullable
    private ResourceFolder processFolder(@NotNull IAbstractFolder folder) {
        ensureInitialized();

        // split the name of the folder in segments.
        String[] folderSegments = folder.getName().split(SdkConstants.RES_QUALIFIER_SEP);

        // get the enum for the resource type.
        ResourceFolderType type = ResourceFolderType.getTypeByName(folderSegments[0]);

        if (type != null) {
            // get the folder configuration.
            FolderConfiguration config = FolderConfiguration.getConfig(folderSegments);

            if (config != null) {
                return add(type, config, folder);
            }
        }

        return null;
    }

    /**
     * Returns the resources values matching a given {@link FolderConfiguration}.
     *
     * @param referenceConfig the configuration that each value must match.
     * @return a map with guaranteed to contain an entry for each {@link ResourceType}
     */
    @NotNull
    public Map<ResourceType, ResourceValueMap> getConfiguredResources(
            @NotNull FolderConfiguration referenceConfig) {
        ensureInitialized();

        return doGetConfiguredResources(referenceConfig);
    }

    /**
     * Returns the resources values matching a given {@link FolderConfiguration} for the current
     * project.
     *
     * @param referenceConfig the configuration that each value must match.
     * @return a map with guaranteed to contain an entry for each {@link ResourceType}
     */
    @NotNull
    private Map<ResourceType, ResourceValueMap> doGetConfiguredResources(
            @NotNull FolderConfiguration referenceConfig) {
        ensureInitialized();

        Map<ResourceType, ResourceValueMap> map =
            new EnumMap<>(ResourceType.class);

        for (ResourceType key : ResourceType.values()) {
            // get the local results and put them in the map
            map.put(key, getConfiguredResource(key, referenceConfig));
        }

        return map;
    }

   /**
     * Loads the resources.
     */
    public void loadResources() {
        clear();
        ensureInitialized();
    }

    protected void removeFile(@NotNull Collection<ResourceType> types,
            @NotNull ResourceFile file) {
        ensureInitialized();

        for (ResourceType type : types) {
            removeFile(type, file);
        }
    }

    private void removeFile(@NotNull ResourceType type, @NotNull ResourceFile file) {
        Map<String, ResourceItem> map = mResourceMap.get(type);
        if (map != null) {
            Collection<ResourceItem> values = map.values();
            List<ResourceItem> toDelete = null;
            for (ResourceItem item : values) {
                item.removeFile(file);
                if (item.hasNoSourceFile()) {
                    if (toDelete == null) {
                        toDelete = new ArrayList<>(values.size());
                    }
                    toDelete.add(item);
                }
            }
            if (toDelete != null) {
                for (ResourceItem item : toDelete) {
                    map.remove(item.getName());
                }
            }
        }
    }

    /**
     * Returns a map of (resource name, resource value) for the given {@link ResourceType}.
     * <p>The values returned are taken from the resource files best matching a given
     * {@link FolderConfiguration}.
     *
     * @param type the type of the resources.
     * @param referenceConfig the configuration to best match.
     */
    @NotNull
    private ResourceValueMap getConfiguredResource(@NotNull ResourceType type,
            @NotNull FolderConfiguration referenceConfig) {
        // get the resource item for the given type
        Map<String, ResourceItem> items = mResourceMap.get(type);
        if (items == null) {
            return ResourceValueMap.create();
        }

        // create the map
        ResourceValueMap map = ResourceValueMap.createWithExpectedSize(items.size());

        for (ResourceItem item : items.values()) {
            ResourceValue value = item.getResourceValue(type, referenceConfig);
            if (value != null) {
                map.put(item.getName(), value);
            }
        }

        return map;
    }

    /**
     * Looks up an existing {@link ResourceItem} by {@link ResourceType} and name.
     *
     * @param type the resource type.
     * @param name the resource name.
     * @return the existing ResourceItem or null if no match was found.
     */
    @Nullable
    private ResourceItem findDeclaredResourceItem(
            @NotNull ResourceType type, @NotNull String name) {
        Map<String, ResourceItem> map = mResourceMap.get(type);
        return map != null ? map.get(name) : null;
    }
}

