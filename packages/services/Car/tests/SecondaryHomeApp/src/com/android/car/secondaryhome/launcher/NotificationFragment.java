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

package com.android.car.secondaryhome.launcher;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.fragment.app.Fragment;

import com.android.car.notification.PreprocessingManager;
import com.android.car.secondaryhome.R;

/**
 * {@link Fragment} that shows list of notifications.
 */
public final class NotificationFragment extends Fragment {

    private static final String TAG = "SecHome.NotificationFragment";

    private NotificationListener mNotificationListener;
    private PreprocessingManager mPreprocessingManager;
    private NotificationViewController mNotificationViewController;
    private Context mContext;
    private View mNotificationView;
    private Button mClearAllButton;

    private final ServiceConnection mNotificationListenerConnection = new ServiceConnection() {
        public void onServiceConnected(ComponentName className, IBinder binder) {
            Log.d(TAG, "onServiceConnected");
            mNotificationListener = ((NotificationListener.LocalBinder) binder).getService();
            mNotificationListener.registerAsSystemService(mContext);
            mNotificationViewController =
                    new NotificationViewController(mNotificationView,
                            mPreprocessingManager,
                            mNotificationListener);
            mNotificationViewController.enable();
        }

        public void onServiceDisconnected(ComponentName className) {
            Log.d(TAG, "onServiceDisconnected");
            mNotificationViewController.disable();
            mNotificationViewController = null;
            mNotificationListener = null;
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mContext = getActivity().getApplicationContext();
        mPreprocessingManager = PreprocessingManager.getInstance(mContext);
        Intent intent = new Intent(mContext, NotificationListener.class);
        intent.setAction(NotificationListener.ACTION_LOCAL_BINDING);

        mContext.bindService(intent, mNotificationListenerConnection,
                Context.BIND_AUTO_CREATE);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.notification_fragment, container, false);
        mNotificationView = view.findViewById(R.id.notification_fragment);
        mClearAllButton = view.findViewById(R.id.clear_all_button);
        mClearAllButton.setOnClickListener(v -> mNotificationViewController.resetNotifications());

        return view;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Unbind notification listener
        mNotificationViewController.disable();
        mNotificationViewController = null;
        mNotificationListener = null;
        mContext.unbindService(mNotificationListenerConnection);
    }

    @Override
    public void onResume() {
        super.onResume();
        Log.d(TAG, "Resuming notification fragment");
    }

    @Override
    public void onPause() {
        super.onPause();
        Log.d(TAG, "Pausing notification fragment");
    }
}
