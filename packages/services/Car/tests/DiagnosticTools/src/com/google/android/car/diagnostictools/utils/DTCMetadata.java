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

package com.google.android.car.diagnostictools.utils;

import android.text.Html;
import android.text.Spanned;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

/** Holds metadata for a DTC. The intended use case is to map this to a DTC code. */
public class DTCMetadata {

    private static final String TAG = "DTCMetadata";
    private static final String SHORT_DESCRIPTION_KEY = "short_description";
    private static final String LONG_DESCRIPTION_KEY = "long_description";

    private String mShortDescription;
    private String mStringLongDescription;
    private Spanned mSpannedLongDescription;

    /**
     * Creates new DTCMeta data object. Both the String and Spanned version of the longDescription
     * are stored to allow for the object to be parcelable (String is used) and allow only one call
     * to Html.fromHTML (Spanned is used)
     *
     * @param shortDescription Main description of 50 characters or less
     * @param longDescription Longer description that can be in HTML format to allow for more
     *     complex formatting.
     */
    private DTCMetadata(String shortDescription, String longDescription) {
        this.mShortDescription = shortDescription;
        this.mStringLongDescription = longDescription;
        this.mSpannedLongDescription = Html.fromHtml(longDescription, 0);
    }

    /**
     * Build a DTCmeta object from a JSON object. Used by JsonMetadataReader
     *
     * @param jsonObject JSONObject that contains both a "short_description" key and a
     *     "long_description" key
     * @return New DTCMetadata object if successful or null if not
     */
    static DTCMetadata buildFromJson(JSONObject jsonObject) {
        try {
            return new DTCMetadata(
                    jsonObject.getString(SHORT_DESCRIPTION_KEY),
                    jsonObject.getString(LONG_DESCRIPTION_KEY));
        } catch (JSONException e) {
            Log.d(
                    TAG,
                    "DTC JSON Object doesn't match expected format: "
                            + jsonObject.toString()
                            + " Error: "
                            + e.toString());
            return null;
        }
    }

    public String getShortDescription() {
        return mShortDescription;
    }

    public Spanned getSpannedLongDescription() {
        return mSpannedLongDescription;
    }

    public String getStringLongDescription() {
        return mStringLongDescription;
    }
}
