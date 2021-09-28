/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * See the License for the specific language governing permissions andf
 * limitations under the License.
 */

package android.permission2.cts;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import javax.annotation.Nullable;

public class CommandBroadcastReceiver extends BroadcastReceiver {

    private static @Nullable OnCommandResultListener sOnCommandResultListener;

    public interface OnCommandResultListener {
        void onCommandResult(@Nullable Intent result);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final OnCommandResultListener listener;
        synchronized (CommandBroadcastReceiver.class) {
            listener = sOnCommandResultListener;
        }
        if (listener != null) {
            listener.onCommandResult(intent);
        }
    }

    public static void setOnCommandResultListener(@Nullable OnCommandResultListener listener) {
        synchronized (CommandBroadcastReceiver.class) {
            sOnCommandResultListener = listener;
        }
    }
}
