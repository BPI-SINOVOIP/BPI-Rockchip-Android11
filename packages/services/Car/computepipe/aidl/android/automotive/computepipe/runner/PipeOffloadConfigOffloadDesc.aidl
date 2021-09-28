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
package android.automotive.computepipe.runner;

import android.automotive.computepipe.runner.PipeOffloadConfigOffloadType;

/**
 * structure that describes the combination of offload options.
 * This is a per graph specific combination.
 */
@VintfStability
parcelable PipeOffloadConfigOffloadDesc {
    /**
     * combination of different offload engines
     */
    PipeOffloadConfigOffloadType[] type;
    /**
     * 1:1 correspondence for each type above.
     * Every offload engine has a flag describing if its virtual device
     */
    boolean[] isVirtual;
}

