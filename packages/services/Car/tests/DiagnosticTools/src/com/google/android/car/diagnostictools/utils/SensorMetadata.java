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

import android.util.Log;

import com.google.android.car.diagnostictools.LiveDataAdapter;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Stores metadata about a sensor and provides methods to better understand the sensor data.
 * Provides support for arbitrary translation, scaling (conversion) following the format [Scaled
 * Data Value] = [Offset] + [Scale] x [Raw Decimal Data Value], and mapping from integer to String.
 */
public class SensorMetadata {

    private static final String TRANSLATION_KEY = "translation";
    private static final String NAME_KEY = "name";
    private static final String UNITS_KEY = "units";
    private static final String CONVERSION_KEY = "conversion";
    private static final String MAPPING_KEY = "mapping";
    private static final String TAG = "SensorMetadata";

    private String mTranslationString;
    private float mScale;
    private float mOffset;
    private String mName;
    private String mUnits;
    private Map<Integer, String> mMapping;
    private boolean mHasConversion;
    private boolean mHasMapping;

    /**
     * Takes in arbitrary translation string that should have x in it as a variable, sensor name,
     * and the sensor data units. For more information view MathEval.eval.
     *
     * @param translationString translation string with a 50 character limit. More information on
     *     usage in MathEval.eval.
     * @param name Name of sensor.
     * @param units Units of the sensor's data.
     */
    private SensorMetadata(String translationString, String name, String units) {
        if (MathEval.testTranslation(translationString)) {
            this.mTranslationString = translationString;
        } else {
            Log.e(
                    "ServiceCenterTools",
                    String.format(
                            "Invalid translation string %s for sensor %s because tests failed",
                            translationString, name));
            this.mTranslationString = "";
        }
        this.mName = name;
        this.mUnits = units;
        this.mHasConversion = false;
        this.mHasMapping = false;
    }

    /**
     * Takes in float scale and offset values, sensor name, and the sensor data units. Scaling and
     * mOffset are used in the following format: [Scaled Data Value] = [Offset] + [Scale] x [Raw
     * Decimal Data Value].
     *
     * @param scale Value to mScale sensor data by.
     * @param offset Value to shift sensor data by.
     * @param name Name of sensor.
     * @param units Units of the sensor's data.
     */
    private SensorMetadata(float scale, float offset, String name, String units) {
        this.mScale = scale;
        this.mOffset = offset;
        this.mName = name;
        this.mUnits = units;
        this.mHasConversion = true;
        this.mHasMapping = false;
    }

    /**
     * Takes in a Integer -> String mapping and sensor mName.
     *
     * @param mapping Mapping between Integer sensor values to their String values
     * @param name Name of sensor.
     */
    private SensorMetadata(Map<Integer, String> mapping, String name) {
        this.mMapping = mapping;
        this.mName = name;
        this.mUnits = "";
        this.mHasMapping = true;
        this.mHasConversion = false;
    }

    /**
     * Takes in json object which represents the metadata for a single sensor and converts it to a
     * SensorMetadata object. Supports mappings, structured conversions (mScale and mOffset), and
     * arbitrary translations. Mappings and conversions are preferred and translation read in if
     * neither are present.
     *
     * @param json JSON object for metadata
     * @return returns a new SensorMeta data object with the data from the JSON object
     */
    static SensorMetadata buildFromJson(JSONObject json) {
        String name = readStringFromJSON(NAME_KEY, json);
        String units = readStringFromJSON(UNITS_KEY, json);

        if (json.has(CONVERSION_KEY)) {
            try {
                JSONObject conversionObject = json.getJSONObject(CONVERSION_KEY);
                float scale = readFloatFromJSON("scale", conversionObject);
                float offset = readFloatFromJSON("offset", conversionObject);

                return new SensorMetadata(scale, offset, name, units);
            } catch (JSONException e) {
                Log.d(TAG, "buildFromJson: Error reading conversion for " + name + ": " + e);
            }
        } else if (json.has(MAPPING_KEY)) {
            try {
                Map<Integer, String> mapping = readMappingFromJSON(MAPPING_KEY, json);
                return new SensorMetadata(mapping, name);
            } catch (JSONException e) {
                Log.d(TAG, "buildFromJson: Error reading mapping: " + e);
            }
        }
        String translationString = readStringFromJSON(TRANSLATION_KEY, json);

        return new SensorMetadata(translationString, name, units);
    }

