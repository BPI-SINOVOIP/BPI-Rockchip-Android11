/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.verifier.managedprovisioning;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * Handles com.android.cts.verifier.managedprovisioning.RECENTS intent sent
 * from main profile to work profile.
 *
 * {@link RecentsRedactionActivity} is required to stay in Recents to complete
 * "Recent Redaction Test". However a {@link Intent#FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS} will be
 * added by framework when intent {@link RecentsRedactionActivity#ACTION_RECENTS} is sent from
 * personal profile to work profile. By adding this {@link IntermediateRecentActivity} and
 * let it hanlde {@link RecentsRedactionActivity#ACTION_RECENTS} and then start
 * {@link IntermediateRecentActivity} in a new task, we can make sure no
 * {@link Intent#FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS} is added for starting
 * {@link IntermediateRecentActivity}.
 */
public class IntermediateRecentActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        startActivity(new Intent(this, RecentsRedactionActivity.class).addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK));
        finish();
    }
}
