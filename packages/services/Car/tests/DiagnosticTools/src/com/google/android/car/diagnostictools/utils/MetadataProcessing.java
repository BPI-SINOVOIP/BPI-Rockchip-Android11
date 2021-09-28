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

import android.content.Context;
import android.util.Log;

import com.google.android.car.diagnostictools.R;

import org.json.JSONException;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;


/** Singleton class that contains all available metadata information */
public class MetadataProcessing {

    private static final String TAG = "MetadataProcessing";
    private static MetadataProcessing sInstance = null;

    private Map<Integer, SensorMetadata> mFloatMetadata = new HashMap<>();
    private Map<Integer, SensorMetadata> mIntegerMetadata = new HashMap<>();
    private Map<String, DTCMetadata> mDTCMetadataMap = new HashMap<>();
    private Map<String, String> mECUMetadataMap = new HashMap<>();

    /**
     * Creates a MetadataProcessing object using a context to access files in res/raw
     *
     * @param context Any context object. Used to access files in res/raw
     */
    private MetadataProcessing(Context context) {
        // DTC Metadata
        try {
            mDTCMetadataMap =
                    JsonMetadataReader.readDTCsFromJson(
                            context.getResources().openRawResource(R.raw.system_dtcs));
        } catch (IOException | JSONException e) {
            Log.d(
                    TAG,
                    "Error reading in JSON Metadata for system DTCs. More info: " + e.toString());
        }
        try {
            mDTCMetadataMap.putAll(
                    JsonMetadataReader.readDTCsFromJson(
                            context.getResources().openRawResource(R.raw.vendor_dtcs)));
        } catch (IOException | JSONException e) {
            Log.d(
                    TAG,
                    "Error reading in JSON Metadata for vendor DTCs. More info: " + e.toString());
        }

        // Float Metadata
        try {
            mFloatMetadata =
                    JsonMetadataReader.readFloatSensorIndexFromJson(
                            context.getResources().openRawResource(R.raw.system_float_sensors));
        } catch (IOException | JSONException e) {
            Log.e(
                    TAG,
                    "Error reading in JSON Metadata for system float sensors. More info: "
                            + e.toString());
        }
        try {
            mFloatMetadata.putAll(
                    JsonMetadataReader.readFloatSensorIndexFromJson(
                            context.getResources().openRawResource(R.raw.vendor_float_sensors)));
        } catch (IOException | JSONException e) {
            Log.e(
                    TAG,
                    "Error reading in JSON Metadata for vendor float sensors. More info: "
                            + e.toString());
        }

        // Integer Metadata
        try {
            mIntegerMetadata =
                    JsonMetadataReader.readIntegerSensorIndexFromJson(
                            context.getResources().openRawResource(R.raw.system_integer_sensors));
        } catch (IOException | JSONException e) {
            Log.e(
                    TAG,
                    "Error reading in JSON Metadata for system integer sensors. More info: "
                            + e.toString());
        }
        try {
            mIntegerMetadata.putAll(
                    JsonMetadataReader.readIntegerSensorIndexFromJson(
                            context.getResources().openRawResource(R.raw.vendor_integer_sensors)));
        } catch (IOException | JSONException e) {
            Log.e(
                    TAG,
                    "Error reading in JSON Metadata for vendor integer sensors. More info: "
                            + e.toString());
        }

        // ECU Metadata
        try {
            mECUMetadataMap =
                    JsonMetadataReader.readECUsFromJson(
                            context.getResources().openRawResource(R.raw.vendor_ecus));
        } catch (IOException | JSONException e) {
            Log.e(TAG, "Error reading in JSON Metadata for ECUs. More info: " + e.toString());
        }
    }

    /**
     * Maintains a singleton object and allows access to it. If no singleton object is created the
     * method will create one based on passed in context
     *
     * @param context Any context that allows access to res/raw
     * @return Singleton MetadataProcessing object
     */
    public static synchronized MetadataProcessing getInstance(Context context) {
        if (sInstance == null) {
            sInstance = new MetadataProcessing(context);
        }
        return sInstance;
    }

    public SensorMetadata getFloatMetadata(Integer floatSensorIndex) {
        return mFloatMetadata.getOrDefault(floatSensorIndex, null);
    }

    public SensorMetadata getIntegerMetadata(Integer integerSensorIndex) {
        return mIntegerMetadata.getOrDefault(integerSensorIndex, null);
    }

    public DTCMetadata getDTCMetadata(String dtcCode) {
        return mDTCMetadataMap.getOrDefault(dtcCode, null);
    }

    public String getECUMetadata(String ecuAddress) {
        return mECUMetadataMap.getOrDefault(ecuAddress, null);
    }
}