    /**
     * Helper function that reads a string at a certain key from a JSONObject. Function wraps the
     * JSON functions with appropriate exception handling.
     *
     * @param key key at which the desired string value is located
     * @param json JSONObject that has the string value at the key
     * @return value at key or null if error or not found
     */
    private static String readStringFromJSON(String key, JSONObject json) {
        try {
            if (json.has(key)) {
                return json.getString(key);
            }
        } catch (JSONException e) {
            Log.e(TAG, "Error reading " + key + " for String SensorMetadata");
        }
        return "";
    }

    /**
     * Helper function that reads a float at a certain key from a JSONObject. Function wraps the
     * JSON functions with appropriate exception handling.
     *
     * @param key key at which the desired float value is located
     * @param json JSONObject that has the float value at the key
     * @return value at key or null if error or not found
     * @throws JSONException Throws JSONException if there is any
     */
    private static float readFloatFromJSON(String key, JSONObject json) throws JSONException {
        try {
            if (json.has(key)) {
                return (float) json.getDouble(key);
            } else {
                Log.e(TAG, "No tag for " + key + " is found");
                throw new JSONException(String.format("%s missing from object", key));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Error reading " + key + " for float SensorMetadata");
            throw e;
        }
    }

    /**
     * Helper function that reads a mapping at a certain key from a JSONObject. Function wraps the
     * JSON functions with appropriate exception handling.
     *
     * @param key key at which the desired float value is located
     * @param json JSONObject that has the float value at the key
     * @return value at key or null if error or not found
     * @throws JSONException Throws JSONException if there is any
     */
    private static Map<Integer, String> readMappingFromJSON(String key, JSONObject json)
            throws JSONException {
        try {
            if (json.has(key)) {
                json = json.getJSONObject(key);
                Map<Integer, String> map = new HashMap<>();
                Iterator<String> keys = json.keys();

                while (keys.hasNext()) {
                    String integer = keys.next();
                    if (json.get(integer) instanceof String && isInteger(integer)) {
                        map.put(Integer.parseInt(integer), json.getString(integer));
                    }
                }
                return map;
            } else {
                Log.e(TAG, "No tag for " + key + " is found");
                throw new JSONException(String.format("%s missing from object", key));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Error reading " + key + " for mapping SensorMetadata");
            throw e;
        }
    }

    /**
     * Helper function to quickly check if a String can be converted to an int. From
     * https://stackoverflow.com/a/237204
     *
     * @param str String to check.
     * @return true if String can be converted to an int, false if not.
     */
    private static boolean isInteger(String str) {
        if (str == null) {
            return false;
        }
        int length = str.length();
        if (length == 0) {
            return false;
        }
        int i = 0;
        if (str.charAt(0) == '-') {
            if (length == 1) {
                return false;
            }
            i = 1;
        }
        for (; i < length; i++) {
            char c = str.charAt(i);
            if (c < '0' || c > '9') {
                return false;
            }
        }
        return true;
    }

    /**
     * Uses appropriate translation method (mapping, conversion, translation string) to convert
     * float sensor data into true value
     *
     * @param data Float of data to translate
     * @return Processed data
     */
    private String translateData(Float data) {
        if (mHasConversion) {
            return String.valueOf(data * mScale + mOffset);
        } else if (mHasMapping) {
            return mMapping.getOrDefault(Math.round(data), "No mapping available");
        }
        try {
            return String.valueOf((float) MathEval.eval(mTranslationString, data));
        } catch (MathEval.TranslationTooLongException e) {
            Log.e("ServiceCenterTools", "Translation: " + mTranslationString + " too long");
            return String.valueOf(data);
        }
    }

    @Override
    public String toString() {
        return "Metadata for "
                + mName
                + " with units: "
                + mUnits
                + " and translation: "
                + mTranslationString;
    }

    /**
     * Translates and wraps data in a SensorDataWrapper object to allow for it to be displayed
     *
     * @param data Sensor data to translate and wrapp
     * @return SensorDataWrapper that can be displayed in the LiveDataAdapter
     */
    public LiveDataAdapter.SensorDataWrapper toLiveDataWrapper(float data) {
        return new LiveDataAdapter.SensorDataWrapper(mName, mUnits, this.translateData(data));
    }
}
