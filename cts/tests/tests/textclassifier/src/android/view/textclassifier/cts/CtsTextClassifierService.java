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
package android.view.textclassifier.cts;

import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.Intent;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.CancellationSignal;
import android.service.textclassifier.TextClassifierService;
import android.util.ArrayMap;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationSessionId;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextLanguage;
import android.view.textclassifier.TextLinks;
import android.view.textclassifier.TextSelection;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link TextClassifierService} used in the tests.
 */
public final class CtsTextClassifierService extends TextClassifierService {

    private static final String TAG = "CtsTextClassifierService";
    public static final String MY_PACKAGE = "android.view.textclassifier.cts";

    private static final Icon ICON_RES =
            Icon.createWithResource("android", android.R.drawable.btn_star);
    static final Icon ICON_URI =
            Icon.createWithContentUri(new Uri.Builder()
                  .scheme("content")
                  .authority("com.android.textclassifier.icons")
                  .path("android")
                  .appendPath("" + android.R.drawable.btn_star)
                  .build());

    private final Map<String, List<TextClassificationSessionId>> mRequestSessions =
            new ArrayMap<>();
    private final CountDownLatch mRequestLatch = new CountDownLatch(1);

    /**
     * Returns the TestWatcher that was used for the testing.
     */
    @NonNull
    public static TextClassifierTestWatcher getTestWatcher() {
        return new TextClassifierTestWatcher();
    }

    /**
     * Returns all incoming session IDs, keyed by API name.
     */
    @NonNull
    Map<String, List<TextClassificationSessionId>> getRequestSessions() {
        return Collections.unmodifiableMap(mRequestSessions);
    }

    void awaitQuery(long timeoutMillis) {
        try {
            mRequestLatch.await(timeoutMillis, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            // do nothing
        }
    }

    @Override
    public void onSuggestSelection(TextClassificationSessionId sessionId,
            TextSelection.Request request, CancellationSignal cancellationSignal,
            Callback<TextSelection> callback) {
        handleRequest(sessionId, "onSuggestSelection");
        callback.onSuccess(TextClassifier.NO_OP.suggestSelection(request));
    }

    @Override
    public void onClassifyText(TextClassificationSessionId sessionId,
            TextClassification.Request request, CancellationSignal cancellationSignal,
            Callback<TextClassification> callback) {
        handleRequest(sessionId, "onClassifyText");
        final TextClassification classification = new TextClassification.Builder()
                .addAction(new RemoteAction(
                        ICON_RES,
                        "Test Action",
                        "Test Action",
                        PendingIntent.getActivity(this, 0, new Intent(), 0)))
                .build();
        callback.onSuccess(classification);
    }

    @Override
    public void onGenerateLinks(TextClassificationSessionId sessionId, TextLinks.Request request,
            CancellationSignal cancellationSignal, Callback<TextLinks> callback) {
        handleRequest(sessionId, "onGenerateLinks");
        callback.onSuccess(TextClassifier.NO_OP.generateLinks(request));
    }

    @Override
    public void onDetectLanguage(TextClassificationSessionId sessionId,
            TextLanguage.Request request, CancellationSignal cancellationSignal,
            Callback<TextLanguage> callback) {
        handleRequest(sessionId, "onDetectLanguage");
        callback.onSuccess(TextClassifier.NO_OP.detectLanguage(request));
    }

    @Override
    public void onSuggestConversationActions(TextClassificationSessionId sessionId,
            ConversationActions.Request request, CancellationSignal cancellationSignal,
            Callback<ConversationActions> callback) {
        handleRequest(sessionId, "onSuggestConversationActions");
        callback.onSuccess(TextClassifier.NO_OP.suggestConversationActions(request));
    }

    @Override
    public void onSelectionEvent(TextClassificationSessionId sessionId, SelectionEvent event) {
        handleRequest(sessionId, "onSelectionEvent");
    }

    @Override
    public void onTextClassifierEvent(TextClassificationSessionId sessionId,
            TextClassifierEvent event) {
        handleRequest(sessionId, "onTextClassifierEvent");
    }

    @Override
    public void onCreateTextClassificationSession(TextClassificationContext context,
            TextClassificationSessionId sessionId) {
        handleRequest(sessionId, "onCreateTextClassificationSession");
    }

    @Override
    public void onDestroyTextClassificationSession(TextClassificationSessionId sessionId) {
        handleRequest(sessionId, "onDestroyTextClassificationSession");
    }

    private void handleRequest(TextClassificationSessionId sessionId, String methodName) {
        mRequestSessions.computeIfAbsent(methodName, key -> new ArrayList<>()).add(sessionId);
        mRequestLatch.countDown();
    }

    @Override
    public void onConnected() {
        TextClassifierTestWatcher.ServiceWatcher.onConnected(/* service */ this);
    }

    @Override
    public void onDisconnected() {
        TextClassifierTestWatcher.ServiceWatcher.onDisconnected();
    }
}
