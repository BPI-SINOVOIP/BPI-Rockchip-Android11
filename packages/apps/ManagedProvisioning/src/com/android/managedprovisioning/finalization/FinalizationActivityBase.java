/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.finalization;

import android.app.Activity;
import android.os.Bundle;
import android.os.StrictMode;

/**
 * Instances of this base class manage interactions with a Device Policy Controller app after it has
 * been set up as either a Device Owner or a Profile Owner.
 *
 * Once we are sure that the DPC app has had an opportunity to run its own setup activities, we
 * record in the system that provisioning has been finalized.
 *
 * Different instances of this class will be tailored to run at different points in the Setup
 * Wizard user flows.
 */
public abstract class FinalizationActivityBase extends Activity {

    private static final String CONTROLLER_STATE_KEY = "controller_state";

    private FinalizationController mFinalizationController;

    @Override
    public final void onCreate(Bundle savedInstanceState) {
        // TODO(b/123987694): Investigate use of buffered i/o for provisioning params file
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                .permitUnbufferedIo()
                .build());

        super.onCreate(savedInstanceState);

        mFinalizationController = createFinalizationController();

        if (savedInstanceState != null) {
            final Bundle controllerState = savedInstanceState.getBundle(CONTROLLER_STATE_KEY);
            if (controllerState != null) {
                mFinalizationController.restoreInstanceState(controllerState);
            }

            // If savedInstanceState is not null, we already executed the logic below, so don't do
            // it again now.  We likely just need to wait for a child activity to complete, so we
            // can execute an onActivityResult() callback before finishing this activity.
            return;
        }

        mFinalizationController.provisioningFinalized();
        final int result = mFinalizationController.getProvisioningFinalizedResult();
        if (FinalizationController.PROVISIONING_FINALIZED_RESULT_CHILD_ACTIVITY_LAUNCHED ==
                result) {
            onFinalizationCompletedWithChildActivityLaunched();
        } else {
            if (FinalizationController.PROVISIONING_FINALIZED_RESULT_SKIPPED != result) {
                mFinalizationController.commitFinalizedState();
            }
            setResult(RESULT_OK);
            finish();
        }
    }

    @Override
    protected final void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        final Bundle controllerState = new Bundle();
        outState.putBundle(CONTROLLER_STATE_KEY, controllerState);
        mFinalizationController.saveInstanceState(controllerState);
    }

    @Override
    public final void onDestroy() {
        mFinalizationController.activityDestroyed(isFinishing());
        super.onDestroy();
    }

    /**
     * Return the finalization controller instance that was constructed in {@link #onCreate}.
     */
    protected FinalizationController getFinalizationController() {
        return mFinalizationController;
    }

    /**
     * Construct the correct subtype of FinalizationController.
     */
    protected abstract FinalizationController createFinalizationController();

    /**
     * Hook for code that should run when the controller has launched a child activity.
     */
    protected abstract void onFinalizationCompletedWithChildActivityLaunched();
}
