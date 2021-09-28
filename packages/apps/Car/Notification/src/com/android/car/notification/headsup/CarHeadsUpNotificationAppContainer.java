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

package com.android.car.notification.headsup;

import android.content.Context;
import android.graphics.PixelFormat;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import com.android.car.notification.R;

public class CarHeadsUpNotificationAppContainer implements CarHeadsUpNotificationContainer {
    // view that contains scrim and notification content
    protected final View mHeadsUpPanel;
    // framelayout that notification content should be added to.
    protected final FrameLayout mHeadsUpContentFrame;

    protected final Context mContext;

    public CarHeadsUpNotificationAppContainer(Context context) {
        mContext = context;
        mHeadsUpPanel = createHeadsUpPanel();
        mHeadsUpContentFrame = mHeadsUpPanel.findViewById(R.id.headsup_content);
        addHeadsUpPanelToDisplay();
    }

    @Override
    public void displayNotification(View notificationView) {
        if (!isVisible()) {
            mHeadsUpPanel.setVisibility(View.VISIBLE);
        }
        mHeadsUpContentFrame.addView(notificationView);
    }

    @Override
    public void removeNotification(View notificationView) {
        mHeadsUpContentFrame.removeView(notificationView);
        if (mHeadsUpContentFrame.getChildCount() == 0) {
            mHeadsUpPanel.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public boolean isVisible() {
        return mHeadsUpPanel.getVisibility() == View.VISIBLE;
    }

    /**
     * Construct and return the heads up panel.
     *
     * @return view that contains R.id.headsup_content
     */
    private View createHeadsUpPanel() {
        return LayoutInflater.from(mContext).inflate(R.layout.headsup_container, null);
    }

    /**
     * Attach the heads up panel to the display
     */
    private void addHeadsUpPanelToDisplay() {
        WindowManager.LayoutParams wrapperParams = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT,
                // This type allows covering status bar and receiving touch input
                WindowManager.LayoutParams.TYPE_SYSTEM_ERROR,
                WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);
        wrapperParams.gravity = Gravity.TOP;
        mHeadsUpPanel.setVisibility(View.INVISIBLE);
        WindowManager windowManager = (WindowManager) mContext.getSystemService(
                Context.WINDOW_SERVICE);
        windowManager.addView(mHeadsUpPanel, wrapperParams);
    }
}
