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

package android.net.wifi.mts;

import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * Reads from the given BufferedReader and saves the output to a string.
 */
public class StreamReader extends Thread {
    private static final String TAG = "ConnMetrics:" + StreamReader.class.getSimpleName();
    private BufferedReader mSource;
    private String mOutput;

    public StreamReader(BufferedReader source) {
        mSource = source;
    }

    public String getOutput() {
        if (mOutput == null) {
            throw new IllegalStateException(
                    "Should call start and join before trying to get output");
        }
        return mOutput;
    }

    @Override
    public void run() {
        StringBuilder sb = new StringBuilder();
        String s;
        try {
            while ((s = mSource.readLine()) != null) {
                sb.append(s);
                sb.append("\n");
            }
        } catch (IOException e) {
            Log.e(TAG, "Error reading input", e);
        } finally {
            try {
                mSource.close();
            } catch (IOException e) {
                Log.e(TAG, "Error closing reader", e);
            }
        }
        mOutput = sb.toString();
    }

    /**
     * Runs the specified process command and returns the results of the input and error streams.
     *
     * @return The output of the input stream, or null if there was an exception.
     */
    public static String runProcessCommand(String command) {
        try {
            Process process = Runtime.getRuntime().exec(command);

            BufferedReader stdInput = new BufferedReader(
                    new InputStreamReader(process.getInputStream()));
            BufferedReader stdError = new BufferedReader(
                    new InputStreamReader(process.getErrorStream()));
            StreamReader inputReader = new StreamReader(stdInput);
            StreamReader errorReader = new StreamReader(stdError);

            inputReader.start();
            errorReader.start();

            process.waitFor();
            try {
                inputReader.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                Log.e(TAG, "Error joining thread", e);
            }
            try {
                errorReader.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                Log.e(TAG, "Error joining thread", e);
            }

            // Some commands will return some valid data while also printing a message in
            // the error stream, so returning result of the input stream regardless of if
            // there's a message in the error stream.
            if (errorReader.getOutput().length() > 0) {
                Log.e(TAG, errorReader.getOutput());
            }
            return inputReader.getOutput();
        } catch (IOException e) {
            Log.e(TAG, "Error running command", e);
            return null;
        } catch (InterruptedException e) {
            Log.e(TAG, "Process was interrupted", e);
            Thread.currentThread().interrupt();
            return null;
        }
    }
}
