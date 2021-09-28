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

package com.android.cts.deviceowner.proxy;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.apache.http.HttpException;
import org.apache.http.HttpRequest;
import org.apache.http.HttpResponse;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.DefaultConnectionReuseStrategy;
import org.apache.http.impl.DefaultHttpResponseFactory;
import org.apache.http.impl.DefaultHttpServerConnection;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.protocol.BasicHttpContext;
import org.apache.http.protocol.BasicHttpProcessor;
import org.apache.http.protocol.HttpContext;
import org.apache.http.protocol.HttpRequestHandler;
import org.apache.http.protocol.HttpRequestHandlerRegistry;
import org.apache.http.protocol.HttpService;
import org.apache.http.protocol.ResponseConnControl;
import org.apache.http.protocol.ResponseContent;
import org.apache.http.protocol.ResponseDate;
import org.apache.http.protocol.ResponseServer;

import android.util.Log;

/**
 * This class hosts a local http server for the PacProxyTest.  Since PAC
 * is setup as a URL to a PAC file on android we host a local HTTP server
 * that will allow specified test files to be downloaded.
 */
public class PacFileServer extends Thread implements HttpRequestHandler {
  static final String TAG = "ProxyCtsTest";
  private static final String ALL_PATTERN = "*";

  private boolean mRunning = false;

  private HttpService mHttpService = null;

  private BasicHttpContext mHttpContext;

  private ServerSocket mServerSocket;

  private String mPacFile;
  private final CountDownLatch mPacSentLatch;

  /**
   * @param pacFile PAC file to respond to HTTP connections with
   * @throws IOException From ServerSocket construction
   */
  public PacFileServer(String pacFile) throws IOException {
    mPacFile = pacFile;
    mPacSentLatch = new CountDownLatch(1);
    BasicHttpProcessor httpProc = new BasicHttpProcessor();
    mHttpContext = new BasicHttpContext();

    httpProc.addInterceptor(new ResponseDate());
    httpProc.addInterceptor(new ResponseServer());
    httpProc.addInterceptor(new ResponseContent());
    httpProc.addInterceptor(new ResponseConnControl());

    mHttpService = new HttpService(httpProc,
        new DefaultConnectionReuseStrategy(), new DefaultHttpResponseFactory());
    HttpRequestHandlerRegistry registry = new HttpRequestHandlerRegistry();

    registry.register(ALL_PATTERN, this);
    mHttpService.setHandlerResolver(registry);
    mServerSocket = new ServerSocket(0);
  }

  @Override
  public void run() {
    try {
      mServerSocket.setReuseAddress(true);
      while (mRunning) {
        try {
          Socket connection = mServerSocket.accept();

          DefaultHttpServerConnection serverConnection = new DefaultHttpServerConnection();
          serverConnection.bind(connection, new BasicHttpParams());
          mHttpService.handleRequest(serverConnection, mHttpContext);
          serverConnection.shutdown();
        } catch (IOException | HttpException e) {
          Log.d(TAG, "HTTP Response failed", e);
        }
      }
      mServerSocket.close();
    } catch (IOException e) {
      Log.d(TAG, "Socket issues", e);
    } finally {
      mRunning = false;
    }
  }

  public void startServer() {
    mRunning = true;
    start();
  }

  public void stopServer() {
    mRunning = false;
    if (mServerSocket != null) {
      try {
        mServerSocket.close();
      } catch (IOException e) {
        Log.d(TAG, "Couldn't close socket", e);
      }
    }
  }

  /**
   * Sets a new PAC file to be downloaded for future HTTP requests.
   */
  public void setPacFile(String pacFile) {
    mPacFile = pacFile;
  }

  /**
   * Get URL that points at this web server.
   */
  public String getPacURL() {
    return "http://localhost:" + mServerSocket.getLocalPort() + "/proxy.pac";
  }

  /**
   * Whether the PAC file has been downloaded.
   */
  public boolean awaitPacSent(long timeout, TimeUnit timeUnit) throws InterruptedException {
    return mPacSentLatch.await(timeout, timeUnit);
  }

  @Override
  public void handle(HttpRequest request, HttpResponse response, HttpContext context)
      throws HttpException, IOException {
    if (mPacFile != null) {
      StringEntity entity = new StringEntity(mPacFile);
      response.setHeader("Content-Type", "application/x-ns-proxy-autoconfig");
      response.setEntity(entity);
      mPacSentLatch.countDown();
    } else {
      throw new IOException("No PAC Set");
    }
  }
}