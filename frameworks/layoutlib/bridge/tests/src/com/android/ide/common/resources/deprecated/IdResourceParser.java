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
import com.android.ide.common.rendering.api.ResourceNamespace;
import com.android.ide.common.rendering.api.ResourceReference;
import com.android.ide.common.rendering.api.ResourceValue;
import com.android.ide.common.rendering.api.ResourceValueImpl;
import com.android.ide.common.resources.deprecated.ValueResourceParser.IValueResourceRepository;
import com.android.resources.ResourceType;
import com.android.tools.layoutlib.annotations.NotNull;

import org.kxml2.io.KXmlParser;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

import com.google.common.io.Closeables;

/**
 * @deprecated This class is part of an obsolete resource repository system that is no longer used
 *     in production code. The class is preserved temporarily for LayoutLib tests.
 */
@Deprecated
public class IdResourceParser {
    private final IValueResourceRepository mRepository;
    private final boolean mIsFramework;

    /**
     * Creates a new {@link com.android.ide.common.resources.deprecated.IdResourceParser}
     *
     * @param repository value repository for registering resource declaration
     * @param isFramework true if scanning a framework resource
     */
    public IdResourceParser(
            @NotNull ValueResourceParser.IValueResourceRepository repository,
            boolean isFramework) {
        mRepository = repository;
        mIsFramework = isFramework;
    }

    /**
     * Parse the given input and register ids with the given
     * {@link IValueResourceRepository}.
     *
     * @param input the input stream of the XML to be parsed (will be closed by this method)
     * @return true if parsing succeeds and false if it fails
     * @throws IOException if reading the contents fails
     */
    public boolean parse(InputStream input)
            throws IOException {
        KXmlParser parser = new KXmlParser();
        try {
            parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, true);

            if (input instanceof FileInputStream) {
                input = new BufferedInputStream(input);
            }
            parser.setInput(input, SdkConstants.UTF_8);

            return parse(parser);
        } catch (XmlPullParserException | RuntimeException e) {
            return false;
        } finally {
            try {
                Closeables.close(input, true /* swallowIOException */);
            } catch (IOException e) {
                // cannot happen
            }
        }
    }

    private boolean parse(KXmlParser parser)
            throws XmlPullParserException, IOException {
        while (true) {
            int event = parser.next();
            if (event == XmlPullParser.START_TAG) {
                for (int i = 0, n = parser.getAttributeCount(); i < n; i++) {
                    String attribute = parser.getAttributeName(i);
                    String value = parser.getAttributeValue(i);
                    assert value != null : attribute;

                    if (value.startsWith("@+")) {       //$NON-NLS-1$
                        // Strip out the @+id/ or @+android:id/ section
                        String id = value.substring(value.indexOf('/') + 1);
                        ResourceValue newId =
                                new ResourceValueImpl(
                                        new ResourceReference(
                                                ResourceNamespace.fromBoolean(mIsFramework),
                                                ResourceType.ID,
                                                id),
                                        null);
                        mRepository.addResourceValue(newId);
                    }
                }
            } else if (event == XmlPullParser.END_DOCUMENT) {
                break;
            }
        }

        return true;
    }
}
