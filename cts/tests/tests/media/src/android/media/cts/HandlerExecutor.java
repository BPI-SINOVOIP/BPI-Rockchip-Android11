/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import android.annotation.NonNull;
import android.os.Handler;
import android.os.Looper;

import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

public class HandlerExecutor extends Handler implements Executor {
    public HandlerExecutor(@NonNull Looper looper) {
        super(looper);
    }

    @Override
    public void execute(Runnable command) {
        if (!post(command)) {
            throw new RejectedExecutionException(this + " is shutting down");
        }
    }
}
