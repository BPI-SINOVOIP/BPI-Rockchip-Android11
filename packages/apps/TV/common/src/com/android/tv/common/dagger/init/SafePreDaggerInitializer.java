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
package com.android.tv.common.dagger.init;

import android.content.Context;
import android.util.Log;

/**
 * Initializes objects one time only.
 *
 * <p>This is needed because ContentProviders can be created before Application.onCreate
 */
public final class SafePreDaggerInitializer {
    private interface Initialize {
        void init(Context context);
    }

    private static final String TAG = "SafePreDaggerInitializer";

    private static boolean initialized = false;
    private static Context oldContext;

    private static final Initialize[] sList =
            new Initialize[] {
                /* Begin_AOSP_Comment_Out
                com.google.android.libraries.phenotype.client.PhenotypeContext::setContext
                End_AOSP_Comment_Out */
            };

    public static synchronized void init(Context context) {
        if (!initialized) {
            for (Initialize i : sList) {
                i.init(context);
            }
            oldContext = context;
            initialized = true;
        } else if (oldContext != context) {
            Log.w(
                    TAG,
                    "init called more than once, skipping. Old context was "
                            + oldContext
                            + " new context is "
                            + context);
        }
    }

    private SafePreDaggerInitializer() {}
}
