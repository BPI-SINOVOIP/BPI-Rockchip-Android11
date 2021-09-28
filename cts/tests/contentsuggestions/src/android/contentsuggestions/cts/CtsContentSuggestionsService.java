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
 * limitations under the License
 */

package android.contentsuggestions.cts;

import android.app.contentsuggestions.ClassificationsRequest;
import android.app.contentsuggestions.ContentSuggestionsManager.ClassificationsCallback;
import android.app.contentsuggestions.ContentSuggestionsManager.SelectionsCallback;
import android.app.contentsuggestions.SelectionsRequest;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.service.contentsuggestions.ContentSuggestionsService;
import android.util.Log;

import org.mockito.Mockito;

import java.util.concurrent.CountDownLatch;

public class CtsContentSuggestionsService extends ContentSuggestionsService {

    private static final String TAG = CtsContentSuggestionsService.class.getSimpleName();
    private static final boolean DEBUG = false;

    private static Watcher sWatcher;

    public static final ComponentName SERVICE_COMPONENT = new ComponentName(
            "android.contentsuggestions.cts", CtsContentSuggestionsService.class.getName());

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "onCreate: ");
        if (sWatcher.verifier != null) {
            Log.e(TAG, "onCreate, trying to set verifier when it already exists");
        }
        sWatcher.verifier = Mockito.mock(CtsContentSuggestionsService.class);
        sWatcher.created.countDown();
    }

    @Override
    public void onDestroy() {
        if (DEBUG) Log.d(TAG, "onDestroy");
        super.onDestroy();
        sWatcher.destroyed.countDown();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        if (DEBUG) Log.d(TAG, "unbind");
        return super.onUnbind(intent);
    }

    @Override
    public void onProcessContextImage(int taskId, Bitmap contextImage, Bundle extras) {
        if (DEBUG) {
            Log.d(TAG,
                    "onProcessContextImage() called with: taskId = [" + taskId
                            + "], contextImage = ["
                            + contextImage + "], extras = [" + extras + "]");
        }
        sWatcher.verifier.onProcessContextImage(taskId, contextImage, extras);
    }

    @Override
    public void onSuggestContentSelections(SelectionsRequest request, SelectionsCallback callback) {
        if (DEBUG) {
            Log.d(TAG,
                    "onSuggestContentSelections() called with: request = [" + request
                            + "], callback = ["
                            + callback + "]");
        }
        sWatcher.verifier.onSuggestContentSelections(request, callback);
    }

    @Override
    public void onClassifyContentSelections(ClassificationsRequest request,
            ClassificationsCallback callback) {
        if (DEBUG) {
            Log.d(TAG,
                    "onClassifyContentSelections() called with: request = [" + request
                            + "], callback = ["
                            + callback + "]");
        }
        sWatcher.verifier.onClassifyContentSelections(request, callback);
    }

    @Override
    public void onNotifyInteraction(String requestId, Bundle interaction) {
        if (DEBUG) {
            Log.d(TAG,
                    "onNotifyInteraction() called with: requestId = [" + requestId
                            + "], interaction = ["
                            + interaction + "]");
        }
        sWatcher.verifier.onNotifyInteraction(requestId, interaction);
    }

    public static Watcher setWatcher() {
        if (sWatcher != null) {
            throw new IllegalStateException("Set watcher with watcher already set");
        }
        sWatcher = new Watcher();
        return sWatcher;
    }

    public static void clearWatcher() {
        sWatcher = null;
    }

    public static final class Watcher {
        public CountDownLatch created = new CountDownLatch(1);
        public CountDownLatch destroyed = new CountDownLatch(1);

        /**
         * Can be used to verify that API specific service methods are called. Not a real mock as
         * the system isn't talking to this directly, it has calls proxied to it.
         */
        public CtsContentSuggestionsService verifier;
    }
}
