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
package android.contentsuggestions.cts;

import static androidx.test.InstrumentationRegistry.getContext;
import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.app.contentsuggestions.ClassificationsRequest;
import android.app.contentsuggestions.ContentSuggestionsManager;
import android.app.contentsuggestions.SelectionsRequest;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.RequiredServiceRule;

import com.google.common.collect.Lists;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link ContentSuggestionsManager}.
 */
@RunWith(AndroidJUnit4.class)
public class ContentSuggestionsManagerTest {
    private static final String TAG = ContentSuggestionsManagerTest.class.getSimpleName();

    private static final long VERIFY_TIMEOUT_MS = 5_000;
    private static final long SERVICE_LIFECYCLE_TIMEOUT_MS = 30_000;

    @Rule
    public final RequiredServiceRule mRequiredServiceRule =
            new RequiredServiceRule(Context.CONTENT_SUGGESTIONS_SERVICE);

    private ContentSuggestionsManager mManager;
    private CtsContentSuggestionsService.Watcher mWatcher;

    @Before
    public void setup() {
        mWatcher = CtsContentSuggestionsService.setWatcher();

        Log.d(TAG, "Test setting service");
        mManager = (ContentSuggestionsManager) getContext()
                .getSystemService(Context.CONTENT_SUGGESTIONS_SERVICE);
        setService(CtsContentSuggestionsService.SERVICE_COMPONENT.flattenToString());

        // TODO: b/126587631 remove when the manager calls no longer need this.
        getInstrumentation().getUiAutomation().adoptShellPermissionIdentity(
                "android.permission.READ_FRAME_BUFFER");

        // The ContentSuggestions services are created lazily, have to call one method on it to
        // start the service for the tests.
        mManager.notifyInteraction("SETUP", new Bundle());

        await(mWatcher.created, "Waiting for create");
        reset(mWatcher.verifier);
        Log.d(TAG, "Service set and watcher reset.");
    }

    @After
    public void tearDown() {
        Log.d(TAG, "Starting tear down, watcher is: " + mWatcher);
        resetService();
        await(mWatcher.destroyed, "Waiting for service destroyed");

        mWatcher = null;
        CtsContentSuggestionsService.clearWatcher();

        // TODO: b/126587631 remove when the manager calls no longer need this.
        getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
    }

    @Test
    public void managerForwards_notifyInteraction() {
        String requestId = "TEST";

        mManager.notifyInteraction(requestId, new Bundle());
        verifyService().onNotifyInteraction(eq(requestId), any());
    }

    @Test
    public void managerForwards_provideContextImage() {
        int taskId = 1;

        mManager.provideContextImage(taskId, new Bundle());
        verifyService().onProcessContextImage(eq(taskId), any(), any());
    }

    @Test
    public void managerForwards_suggestContentSelections() {
        SelectionsRequest request = new SelectionsRequest.Builder(1).build();
        ContentSuggestionsManager.SelectionsCallback callback = (statusCode, selections) -> {};


        mManager.suggestContentSelections(request, Executors.newSingleThreadExecutor(), callback);
        verifyService().onSuggestContentSelections(any(), any());
    }

    @Test
    public void managerForwards_classifyContentSelections() {
        ClassificationsRequest request = new ClassificationsRequest.Builder(
                Lists.newArrayList()).build();
        ContentSuggestionsManager.ClassificationsCallback callback =
                (statusCode, classifications) -> {};


        mManager.classifyContentSelections(request, Executors.newSingleThreadExecutor(), callback);
        verifyService().onClassifyContentSelections(any(), any());
    }

    private CtsContentSuggestionsService verifyService() {
        return verify(mWatcher.verifier, timeout(VERIFY_TIMEOUT_MS));
    }

    private void await(@NonNull CountDownLatch latch, @NonNull String message) {
        try {
            assertWithMessage(message).that(
                latch.await(SERVICE_LIFECYCLE_TIMEOUT_MS, TimeUnit.MILLISECONDS)).isTrue();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException("Interrupted while: " + message);
        }
    }

    /**
     * Sets the content capture service.
     */
    private static void setService(@NonNull String service) {
        Log.d(TAG, "Setting service to " + service);
        int userId = android.os.Process.myUserHandle().getIdentifier();
        runShellCommand(
                "cmd content_suggestions set temporary-service %d %s 120000", userId, service);
    }

    private static void resetService() {
        Log.d(TAG, "Resetting service");
        int userId = android.os.Process.myUserHandle().getIdentifier();
        runShellCommand("cmd content_suggestions set temporary-service %d", userId);
    }
}
