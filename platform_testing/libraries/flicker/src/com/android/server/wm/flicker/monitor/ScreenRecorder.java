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

package com.android.server.wm.flicker.monitor;

import static com.android.compatibility.common.util.SystemUtil.runShellCommand;

import static java.nio.file.StandardCopyOption.REPLACE_EXISTING;

import android.util.Log;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;

/** Captures screen contents and saves it as a mp4 video file. */
public class ScreenRecorder implements ITransitionMonitor {
    private static final String TAG = "FLICKER";
    private int mWidth;
    private int mHeight;
    private Path mOutputPath;
    private Thread mRecorderThread;

    public ScreenRecorder() {
        this(720, 1280, OUTPUT_DIR.resolve("transition.mp4"));
    }

    public ScreenRecorder(int width, int height, Path outputPath) {
        mWidth = width;
        mHeight = height;
        mOutputPath = outputPath;
    }

    public Path getPath() {
        return mOutputPath;
    }

    @Override
    public void start() {
        mOutputPath.getParent().toFile().mkdirs();
        String command =
                String.format(
                        Locale.getDefault(),
                        "screenrecord --size %dx%d %s",
                        mWidth,
                        mHeight,
                        mOutputPath);
        mRecorderThread =
                new Thread(
                        () -> {
                            try {
                                Runtime.getRuntime().exec(command);
                            } catch (IOException e) {
                                Log.e(TAG, "Error executing " + command, e);
                            }
                        });
        mRecorderThread.start();
    }

    @Override
    public void stop() {
        runShellCommand("killall -s 2 screenrecord");
        try {
            mRecorderThread.join();
        } catch (InterruptedException e) {
            // ignore
        }
    }

    @Override
    public Path save(String testTag) {
        if (!Files.exists(mOutputPath)) {
            Log.w(TAG, "No video file found on " + mOutputPath);
            return null;
        }

        try {
            Path targetPath =
                    Files.move(mOutputPath, OUTPUT_DIR.resolve(testTag + ".mp4"), REPLACE_EXISTING);
            Log.i(TAG, "Video saved to " + targetPath.toString());
            return targetPath;
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
