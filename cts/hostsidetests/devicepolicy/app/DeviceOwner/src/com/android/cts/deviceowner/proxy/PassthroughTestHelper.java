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
import java.io.InputStream;
import java.io.PrintStream;
import java.net.ServerSocket;
import java.net.Socket;

import android.net.ProxyInfo;

/**
 * Used to test the HTTP CONNECT on a proxy, by holding
 * both sides of the connection and passing data back
 * and forth.
 */
public class PassthroughTestHelper {
  private ProxyInfo mProxy;

  public String error = null;
  public boolean isTestDone = false;

  private final String[] testStrings = new String[] {
      "These are test lines that will be sent",
      "They must contain the same data on both sides",
      "rEal data will contain some encrypted data",
      "and maybe some other stuff -!@$^@#$^1235432",
  };
  private PrintStream mClientPrint;
  private InputStream mClientRead;

  public PassthroughTestHelper(ProxyInfo proxy) {
    mProxy = proxy;
  }

  public void runTest() throws Exception {
    // Open up a server socket that the proxy will connect to.
    ServerSocket server = new ServerSocket(0);
    // Ask the proxy to connect to our server socket and wait for
    // the proxy to tell us its ok.
    Socket s = createHttpConnectConnection(server.getLocalPort());

    // Accept the other side of the connection from the server.
    Socket serverSock = server.accept();
    PrintStream serverPrint = new PrintStream(serverSock.getOutputStream());
    InputStream serverRead = serverSock.getInputStream();

    // Pass some data in one direction.
    passthrough(mClientPrint, serverRead);
    // Pass some data in the other direction.
    passthrough(serverPrint, mClientRead);

    s.close();
    server.close();
  }

  private Socket createHttpConnectConnection(int port) throws Exception {
    // Open a connection to the proxy.
    Socket s = new Socket(mProxy.getHost(), mProxy.getPort());
    if (!s.isConnected()) {
      throw new IOException("Must connect to proxy");
    }

    mClientPrint = new PrintStream(s.getOutputStream());
    mClientRead = s.getInputStream();

    // Send the proxy a request to connect to our ServerSocket.
    mClientPrint.println("CONNECT localhost:" + port + " HTTP/1.0\r\n\r");
    mClientPrint.flush();

    // Read the response and verify we are connected.
    String line = readAsciiLine(mClientRead);
    if (!line.equals("HTTP/1.1 200 OK")) {
      throw new IOException("Proxy not connected: " + line);
    }
    // Extra line after OK
    readAsciiLine(mClientRead);
    return s;
  }

  /**
   * Read a line off from a connection to the proxy.
   */
  public static String readAsciiLine(InputStream in) throws IOException {
    StringBuilder result = new StringBuilder(80);
    while (true) {
      int c = in.read();
      if (c == -1) {
        throw new IOException();
      } else if (c == '\n') {
        break;
      }

      result.append((char) c);
    }
    int length = result.length();
    if (length > 0 && result.charAt(length - 1) == '\r') {
      result.setLength(length - 1);
    }
    return result.toString();
  }

  private void passthrough(PrintStream print, InputStream read) throws Exception {
    // Write out a bunch of strings and read them in on the other side.
    for (String testString : testStrings) {
      print.println(testString);
      print.flush();
      String line = readAsciiLine(read);
      if (!line.equals(testString)) {
        throw new Exception("Unmatched line: " + line + "\nWith:" + testString);
      }
    }
  }
}