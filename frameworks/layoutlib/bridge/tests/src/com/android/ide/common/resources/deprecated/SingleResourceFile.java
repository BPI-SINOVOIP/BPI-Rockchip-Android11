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

import com.android.ide.common.rendering.api.DensityBasedResourceValueImpl;
import com.android.ide.common.rendering.api.ResourceNamespace;
import com.android.ide.common.rendering.api.ResourceReference;
import com.android.ide.common.rendering.api.ResourceValue;
import com.android.ide.common.rendering.api.ResourceValueImpl;
import com.android.ide.common.resources.configuration.DensityQualifier;
import com.android.ide.common.resources.configuration.ResourceQualifier;
import com.android.io.IAbstractFile;
import com.android.resources.FolderTypeRelationship;
import com.android.resources.ResourceType;
import com.android.utils.SdkUtils;

import java.util.List;


import static com.android.SdkConstants.DOT_XML;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public class SingleResourceFile extends ResourceFile {
    private final String mResourceName;
    private final ResourceType mType;
    private final ResourceValue mValue;

    public SingleResourceFile(IAbstractFile file, ResourceFolder folder) {
        super(file, folder);

        // we need to infer the type of the resource from the folder type.
        // This is easy since this is a single Resource file.
        List<ResourceType> types = FolderTypeRelationship.getRelatedResourceTypes(folder.getType());
        mType = types.get(0);

        // compute the resource name
        mResourceName = getResourceName();

        // test if there's a density qualifier associated with the resource
        DensityQualifier qualifier = folder.getConfiguration().getDensityQualifier();

        if (!ResourceQualifier.isValid(qualifier)) {
            mValue =
                    new ResourceValueImpl(
                            new ResourceReference(
                                    ResourceNamespace.fromBoolean(isFramework()),
                                    mType,
                                    getResourceName()),
                            file.getOsLocation());
        } else {
            mValue =
                    new DensityBasedResourceValueImpl(
                            new ResourceReference(
                                    ResourceNamespace.fromBoolean(isFramework()),
                                    mType,
                                    getResourceName()),
                            file.getOsLocation(),
                            qualifier.getValue());
        }
    }

    @Override
    protected void load(ScanningContext context) {
        // get a resource item matching the given type and name
        ResourceItem item = getRepository().getResourceItem(mType, mResourceName);

        // add this file to the list of files generating this resource item.
        item.add(this);

        // Ask for an ID refresh since we're adding an item that will generate an ID
        context.requestFullAapt();
    }

    @Override
    protected void update(ScanningContext context) {
        // when this happens, nothing needs to be done since the file only generates
        // a single resources that doesn't actually change (its content is the file path)

        // However, we should check for newly introduced errors
        // Parse the file and look for @+id/ entries
        validateAttributes(context);
    }

    /*
     * (non-Javadoc)
     * @see com.android.ide.eclipse.editors.resources.manager.ResourceFile#getValue(com.android.ide.eclipse.common.resources.ResourceType, java.lang.String)
     *
     * This particular implementation does not care about the type or name since a
     * SingleResourceFile represents a file generating only one resource.
     * The value returned is the full absolute path of the file in OS form.
     */
    @Override
    public ResourceValue getValue(ResourceType type, String name) {
        return mValue;
    }

    /**
     * Returns the name of the resources.
     */
    private String getResourceName() {
        // get the name from the filename.
        String name = getFile().getName();

        int pos = name.indexOf('.');
        if (pos != -1) {
            name = name.substring(0, pos);
        }

        return name;
    }

    /**
     * Validates the associated resource file to make sure the attribute references are valid
     *
     * @return true if parsing succeeds and false if it fails
     */
    private boolean validateAttributes(ScanningContext context) {
        // We only need to check if it's a non-framework file (and an XML file; skip .png's)
        if (!isFramework() && SdkUtils.endsWith(getFile().getName(), DOT_XML)) {
            ValidatingResourceParser parser = new ValidatingResourceParser(context, false);
            try {
                IAbstractFile file = getFile();
                return parser.parse(file.getContents());
            } catch (Exception e) {
                context.needsFullAapt();
            }

            return false;
        }

        return true;
    }
}
