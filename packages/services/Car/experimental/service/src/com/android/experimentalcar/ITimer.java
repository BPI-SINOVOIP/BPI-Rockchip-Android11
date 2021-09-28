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
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.experimentalcar;

import java.util.TimerTask;

/**
 * Interface to abstract dependency on {@link java.util.Timer}.
 */
public interface ITimer {

    /**
     * Cancel the timer and prepare for new events.
     */
    void reset();

    /**
     * See {@link java.util.Timer#schedule(TimerTask, long)}.
     */
    void schedule(TimerTask task, long delay);

    /**
     * See {@link java.util.Timer#cancel()}.
     */
    void cancel();
}
