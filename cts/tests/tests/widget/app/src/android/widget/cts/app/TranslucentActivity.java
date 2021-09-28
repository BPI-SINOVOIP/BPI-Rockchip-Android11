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

package android.widget.cts.app;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

public class TranslucentActivity extends AppCompatActivity {
    private static final String ACTION_TRANSLUCENT_ACTIVITY_RESUMED =
            "android.widget.cts.app.TRANSLUCENT_ACTIVITY_RESUMED";
    private static final String ACTION_TRANSLUCENT_ACTIVITY_FINISH =
            "android.widget.cts.app.TRANSLUCENT_ACTIVITY_FINISH";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (fragmentManager.findFragmentByTag("dialog") == null) {
            DialogFragment fragment = new SampleFragment();
            fragment.show(fragmentManager, "dialog");
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_TRANSLUCENT_ACTIVITY_FINISH);
        registerReceiver(mFinishActivityReceiver, filter);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Intent intent = new Intent(ACTION_TRANSLUCENT_ACTIVITY_RESUMED);
        intent.addFlags(Intent.FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS);
        sendBroadcast(intent);
    }

    @Override
    protected void onStop() {
        super.onStop();
        unregisterReceiver(mFinishActivityReceiver);
    }

    private final BroadcastReceiver mFinishActivityReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            finish();
        }
    };

    public static class SampleFragment extends DialogFragment {
        @NonNull
        @Override
        public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity())
                    .setTitle("Title")
                    .setMessage("Message")
                    .setOnDismissListener(dialog -> getActivity().finish())
                    .create();
        }
    }
}
