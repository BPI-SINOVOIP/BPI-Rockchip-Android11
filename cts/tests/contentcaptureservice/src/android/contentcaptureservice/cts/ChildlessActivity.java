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
package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Assertions.assertNoViewLevelEvents;

import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.os.Bundle;

import androidx.annotation.NonNull;

public class ChildlessActivity extends AbstractRootViewActivity {

    @Override
    protected void setContentViewOnCreate(Bundle savedInstanceState) {
        setContentView(R.layout.childless_activity);
    }

    @Override
    public void assertDefaultEvents(@NonNull Session session) {
        // Should be empty because the root view is not important for content capture without a
        // child that is important.
        assertNoViewLevelEvents(session, this);
    }
}
