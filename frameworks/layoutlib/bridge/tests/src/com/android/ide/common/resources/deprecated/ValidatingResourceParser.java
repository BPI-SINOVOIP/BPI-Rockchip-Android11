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
public class ValidatingResourceParser {
    private final boolean mIsFramework;
    private final ScanningContext mContext;

    /**
     * Creates a new {@link com.android.ide.common.resources.deprecated.ValidatingResourceParser}
     *
     * @param context a context object with state for the current update, such
     *            as a place to stash errors encountered
     * @param isFramework true if scanning a framework resource
     */
    public ValidatingResourceParser(
            @NotNull ScanningContext context,
            boolean isFramework) {
        mContext = context;
        mIsFramework = isFramework;
    }

    /**
     * Parse the given input and return false if it contains errors, <b>or</b> if
     * the context is already tagged as needing a full aapt run.
     *
     * @param input the input stream of the XML to be parsed (will be closed by this method)
     * @return true if parsing succeeds and false if it fails
     * @throws IOException if reading the contents fails
     */
    public boolean parse(InputStream input)
            throws IOException {
        // No need to validate framework files
        if (mIsFramework) {
            try {
                Closeables.close(input, true /* swallowIOException */);
            } catch (IOException e) {
                // cannot happen
            }
            return true;
        }
        if (mContext.needsFullAapt()) {
            try {
                Closeables.close(input, true /* swallowIOException */);
            } catch (IOException ignore) {
                // cannot happen
            }
            return false;
        }

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
                }
            } else if (event == XmlPullParser.END_DOCUMENT) {
                break;
            }
        }

        return true;
    }
}
