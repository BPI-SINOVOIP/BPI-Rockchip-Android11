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
package android.apppredictionservice.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.app.prediction.AppPredictionSessionId;
import android.app.prediction.AppTargetEvent;
import android.app.prediction.AppTargetId;
import android.os.CancellationSignal;
import android.app.prediction.AppPredictionContext;
import android.service.appprediction.AppPredictionService;
import android.app.prediction.AppTarget;

import java.util.List;
import java.util.function.Consumer;

public class PredictionService extends AppPredictionService {

    private static final String TAG = PredictionService.class.getSimpleName();

    public static final String MY_PACKAGE = "android.apppredictionservice.cts";
    public static final String SERVICE_NAME = MY_PACKAGE + "/."
            + PredictionService.class.getSimpleName();

    public static final String EXTRA_REPORTER = "extra_reporter";

    private ServiceReporter mReporter;

    @Override
    public void onCreatePredictionSession(AppPredictionContext context,
            AppPredictionSessionId sessionId) {
        mReporter = (ServiceReporter) context.getExtras().getBinder(EXTRA_REPORTER);
        mReporter.onCreatePredictionSession(context, sessionId);
    }

    @Override
    public void onAppTargetEvent(AppPredictionSessionId sessionId, AppTargetEvent event) {
        mReporter.onAppTargetEvent(sessionId, event);
    }

    @Override
    public void onLaunchLocationShown(AppPredictionSessionId sessionId, String launchLocation,
            List<AppTargetId> targetIds) {
        mReporter.onLocationShown(sessionId, launchLocation, targetIds);
    }

    @Override
    public void onSortAppTargets(AppPredictionSessionId sessionId, List<AppTarget> targets,
            CancellationSignal cancellationSignal, Consumer<List<AppTarget>> callback) {
        mReporter.onSortAppTargets(sessionId, targets, callback);
        callback.accept(mReporter.getSortedPredictionsProvider().sortTargets(targets));
    }

    @Override
    public void onRequestPredictionUpdate(AppPredictionSessionId sessionId) {
        mReporter.onRequestPredictionUpdate(sessionId);
        updatePredictions(sessionId, mReporter.getPredictionsProvider().getTargets(sessionId));
    }

    @Override
    public void onDestroyPredictionSession(AppPredictionSessionId sessionId) {
        mReporter.onDestroyPredictionSession(sessionId);
    }

    @Override
    public void onStartPredictionUpdates() {
        mReporter.onStartPredictionUpdates();
    }

    @Override
    public void onStopPredictionUpdates() {
        mReporter.onStopPredictionUpdates();
    }
}
