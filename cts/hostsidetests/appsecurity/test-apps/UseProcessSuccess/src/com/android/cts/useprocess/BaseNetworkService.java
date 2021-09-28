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

package com.android.cts.useprocess;

import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;
import android.os.StrictMode;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;

public class BaseNetworkService extends Service {
    private static final String LOG_TAG = "ServiceWithInternet";

    public static final int TRANSACT_TEST = IBinder.FIRST_CALL_TRANSACTION;

    final boolean mNetworkAllowed;
    final String mExpectedProcessName;

    public static final String readResult(Parcel in) {
        if (in.readInt() != 0) {
            return in.readString();
        } else {
            return null;
        }
    }

    public static final String readResultCallstack(Parcel in) {
        return in.readString();
    }

    static final void writeResult(Parcel out, String errorMsg, Throwable where) {
        if (errorMsg != null) {
            out.writeInt(1);
            out.writeString(errorMsg);
            if (where == null) {
                where = new Exception();
                where.fillInStackTrace();
            }
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw, false);
            where.printStackTrace(pw);
            pw.flush();
            out.writeString(sw.toString());
        } else {
            out.writeInt(0);
        }
    }

    private final IBinder mBinder = new Binder() {
        @Override
        protected boolean onTransact(int code, Parcel data, Parcel reply, int flags)
                throws RemoteException {
            if (code == TRANSACT_TEST) {
                try {
                    StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                            .permitNetwork().build());
                    doTest(reply);
                } catch (Throwable e) {
                    writeResult(reply, "Unexpected exception: " + e, e);
                }
                return true;
            } else {
                return super.onTransact(code, data, reply, flags);
            }
        }

        private void doTest(Parcel reply) {
            if (!mExpectedProcessName.equals(getApplication().getProcessName())) {
                writeResult(reply,
                        "Not running in correct process, expected " + mExpectedProcessName
                        + " but got " + getApplication().getProcessName(),null);
                return;
            }

            try {
                Socket socket = new Socket("example.com", 80);
                socket.close();
                if (!mNetworkAllowed) {
                    writeResult(reply,
                            "Create inet socket did not throw SecurityException as expected",
                            null);
                    return;
                }
            } catch (SecurityException e) {
                if (mNetworkAllowed) {
                    writeResult(reply,
                            "Create inet socket should be allowed but: " + e, e);
                    return;
                }
            } catch (IOException e) {
                if (!mNetworkAllowed) {
                    writeResult(reply,
                            "Create inet socket did not throw SecurityException as expected", e);
                    return;
                }
            }

            int hasPerm = checkPermission(android.Manifest.permission.INTERNET,
                    Process.myPid(), Process.myUid());
            if (mNetworkAllowed) {
                if (hasPerm != PackageManager.PERMISSION_GRANTED) {
                    writeResult(reply, "Should have INTERNET permission, but got: " + hasPerm,
                            null);
                    return;
                }
            } else {
                if (hasPerm != PackageManager.PERMISSION_DENIED) {
                    writeResult(reply, "Shouldn't have INTERNET permission, but got: " + hasPerm,
                            null);
                    return;
                }
            }

            // A local socket binding is not allowed if you don't have network access, and
            // also not allowed for instant aps.
            final boolean allowLocalSocket = mNetworkAllowed
                    && !getPackageManager().isInstantApp();

            try {
                // Transfer 128K of data across an explicitly localhost socket.
                final int byteCount = 1024;
                final int packetCount = 128;
                final ServerSocket server;
                try {
                    server = new ServerSocket(0);
                    if (!allowLocalSocket) {
                        server.close();
                        writeResult(reply,
                                "Create local socket did not throw SecurityException as expected",
                                null);
                        return;
                    }
                } catch (SocketException e) {
                    if (allowLocalSocket) {
                        writeResult(reply,
                                "Create local socket should be allowed but: " + e, null);
                    }
                    return;
                }
                new Thread("TrafficStatsTest.testTrafficStatsForLocalhost") {
                    @Override
                    public void run() {
                        try {
                            Socket socket = new Socket("localhost", server.getLocalPort());
                            OutputStream out = socket.getOutputStream();
                            byte[] buf = new byte[byteCount];
                            for (int i = 0; i < packetCount; i++) {
                                out.write(buf);
                                out.flush();
                            }
                            out.close();
                            socket.close();
                        } catch (IOException e) {
                            writeResult(reply, "Badness during writes to socket: " + e, e);
                            return;
                        }
                    }
                }.start();

                int read = 0;
                try {
                    Socket socket = server.accept();
                    InputStream in = socket.getInputStream();
                    byte[] buf = new byte[byteCount];
                    while (read < byteCount * packetCount) {
                        int n = in.read(buf);
                        if (n <= 0) {
                            writeResult(reply, "Unexpected EOF @ " + read,null);
                            return;
                        }
                        read += n;
                    }
                } finally {
                    server.close();
                }
                if (read != (byteCount * packetCount)) {
                    writeResult(reply, "Not all data read back: expected "
                            + (byteCount * packetCount) + ", received" + read, null);
                    return;

                }
            } catch (IOException e) {
                writeResult(reply, e.toString(), e);
                return;
            }

            writeResult(reply, null, null);
        }
    };

    public BaseNetworkService(boolean networkAllowed, String expectedProcessName) {
        mNetworkAllowed = networkAllowed;
        mExpectedProcessName = expectedProcessName;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
