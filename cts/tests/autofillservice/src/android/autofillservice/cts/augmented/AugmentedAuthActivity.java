/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.autofillservice.cts.augmented;

import android.app.PendingIntent;
import android.autofillservice.cts.AbstractAutoFillActivity;
import android.autofillservice.cts.R;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.service.autofill.Dataset;
import android.util.Log;
import android.view.autofill.AutofillManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Activity for testing Augmented Autofill authentication flow. This activity shows a simple UI;
 * when the UI is tapped, it returns whatever data was configured via the auth intent.
 */
public class AugmentedAuthActivity extends AbstractAutoFillActivity {
    private static final String TAG = "AugmentedAuthActivity";

    public static final String ID_AUTH_ACTIVITY_BUTTON = "button";

    private static final String EXTRA_DATASET_TO_RETURN = "dataset_to_return";
    private static final String EXTRA_CLIENT_STATE_TO_RETURN = "client_state_to_return";
    private static final String EXTRA_RESULT_CODE_TO_RETURN = "result_code_to_return";

    private static final List<PendingIntent> sPendingIntents = new ArrayList<>(1);

    public static void resetStaticState() {
        for (PendingIntent pendingIntent : sPendingIntents) {
            pendingIntent.cancel();
        }
        sPendingIntents.clear();
    }

    public static IntentSender createSender(Context context, int requestCode,
            Dataset datasetToReturn, Bundle clientStateToReturn, int resultCodeToReturn) {
        Intent intent = new Intent(context, AugmentedAuthActivity.class);
        intent.putExtra(EXTRA_DATASET_TO_RETURN, datasetToReturn);
        intent.putExtra(EXTRA_CLIENT_STATE_TO_RETURN, clientStateToReturn);
        intent.putExtra(EXTRA_RESULT_CODE_TO_RETURN, resultCodeToReturn);
        PendingIntent pendingIntent = PendingIntent.getActivity(context, requestCode, intent, 0);
        sPendingIntents.add(pendingIntent);
        return pendingIntent.getIntentSender();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d(TAG, "Auth activity invoked, showing auth UI");
        setContentView(R.layout.single_button_activity);
        findViewById(R.id.button).setOnClickListener((v) -> {
            Log.d(TAG, "Auth UI tapped, returning result");

            Intent intent = getIntent();
            Dataset dataset = intent.getParcelableExtra(EXTRA_DATASET_TO_RETURN);
            Bundle clientState = intent.getParcelableExtra(EXTRA_CLIENT_STATE_TO_RETURN);
            int resultCode = intent.getIntExtra(EXTRA_RESULT_CODE_TO_RETURN, RESULT_OK);
            Log.d(TAG, "Output: dataset=" + dataset + ", clientState=" + clientState
                    + ", resultCode=" + resultCode);

            Intent result = new Intent();
            result.putExtra(AutofillManager.EXTRA_AUTHENTICATION_RESULT, dataset);
            result.putExtra(AutofillManager.EXTRA_CLIENT_STATE, clientState);
            setResult(resultCode, result);

            finish();
        });
    }
}
