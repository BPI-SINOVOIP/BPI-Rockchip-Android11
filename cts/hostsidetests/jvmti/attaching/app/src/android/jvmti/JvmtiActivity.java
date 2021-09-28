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

package android.jvmti;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.lang.Override;

import java.io.PrintStream;
import java.nio.charset.Charset;
import java.net.InetAddress;
import java.net.InetSocketAddress;

/**
 * A specialized activity. This is separate from the other agent tests as we can't use
 * instrumentation for this.
 */
public class JvmtiActivity extends Activity {

    private static final String SOCK_NAME = "CtsJvmtiAttachingHostTestCases_SOCKET";
    private final static String APP_DESCRIPTOR = "Landroid/jvmti/JvmtiApplication;";
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        try {
          runTest();
          returnResult("SUCCESS");
        } catch (Throwable e) {
          ByteArrayOutputStream baos = new ByteArrayOutputStream();
          e.printStackTrace(new PrintStream(baos));

          returnResult("FAILED: " + baos.toString());
        }
    }

    void returnResult(final String s) {
      // Annoying, well-intentioned, 'sanity' checks prevent us from performing network actions on
      // the current thread. Create another one and wait for it.
      Thread t = new Thread(() -> {
          try (LocalSocket ls = new LocalSocket()) {
              // LocalSocket doesn't support timeout. It doesn't really matter since the host-side
              // has one.
              ls.connect(new LocalSocketAddress(SOCK_NAME));
              DataOutputStream dos = new DataOutputStream(ls.getOutputStream());
              dos.writeUTF(s);
              dos.flush();
              // Wait for the other end to close the connection.
              while (-1 != ls.getInputStream().read()) {}
          } catch (IOException e) {
              ByteArrayOutputStream baos = new ByteArrayOutputStream();
              e.printStackTrace(new PrintStream(baos));
              Log.wtf("JvmtiActivity", "Could not connect to result socket! " + e + baos.toString());
          }
      });
      t.start();
      try {
        t.join();
      } catch (Exception e) {}
    }

    public void runTest() throws Exception {
        System.out.println("Checking for " + APP_DESCRIPTOR);
        if (!didSeeLoadOf(APP_DESCRIPTOR)) {
            throw new IllegalStateException("Did not see the load of the application class!");
        }
    }

    private static native boolean didSeeLoadOf(String s);
}
