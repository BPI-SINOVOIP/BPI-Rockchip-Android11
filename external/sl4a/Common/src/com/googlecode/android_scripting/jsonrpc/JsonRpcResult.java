/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.googlecode.android_scripting.jsonrpc;

import com.googlecode.android_scripting.Sl4aErrors;
import com.googlecode.android_scripting.Sl4aException;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Represents a JSON RPC result.
 *
 * @see http://json-rpc.org/wiki/specification
 */
public class JsonRpcResult {

    private JsonRpcResult() {
        // Utility class.
    }

    /**
     * Returns a JSON-RPC 1.0 formatted request response.
     *
     * @param id   the id given by the request
     * @param data the data to pass into the result field
     * @return a JSON-RPC 1.0 formatted request response
     * @throws JSONException if JsonBuilder.build() cannot correctly convert the object into JSON
     */
    public static JSONObject result(Object id, Object data) throws JSONException {
        JSONObject response = new JSONObject();
        response.put("id", id);
        response.put("result", JsonBuilder.build(data));
        response.put("error", JSONObject.NULL);
        return response;
    }

    /**
     * Returns a JSON-RPC 2.0 formatted error response.
     *
     * @param id      the id given by the request
     * @param message the error message to send
     * @return a JSON-RPC 2.0 formatted request response
     * @throws JSONException if the world is ending
     */
    public static JSONObject error(Object id, Object message) throws JSONException {
        JSONObject response = new JSONObject();
        if (id == null) {
            id = JSONObject.NULL;
        }
        response.put("id", id);
        response.put("result", JSONObject.NULL);
        response.put("error", message.toString());
        return response;
    }

    /**
     * Returns a JSON-RPC 2.0 formatted error response.
     *
     * @param id            the id given by the request
     * @param sl4aException the Sl4aException to send over JSON-RPC
     * @return a JSON-RPC 2.0 formatted error response
     * @throws JSONException if the world is ending
     */
    public static JSONObject error(Object id, Sl4aException sl4aException) throws JSONException {
        JSONObject response = new JSONObject();
        if (id == null) {
            id = JSONObject.NULL;
        }
        response.put("id", id);
        response.put("result", JSONObject.NULL);
        JSONObject error = new JSONObject();
        error.put("code", sl4aException.getErrorCode());
        error.put("message", sl4aException.getErrorMessage());
        response.put("error", error);
        return response;
    }

    /**
     * Returns a JSON-RPC 2.0 formatted error response.
     *
     * @param id        the id given by the request
     * @param sl4aError the Sl4aErrors object to send over JSON-RPC
     * @param details   additional data associated with the error
     * @return a JSON-RPC 1.0 formatted error response
     * @throws JSONException if details cannot be converted to JSON
     */
    public static JSONObject error(Object id, Sl4aErrors sl4aError, Object details)
            throws JSONException {
        return error(id, sl4aError.getError(), details);
    }


    /**
     * Returns a JSON-RPC 2.0 formatted error response.
     *
     * @param id            the id given by the request
     * @param sl4aException the Sl4aException to send over JSON-RPC
     * @param details       additional data associated with the error
     * @return a JSON-RPC 2.0 formatted error response
     * @throws JSONException if the world is ending
     */
    public static JSONObject error(Object id, Sl4aException sl4aException, Object details)
            throws JSONException {
        JSONObject returnValue = error(id, sl4aException);
        returnValue.getJSONObject("error").put("data", details);
        return returnValue;
    }

    /**
     * A function that returns a valid JSON-RPC error as a string when all else has failed.
     *
     * @return a String representation of {@see Sl4aErrors.JSON_RPC_UNKNOWN_EXCEPTION} in JSON
     */
    public static String wtf() {
        Sl4aException exception = Sl4aErrors.JSON_RPC_UNKNOWN_EXCEPTION.getError();
        return "{\"id\":null,"
                + "\"error\":{"
                + "\"code\":" + exception.getErrorCode() + ","
                + "\"message\":\"" + exception.getErrorMessage() + "\"}}";
    }
}
