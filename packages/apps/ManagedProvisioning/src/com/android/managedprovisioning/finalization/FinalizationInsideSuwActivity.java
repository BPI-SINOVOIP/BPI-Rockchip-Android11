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

import static com.google.android.setupcompat.util.ResultCodes.RESULT_SKIP;

import android.content.Intent;

import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.ProvisionLogger;
import com.android.managedprovisioning.common.SettingsFacade;

/**
 * This class is used to start the Device Policy Controller app while the Setup Wizard is still
 * running.  It listens for a result from any child activity that is started, and returns its own
 * result code when all child activities have completed.
 */
public class FinalizationInsideSuwActivity extends FinalizationActivityBase {

    private static final int DPC_SETUP_REQUEST_CODE = 1;
    private static final int FINAL_SCREEN_REQUEST_CODE = 2;

    @Override
    protected FinalizationController createFinalizationController() {
        return new FinalizationController(this, new FinalizationInsideSuwControllerLogic(this));
    }

    @Override
    protected void onFinalizationCompletedWithChildActivityLaunched() {
        // Don't commit finalized state until child activity completes.
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            // We use FINAL_SCREEN_REQUEST_CODE in the admin-integrated flow, because the DPC has
            // already run earlier, so we don't run it again during finalization.  Instead, we show
            // a generic UI indicating that provisioning is done.  In other flows, we start up the
            // DPC during finalization, using DPC_SETUP_REQUEST_CODE.
            case DPC_SETUP_REQUEST_CODE:
                logDpcSetupCompleted(resultCode);
                // Fall through; code below this applies to both request codes
            case FINAL_SCREEN_REQUEST_CODE:
                ProvisionLogger.logi("onActivityResult: received "
                        + "requestCode = " + requestCode + ", resultCode = " + resultCode);

                // Inside of SUW, if this activity returns RESULT_CANCELED, we will go back to
                // the previous SUW action.  Don't finalize the user provisioning state in that
                // case, so that we run finalization again when the user goes forward again in SUW.
                //
                // In this Android version, the DPC can also request finalization to be postponed
                // until after SUW, by returning RESULT_SKIP.  This special handling of RESULT_SKIP
                // will be removed in a future version, at which point the only option for the DPC
                // will be to run during SUW.  Note that RESULT_SKIP is not relevant in the case of
                // FINAL_SCREEN_REQUEST_CODE, so it should not be used in that case.
                if (resultCode != RESULT_CANCELED && resultCode != RESULT_SKIP) {
                    getFinalizationController().commitFinalizedState();
                }
                setResult(resultCode);
                finish();
                break;
            default:
                ProvisionLogger.logw("onActivityResult: Unknown request code: " + requestCode);
                break;
        }
    }

    private void logDpcSetupCompleted(int resultCode) {
        final ProvisioningAnalyticsTracker provisioningAnalyticsTracker =
                new ProvisioningAnalyticsTracker(
                        MetricsWriterFactory.getMetricsWriter(this, new SettingsFacade()),
                        new ManagedProvisioningSharedPreferences(this));
        provisioningAnalyticsTracker.logDpcSetupCompleted(this, resultCode);
    }
}
