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
package com.android.car.uxr;

import android.content.Context;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Xml;
import android.view.View;

import androidx.annotation.XmlRes;

import com.android.car.uxr.CarUxRestrictionsAppConfig.ListConfig;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * A parser that can read an XML resource file and construct the corresponding
 * {@link CarUxRestrictionsAppConfig} object.
 *
 * See car-uxr-client-lib/res/values/attrs.xml for the definition of the relevant XML tags.
 */
public class CarUxRestrictionsAppConfigParser {
    private static final String TAG = "UxrAppConfigParser";

    static CarUxRestrictionsAppConfig parseConfig(Context context, @XmlRes int xmlRes) {
        try (XmlResourceParser parser = context.getResources().getXml(xmlRes)) {
            AttributeSet attrs = Xml.asAttributeSet(parser);
            Map<Integer, ListConfig> mapping = new HashMap<>();

            // Skip over the xml version tag
            parser.next();
            // Skip over the copyright comment block
            parser.next();
            parser.require(XmlPullParser.START_TAG, null, "Mapping");
            while (parser.next() != XmlPullParser.END_TAG) {
                ListConfig listConfig = parseListConfigItem(context, parser, attrs);
                mapping.put(listConfig.getId(), listConfig);
            }

            return new CarUxRestrictionsAppConfig(mapping);
        } catch (XmlPullParserException | IOException e) {
            throw new RuntimeException("Unable to parse CarUxRestrictionsAppConfig", e);
        }
    }

    private static ListConfig parseListConfigItem(
            Context context, XmlResourceParser parser, AttributeSet attrs)
            throws XmlPullParserException, IOException {

        parser.require(XmlPullParser.START_TAG, null, "ListConfig");

        TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.CarUxRestrictionsAppConfig_ListConfig);

        try {
            int id = a.getResourceId(R.styleable.CarUxRestrictionsAppConfig_ListConfig_id,
                    View.NO_ID);
            if (id == View.NO_ID) {
                throw new IllegalStateException("Id field is required");
            }

            boolean messageExists = a.hasValue(
                    R.styleable.CarUxRestrictionsAppConfig_ListConfig_message);
            int messageResId = a.getResourceId(
                    R.styleable.CarUxRestrictionsAppConfig_ListConfig_message, View.NO_ID);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, messageExists
                        ? "message field is set to " + messageResId
                        : "message field not specified");
            }

            boolean maxLengthExists = a.hasValue(
                    R.styleable.CarUxRestrictionsAppConfig_ListConfig_maxLength);
            int maxLengthInt = a.getInt(
                    R.styleable.CarUxRestrictionsAppConfig_ListConfig_maxLength, 0);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, maxLengthExists
                        ? "maxLength field is set to " + maxLengthInt
                        : "maxLength field not specified");
            }

            parser.next();
            parser.require(XmlPullParser.END_TAG, null, "ListConfig");

            ListConfig.Builder builder = ListConfig.builder(id);
            if (maxLengthExists) {
                builder.setContentLimit(maxLengthInt);
            }
            if (messageExists) {
                builder.setScrollingLimitedMessageResId(messageResId);
            }
            return builder.build();
        } finally {
            a.recycle();
        }
    }
}
