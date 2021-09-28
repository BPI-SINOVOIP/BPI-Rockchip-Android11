/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.tuner.hdhomerun;

import android.content.Context;
import android.media.tv.TvContract;
import android.os.ConditionVariable;
import android.util.Log;
import android.util.Xml;
import com.android.tv.tuner.api.ChannelScanListener;
import com.android.tv.tuner.data.TunerChannel;
import com.android.tv.tuner.ts.EventDetector.EventListener;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.regex.Pattern;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

/** A helper class to perform channel scan on HDHomeRun tuner. */
public class HdHomeRunChannelScan {
    private static final String TAG = "HdHomeRunChannelScan";
    private static final boolean DEBUG = false;

    private static final String LINEUP_FILENAME = "lineup.xml";
    private static final String NAME_LINEUP = "Lineup";
    private static final String NAME_PROGRAM = "Program";
    private static final String NAME_GUIDE_NUMBER = "GuideNumber";
    private static final String NAME_GUIDE_NAME = "GuideName";
    private static final String NAME_HD = "HD";
    private static final String NAME_TAGS = "Tags";
    private static final String NAME_DRM = "DRM";

    private final Context mContext;
    private final ChannelScanListener mEventListener;
    private final HdHomeRunTunerHal mTunerHal;
    private int mProgramCount;

    public HdHomeRunChannelScan(
            Context context, EventListener eventListener, HdHomeRunTunerHal hal) {
        mContext = context;
        mEventListener = eventListener;
        mTunerHal = hal;
    }

    public void scan(ConditionVariable conditionStopped) {
        String urlString = "http://" + mTunerHal.getIpAddress() + "/" + LINEUP_FILENAME;
        if (DEBUG) Log.d(TAG, "Reading " + urlString);
        URL url;
        HttpURLConnection connection = null;
        InputStream inputStream;
        try {
            url = new URL(urlString);
            connection = (HttpURLConnection) url.openConnection();
            connection.setReadTimeout(HdHomeRunTunerHal.READ_TIMEOUT_MS_FOR_URLCONNECTION);
            connection.setConnectTimeout(HdHomeRunTunerHal.CONNECTION_TIMEOUT_MS_FOR_URLCONNECTION);
            connection.setRequestMethod("GET");
            connection.setDoInput(true);
            connection.connect();
            inputStream = connection.getInputStream();
        } catch (IOException e) {
            Log.e(TAG, "Connection failed: " + urlString, e);
            if (connection != null) {
                connection.disconnect();
            }
            return;
        }
        if (conditionStopped.block(-1)) {
            try {
                inputStream.close();
            } catch (IOException e) {
                // Does nothing.
            }
            connection.disconnect();
            return;
        }

        XmlPullParser parser = Xml.newPullParser();
        try {
            parser.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, false);
            parser.setInput(inputStream, null);
            parser.nextTag();
            parser.require(XmlPullParser.START_TAG, null, NAME_LINEUP);
            while (parser.next() != XmlPullParser.END_TAG) {
                if (conditionStopped.block(-1)) {
                    break;
                }
                if (parser.getEventType() != XmlPullParser.START_TAG) {
                    continue;
                }
                String name = parser.getName();
                // Starts by looking for the program tag
                if (name.equals(NAME_PROGRAM)) {
                    readProgram(parser);
                } else {
                    skip(parser);
                }
            }
            inputStream.close();
        } catch (IOException | XmlPullParserException e) {
            Log.e(TAG, "Parse error", e);
        }
        connection.disconnect();
        mTunerHal.markAsScannedDevice(mContext);
    }

    private void readProgram(XmlPullParser parser) throws XmlPullParserException, IOException {
        parser.require(XmlPullParser.START_TAG, null, NAME_PROGRAM);
        String guideNumber = "";
        String guideName = "";
        String videoFormat = null;
        String tags = "";
        boolean recordingProhibited = false;
        while (parser.next() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            String name = parser.getName();
            if (name.equals(NAME_GUIDE_NUMBER)) {
                guideNumber = readText(parser, NAME_GUIDE_NUMBER);
            } else if (name.equals(NAME_GUIDE_NAME)) {
                guideName = readText(parser, NAME_GUIDE_NAME);
            } else if (name.equals(NAME_HD)) {
                videoFormat = TvContract.Channels.VIDEO_FORMAT_720P;
                skip(parser);
            } else if (name.equals(NAME_TAGS)) {
                tags = readText(parser, NAME_TAGS);
            } else if (name.equals(NAME_DRM)) {
                String drm = readText(parser, NAME_DRM);
                try {
                    recordingProhibited = (Integer.parseInt(drm)) != 0;
                } catch (NumberFormatException e) {
                    Log.e(TAG, "Load DRM property failed: illegal number: " + drm);
                    // If DRM property is present, we treat it as copy-once or copy-never.
                    recordingProhibited = true;
                }
            } else {
                skip(parser);
            }
        }
        if (!tags.isEmpty()) {
            // Skip encrypted channels since we don't know how to decrypt them.
            return;
        }
        int major;
        int minor = 0;
        final String separator = Character.toString(HdHomeRunTunerHal.VCHANNEL_SEPARATOR);
        if (guideNumber.contains(separator)) {
            String[] parts = guideNumber.split(Pattern.quote(separator));
            major = Integer.parseInt(parts[0]);
            minor = Integer.parseInt(parts[1]);
        } else {
            major = Integer.parseInt(guideNumber);
        }
        // Need to assign a unique program number (i.e. mProgramCount) to avoid being duplicated.
        mEventListener.onChannelDetected(
                TunerChannel.forNetwork(
                        major, minor, mProgramCount++, guideName, recordingProhibited, videoFormat),
                true);
    }

    private String readText(XmlPullParser parser, String name)
            throws IOException, XmlPullParserException {
        String result = "";
        parser.require(XmlPullParser.START_TAG, null, name);
        if (parser.next() == XmlPullParser.TEXT) {
            result = parser.getText();
            parser.nextTag();
        }
        parser.require(XmlPullParser.END_TAG, null, name);
        if (DEBUG) Log.d(TAG, "<" + name + ">=" + result);
        return result;
    }

    private void skip(XmlPullParser parser) throws XmlPullParserException, IOException {
        if (parser.getEventType() != XmlPullParser.START_TAG) {
            throw new IllegalStateException();
        }
        int depth = 1;
        while (depth != 0) {
            switch (parser.next()) {
                case XmlPullParser.END_TAG:
                    depth--;
                    break;
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
            }
        }
    }
}
