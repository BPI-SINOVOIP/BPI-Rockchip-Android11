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

package android.inputmethodservice.cts.devicetest;

import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import java.io.BufferedReader;
import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Method;
import java.util.StringJoiner;

/**
 * A utility class to invoke a hidden API {@code IBinder#shellCommand(FileDescriptor in,
 * FileDescriptor out, FileDescriptor err, String[] args, ShellCallback shellCallback,
 * ResultReceiver resultReceiver)} via reflection for any the system server.
 */
class DirectShellCommand {
    /**
     * A {@link java.util.concurrent.Future} style result object.
     */
    @FunctionalInterface
    public interface FutureResult {
        /**
         * @param mills timeout in milliseconds.
         * @return Non-{@code null} object is the {@link Result} becomes available within timeout.
         *         Otherwise {@code null}.
         */
        Result get(long mills);
    }

    /**
     * Represents general information about invocation result.
     */
    public static final class Result {
        private final int mCode;
        private final Bundle mData;
        private final String mString;
        private final Exception mException;

        private Result(int code, Bundle data, String string, Exception exception) {
            mCode = code;
            mData = data;
            mString = string;
            mException = exception;
        }

        /**
         * @return {@code 0} when the command is completed successfully.
         */
        public int getCode() {
            return mCode;
        }

        /**
         * @return {@link Bundle} returned from the system server as a response.
         */
        public Bundle getData() {
            return mData;
        }

        /**
         * @return Console output from the system server.
         */
        public String getString() {
            return mString;
        }

        /**
         * @return Any {@link Exception} thrown during the invocation.
         */
        public Exception getException() {
            return mException;
        }
    }

    private static final class MyResultReceiver extends ResultReceiver {
        private final ParcelFileDescriptor mInputFileDescriptor;
        private final Object mLock = new Object();

        private Result mResult;

        private static Looper createBackgroundLooper() {
            final HandlerThread handlerThread = new HandlerThread("ShellCommandResultReceiver");
            handlerThread.start();
            return handlerThread.getLooper();
        }

        MyResultReceiver(ParcelFileDescriptor inputFileDescriptor) {
            super(new Handler(createBackgroundLooper()));
            mInputFileDescriptor = inputFileDescriptor;
        }

        @Override
        protected void onReceiveResult(int resultCode, Bundle resultData) {
            synchronized (mLock) {
                final StringJoiner joiner = new StringJoiner("\n");
                Exception resultException = null;
                // Note that system server is expected to be using Charset.defaultCharset() hence
                // we do not set the encoding here.
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                        new ParcelFileDescriptor.AutoCloseInputStream(mInputFileDescriptor)))) {
                    while (true) {
                        final String line = reader.readLine();
                        if (line == null) {
                            break;
                        }
                        joiner.add(line);
                    }
                } catch (IOException e) {
                    resultException = e;
                }

                mResult = new Result(resultCode, resultData, joiner.toString(), resultException);
                mLock.notifyAll();
                Looper.myLooper().quitSafely();
            }
        }

        Result getResult(long millis) {
            synchronized (mLock) {
                if (mResult == null) {
                    try {
                        mLock.wait(millis);
                    } catch (InterruptedException e) {
                    }
                }
                return mResult;
            }
        }
    }

    private static ParcelFileDescriptor createEmptyInput() throws IOException {
        final ParcelFileDescriptor[] pipeFds = ParcelFileDescriptor.createReliablePipe();
        pipeFds[1].close();
        return pipeFds[0];
    }

    private static FutureResult run(String serviceName, String[] args) throws Exception {
        final Class<?> serviceManagerClass = Class.forName("android.os.ServiceManager");
        final Method getService = serviceManagerClass.getMethod("getService", String.class);
        final IBinder service = (IBinder) getService.invoke(null, serviceName);

        final Class<?> shellCallbackClass = Class.forName("android.os.ShellCallback");
        final Method shellCommand = IBinder.class.getMethod("shellCommand",
                FileDescriptor.class, FileDescriptor.class, FileDescriptor.class,
                String[].class, shellCallbackClass, ResultReceiver.class);
        shellCommand.setAccessible(true);
        final ParcelFileDescriptor[] pipeFds = ParcelFileDescriptor.createReliablePipe();
        final MyResultReceiver resultReceiver = new MyResultReceiver(pipeFds[0]);
        try (ParcelFileDescriptor emptyInput = createEmptyInput();
             ParcelFileDescriptor out = pipeFds[1];
             ParcelFileDescriptor err = out.dup()) {
            shellCommand.invoke(service, emptyInput.getFileDescriptor(),
                    out.getFileDescriptor(), err.getFileDescriptor(), args, null, resultReceiver);
        }
        return resultReceiver::getResult;
    }

    /**
     * Synchronously invoke {@code IBinder#shellCommand()} then return the result.
     *
     * @param serviceName internal system service name, e.g.
     *                    {@link android.content.Context#INPUT_METHOD_SERVICE}.
     * @param args commands to be passedto the system service.
     * @param millis timeout in milliseconds.
     * @return Non-{@code null} object if the command is not timed out.  Otherwise {@code null}.
     */
    static Result runSync(String serviceName, String[] args, long millis) {
        try {
            return run(serviceName, args).get(millis);
        } catch (Exception e) {
            return new Result(-1, null, null, e);
        }
    }
}
