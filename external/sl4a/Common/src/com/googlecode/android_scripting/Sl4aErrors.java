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

package com.googlecode.android_scripting;

import com.googlecode.android_scripting.jsonrpc.JsonRpcResult;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * A enum of Sl4aException objects. This enum holds all of the innate systematic
 * errors that can be sent to the client. Errors that come from Facades should
 * not use these Sl4aErrors, unless the Facades themselves are designed to
 * modify the state of SL4A's connection
 */
public enum Sl4aErrors {
    // Process-Level Errors 0-99

    // JSON RPC Errors 100 - 199
    JSON_RPC_UNKNOWN_EXCEPTION(100,
            "Something went horribly wrong when parsing or returning your request."),
    JSON_RPC_REQUEST_NOT_JSON(101, "The request sent was not a valid JSONObject."),
    JSON_RPC_MISSING_ID(102, "The \"id\" field is missing."),
    JSON_RPC_MISSING_METHOD(103, "The \"method\" field is missing."),
    JSON_RPC_METHOD_NOT_STRING(104, "The \"method\" field must be a string."),
    JSON_RPC_PARAMS_NOT_AN_ARRAY(105, "The \"params\" field must be an array."),
    JSON_RPC_UNKNOWN_RPC_METHOD(106, "No known RPC for the given \"method\"."),
    JSON_RPC_RESULT_NOT_JSON_VALID(107,
            "The JsonBuilder was unable to convert the result to valid JSON."),
    JSON_RPC_FAILED_TO_BUILD_RESULT(108, "The JsonBuilder failed to build the result."),
    JSON_RPC_FACADE_EXCEPTION_OCCURRED(109, "An exception occurred while handling this RPC. "
            + "Check the \"data\" field for the error received."),
    JSON_RPC_INVALID_PARAMETERS(110, "The \"params\" given are not valid for this \"method\"."),

    // Session Errors 200 - 299
    SESSION_UNKNOWN_EXCEPTION(200, "Something went horribly wrong when handling your session."),
    SESSION_INVALID(201, "This session no longer exists or is invalid."),
    ;

    private final Sl4aException mSl4aException;

    Sl4aErrors(int errorCode, String errorMessage) {
        mSl4aException = new Sl4aException(errorCode, errorMessage);
    }

    /**
     * Returns the underlying {@see Sl4aException}.
     */
    public Sl4aException getError() {
        return mSl4aException;
    }

    /**
     * Converts this Sl4aError to a JSON-RPC 2.0 compliant JSONObject.
     * @param id the id given by the request
     * @return a JSON-RPC 2.0 error response as a JSONObject
     * @throws JSONException
     */
    public JSONObject toJson(Object id) throws JSONException {
        return JsonRpcResult.error(id, mSl4aException);
    }

    /**
     * Converts this Sl4aError to a JSON-RPC 2.0 compliant JSONObject.
     * @param id      the id given by the request
     * @param details additional details to pass along with the error
     * @return a JSON-RPC 2.0 error response as a JSONObject
     * @throws JSONException
     */
    public JSONObject toJson(Object id, Object details) throws JSONException {
        return JsonRpcResult.error(id, mSl4aException, details);
    }
}
