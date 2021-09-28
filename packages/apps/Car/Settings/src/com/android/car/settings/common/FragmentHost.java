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

package com.android.car.settings.common;

import androidx.fragment.app.Fragment;

/**
 * Implemented by Settings {@link android.app.Activity} instances to host Settings {@link Fragment}
 * instances.
 */
public interface FragmentHost {
    /**
     * Launches a Fragment in the main container of the current Activity.
     */
    void launchFragment(Fragment fragment);

    /**
     * Pops the top off the Fragment stack.
     */
    void goBack();

    /**
     * Shows a message to inform the user that the current feature is not available when driving.
     */
    void showBlockingMessage();
}
