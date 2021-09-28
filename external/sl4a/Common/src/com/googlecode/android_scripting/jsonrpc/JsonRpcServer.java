/*
 * Copyright (C) 2017 The Android Open Source Project
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
import com.googlecode.android_scripting.SimpleServer;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;

/**
 * A JSON RPC server that forwards RPC calls to a specified receiver object.
 *
 */
public class JsonRpcServer extends SimpleServer {
    private final RpcReceiverManagerFactory mRpcReceiverManagerFactory;

    // private final String mHandshake;

    /**
     * Construct a {@link JsonRpcServer} connected to the provided {@link RpcReceiverManager}.
     *
     * @param managerFactory the {@link RpcReceiverManager} to register with the server
     * @param handshake the secret handshake required for authorization to use this server
     */
    public JsonRpcServer(RpcReceiverManagerFactory managerFactory, String handshake) {
        // mHandshake = handshake;
        mRpcReceiverManagerFactory = managerFactory;
    }

    @Override
    public void shutdown() {
        super.shutdown();
        // Notify all RPC receiving objects. They may have to clean up some of their state.
        for (RpcReceiverManager manager : mRpcReceiverManagerFactory.getRpcReceiverManagers()
                .values()) {
            manager.shutdown();
        }
    }

    private void send(PrintWriter writer, Object result) {
        writer.write(result + "\n");
        writer.flush();
        Log.v("Sent Response: " + result);
    }

    @Override
    protected void handleConnection(Socket socket) throws Exception {
        BufferedReader reader =
                new BufferedReader(new InputStreamReader(socket.getInputStream()), 8192);
        PrintWriter writer = new PrintWriter(socket.getOutputStream(), true);
        String data;
        JsonRpcHandler jsonRpcHandler = new JsonRpcHandler(mRpcReceiverManagerFactory);
        while ((data = reader.readLine()) != null) {
            Log.v("Received Request: " + data);
            send(writer, jsonRpcHandler.getResponse(data));
        }
    }
}
