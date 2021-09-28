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
package android.appwidget.cts.activity;

import android.app.Activity;
import android.app.SharedElementCallback;
import android.appwidget.cts.R;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

public class TransitionActivity extends Activity {

    public static final String BROADCAST_ACTION = TransitionActivity.class.getSimpleName();
    public static final String EXTRA_ELEMENTS = "elements";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_transition_end);

        setEnterSharedElementCallback(new SharedElementCallback() {
            @Override
            public void onSharedElementsArrived(List<String> sharedElementNames,
                    List<View> sharedElements, OnSharedElementsReadyListener listener) {
                super.onSharedElementsArrived(sharedElementNames, sharedElements, listener);
                sendBroadcast(new Intent(BROADCAST_ACTION).putStringArrayListExtra(
                        EXTRA_ELEMENTS, new ArrayList<>(sharedElementNames)));
            }
        });
    }
}
