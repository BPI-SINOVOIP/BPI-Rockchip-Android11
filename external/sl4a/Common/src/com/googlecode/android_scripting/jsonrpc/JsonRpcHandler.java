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

import com.googlecode.android_scripting.Log;
import com.googlecode.android_scripting.Sl4aErrors;
import com.googlecode.android_scripting.rpc.MethodDescriptor;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A class that parses given JSON RPC Messages, and handles their execution.
 */
public class JsonRpcHandler {
    private int mNextSessionId = 1;
    private String mSessionId = "";
    private final RpcReceiverManagerFactory mManagerFactory;

    public JsonRpcHandler(RpcReceiverManagerFactory managerFactory) {
        mManagerFactory = managerFactory;
    }

    /**
     * Returns the response object for the given request.
     * <p>
     * The response will be returned as a JSONObject, unless something fatal has occurred. In that
     * case, the resulting object will be a JSON formatted string.
     *
     * @param request the received request as a string
     * @return the response to the request
     */
    public Object getResponse(String request) {
        try {
            return getResponse(new JSONObject(request));
        } catch (JSONException ignored) {
            // May have been a bad request.
        }
        try {
            return Sl4aErrors.JSON_RPC_REQUEST_NOT_JSON.toJson(JSONObject.NULL);
        } catch (JSONException e) {
            // This error will never occur.
            Log.e("Received JSONException on invalid request.", e);
            return JsonRpcResult.wtf();
        }
    }

    /**
     * Returns the response for the given request.
     * <p>
     * This message will be returned as a JSONObject, unless something fatal has occurred. In that
     * case, the resulting object will be a JSON formatted string.
     *
     * @param request the JSON-RPC request message as a JSONObject
     * @return the response to the request
     */
    public Object getResponse(JSONObject request) {
        if (isSessionManagementRpc(request)) {
            return handleSessionRpc(request);
        }

        Object validationErrors = getValidationErrors(request);
        if (validationErrors != null) {
            return validationErrors;
        }

        try {
            Object id = request.get("id");
            String method = request.getString("method");

            JSONArray params = new JSONArray();
            if (request.has("params")) {
                params = request.getJSONArray("params");
            }

            RpcReceiverManager receiverManager =
                    mManagerFactory.getRpcReceiverManagers().get(mSessionId);
            if (receiverManager == null) {
                Log.e("Unable to find sessionId \"" + mSessionId + "\".");
                for (String key : mManagerFactory.getRpcReceiverManagers().keySet()) {
                    Log.d("Available key: " + key);
                }
                return Sl4aErrors.SESSION_INVALID.toJson(id, mSessionId);
            }
            MethodDescriptor rpc = receiverManager.getMethodDescriptor(method);
            if (rpc == null) {
                return Sl4aErrors.JSON_RPC_UNKNOWN_RPC_METHOD.toJson(id, method);
            }
            Object rpcResult;
            try {
                rpcResult = rpc.invoke(receiverManager, params);
            } catch (Throwable t) {
                Log.e("RPC call threw an error.", t);
                return Sl4aErrors.JSON_RPC_FACADE_EXCEPTION_OCCURRED.toJson(id, t);
            }
            try {
                return JsonRpcResult.result(id, rpcResult);
            } catch (JSONException e) {
                // This error is rare, but may occur when the JsonBuilder fails to properly
                // convert the resulting class from the RPC into valid JSON. This may happen
                // when a resulting number is returned as NaN or infinite, but is not converted
                // to a String first.
                Log.e("Unable to build object of class \"" + rpcResult.getClass() + "\".", e);
                return Sl4aErrors.JSON_RPC_RESULT_NOT_JSON_VALID.toJson(id, e);
            } catch (Throwable t) {
                // This error will occur whenever the underlying Android APIs built against are
                // unavailable for the version of Android SL4A is installed onto.
                Log.e("Unable to build object of class \"" + rpcResult.getClass() + "\".", t);
                return Sl4aErrors.JSON_RPC_FAILED_TO_BUILD_RESULT.toJson(id, t);
            }
        } catch (JSONException exception) {
            // This error should never happen. NULL and all values found within Sl4aErrors are
            // guaranteed not to raise an error when being converted to JSON.
            // Also, all JSONObject.get() calls are only done after they have been validated to
            // exist after a corresponding JSONObject.has() call has been made.
            // Returning a raw string here to prevent the need to catch JsonRpcResult.error().
            Log.e(exception);
            return JsonRpcResult.wtf();
        }
    }

    /**
     * Returns a validation error if the request is invalid. Null otherwise.
     * @param message the request message
     * @return a JSONObject error response, or null if the message is valid
     */
    private Object getValidationErrors(JSONObject message) {
        Object id = JSONObject.NULL;
        try {
            if (!message.has("id")) {
                return Sl4aErrors.JSON_RPC_MISSING_ID.toJson(id);
            } else {
                id = message.get("id");
            }
            if (!message.has("method")) {
                return Sl4aErrors.JSON_RPC_MISSING_METHOD.toJson(id);
            } else if (!(message.get("method") instanceof String)) {
                return Sl4aErrors.JSON_RPC_METHOD_NOT_STRING.toJson(id);
            }
            if (message.has("params") && !(message.get("params") instanceof JSONArray)) {
                return Sl4aErrors.JSON_RPC_PARAMS_NOT_AN_ARRAY.toJson(id);
            }
        } catch (JSONException exception) {
            // This error should never happen. NULL and all values found within Sl4aException are
            // guaranteed not to raise an error when being converted to JSON.
            // Also, all JSONObject.get() calls are only done after they have been validated to
            // exist after a corresponding JSONObject.has() call has been made.
            // Returning a raw string here to prevent the need to wrap JsonRpcResult.error().
            Log.e(exception);
            return JsonRpcResult.wtf();
        }
        return null;
    }

    //TODO(markdr): Refactor this function into a class that handles session management.
    private boolean isSessionManagementRpc(JSONObject request) {
        return request.has("cmd") && request.has("uid");
    }

    //TODO(markdr): Refactor this function into a class that handles session management.
    private Object handleSessionRpc(JSONObject request) {
        try {
            Object id = request.get("uid");
            String cmd = request.getString("cmd");
            switch (cmd) {
                case "initiate": {
                    String newId;
                    synchronized (mManagerFactory) {
                        newId = "" + mNextSessionId;
                        while (mManagerFactory.getRpcReceiverManagers().containsKey(newId)) {
                            newId = "" + mNextSessionId++;
                        }
                        mSessionId = newId;
                        mManagerFactory.create(newId);
                    }
                    return "{\"uid\": \"" + newId + "\", \"status\":true}";
                }
                case "continue": {
                    for (String key : mManagerFactory.getRpcReceiverManagers().keySet()) {
                        Log.d("Available key: " + key);
                    }
                    String stringId = id.toString();
                    boolean canContinue =
                            mManagerFactory.getRpcReceiverManagers().containsKey(stringId);
                    if (canContinue) {
                        mSessionId = stringId;
                        return "{\"uid\":\"" + id + "\",\"status\":true}";
                    }
                    return "{\"uid\":\"" + id + "\",\"status\":false,"
                            + "\"error\":\"Session does not exist.\"}";
                }
                default: {
                    // This case should never be reached if isSessionManagementRpc is called first.
                    Log.e("Hit default case for handling session rpcs.");
                    return JsonRpcResult.wtf();
                }
            }
        } catch (JSONException jsonException) {
            // This error will never occur, because the above IDs and objects will always be JSON
            // serializable.
            Log.e("Exception during handleRpcCall: " + jsonException);
            return JsonRpcResult.wtf();
        }
    }
}
