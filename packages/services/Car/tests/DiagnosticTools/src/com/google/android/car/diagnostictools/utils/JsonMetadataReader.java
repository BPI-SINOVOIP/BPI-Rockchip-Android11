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

import android.car.diagnostic.FloatSensorIndex;
import android.car.diagnostic.IntegerSensorIndex;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/**
 * Takes in an InputStream of a JSON file and converts it into a mapping from Integer (SensorIndex
 * IntDef) to a SensorMetadata object
 *
 * <p>Two similar methods used are read(Float|Integer)SensorIndexFromJson. These utilize the
 * (Float|Integer)SensorIndex.class objects to allow the JSON object to utilize @IntDef variable
 * names.
 *
 * <p>Additionally two similar methods are read(DTCs|ECUs)FromJson which take in an input stream and
 * return a mapping between DTCs/ECUs and their respective metadata (DTCMetadata or String
 * respectively)
 */
class JsonMetadataReader {

    private static final String TAG = "JsonMetadataReader";

    /**
     * Converts a JSON file with DTC -> DTCMetadata formatting into the corresponding mapping
     *
     * @param in InputStream for JSON file
     * @return Mapping from String to SensorMetadata
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    static Map<String, DTCMetadata> readDTCsFromJson(InputStream in)
            throws IOException, JSONException {
        JSONObject metadataMapping = new JSONObject(inputStreamToString(in));
        Map<String, DTCMetadata> metadata = new HashMap<>();
        for (String dtc : metadataMapping.keySet()) {
            try {
                metadata.put(dtc, DTCMetadata.buildFromJson(metadataMapping.getJSONObject(dtc)));
            } catch (JSONException e) {
                Log.d(TAG, "Invalid JSON for DTC: " + dtc);
            }
        }
        return metadata;
    }

    /**
     * Converts a JSON file with ECU -> String formatting into the corresponding mapping
     *
     * @param in InputStream for JSON file
     * @return Mapping from String to SensorMetadata
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    static Map<String, String> readECUsFromJson(InputStream in) throws IOException, JSONException {
        JSONObject metadataMapping = new JSONObject(inputStreamToString(in));
        Map<String, String> metadata = new HashMap<>();
        for (String address : metadataMapping.keySet()) {
            try {
                metadata.put(address, metadataMapping.getString(address));
            } catch (JSONException e) {
                Log.d(TAG, "Invalid JSON for ECU: " + address);
            }
        }

        return metadata;
    }

    /**
     * Takes an InputStream of JSON and converts it to a mapping from FloatSensorIndex field value
     * to Sensor Metadata objects
     *
     * @param in InputStream of JSON
     * @return Mapping from FloatSensorIndex field values to SensorMetadata objects
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    static Map<Integer, SensorMetadata> readFloatSensorIndexFromJson(InputStream in)
            throws IOException, JSONException {
        return readSensorIndexFromJson(in, FloatSensorIndex.class);
    }

    /**
     * Takes an InputStream of JSON and converts it to a mapping from IntegerSensorIndex field value
     * to Sensor Metadata objects
     *
     * @param in InputStream of JSON
     * @return Mapping from IntegerSensorIndex field values to SensorMetadata objects
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    static Map<Integer, SensorMetadata> readIntegerSensorIndexFromJson(InputStream in)
            throws IOException, JSONException {
        return readSensorIndexFromJson(in, IntegerSensorIndex.class);
    }

    /**
     * Takes an InputStream and Class to read in as a SensorIndex. The Class is used to connect with
     * reflections and assert that key names are consistent with the SensorIndex class input
     *
     * @param in InputStream of JSON file
     * @param sensorIndexClass Defines which of the (Float|Integer)SensorIndex classes does the key
     *     for the output mapping link to
     * @return Mapping from sensorIndexClass field values to SensorMetadata objects
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    private static Map<Integer, SensorMetadata> readSensorIndexFromJson(
            InputStream in, Class sensorIndexClass) throws IOException, JSONException {
        Map<String, SensorMetadata> metadata = JsonMetadataReader.readSensorsFromJson(in);
        Map<Integer, SensorMetadata> metadataMap = new HashMap<>();
        // Use Reflection to get all fields of the specified sensorIndexClass
        Field[] sensorIndexFields = sensorIndexClass.getFields();
        // For all fields, if the JSON object contains the field then put it into the map
        for (Field f : sensorIndexFields) {
            if (metadata.containsKey(f.getName())) {
                try {
                    metadataMap.put((Integer) f.get(sensorIndexClass), metadata.get(f.getName()));
                } catch (IllegalAccessException e) { // Added for safety though should never
                    // trigger
                    Log.e(TAG, "Illegal Access Exception throws but should not be thrown");
                }
            }
        }
        return metadataMap;
    }

    /**
     * Helper method that creates an unchecked String to SensorMetadata mapping from an InputStream
     *
     * @param in InputStream for JSON file
     * @return Mapping from String to SensorMetadata
     * @throws IOException Thrown if there is an issue with reading the file from the InputStream
     * @throws JSONException Thrown if the JSON object is malformed
     */
    private static Map<String, SensorMetadata> readSensorsFromJson(InputStream in)
            throws IOException, JSONException {
        JSONObject metadataMapping = new JSONObject(inputStreamToString(in));
        Map<String, SensorMetadata> metadata = new HashMap<>();
        for (String sensorIndex : metadataMapping.keySet()) {
            // Use a static method from SensorMetadata to handle the conversion from
            // JSON->SensorMetadata
            metadata.put(
                    sensorIndex,
                    SensorMetadata.buildFromJson(metadataMapping.getJSONObject(sensorIndex)));
        }
        return metadata;
    }

    /**
     * Converts an InputStream into a String
     *
     * @param in InputStream
     * @return InputStream converted to a String
     * @throws IOException Thrown if there are issues in reading the file
     */
    private static String inputStreamToString(InputStream in) throws IOException {
        StringBuilder builder = new StringBuilder();
        try (BufferedReader reader =
                new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            reader.lines().forEach(builder::append);
        }
        return builder.toString();
    }
}
