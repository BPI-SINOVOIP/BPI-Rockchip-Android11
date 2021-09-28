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

package android.server.wm.app;

import static android.server.wm.app.Components.UnresponsiveActivity.EXTRA_ON_MOTIONEVENT_DELAY_MS;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;
import android.view.SurfaceControlViewHost;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;

public class RenderService extends Service {
    static final String EXTRAS_BUNDLE = "INTENT_BUNDLE";
    static final String EXTRAS_DISPLAY_ID = "hostDisplayId";
    static final String EXTRAS_HOST_TOKEN = "hostInputToken";
    static final String BROADCAST_EMBED_CONTENT
            = "android.server.wm.app.RenderService.EMBED_CONTENT";
    static final String EXTRAS_SURFACE_PACKAGE = "surfacePackage";

    private int mOnMotionEventDelayMs;

    private boolean onTouch(View v, MotionEvent event) {
        SystemClock.sleep(mOnMotionEventDelayMs);
        // Don't consume the event.
        return false;
    }

    @Override
    public IBinder onBind(Intent intent) {
        Bundle b = intent.getBundleExtra(EXTRAS_BUNDLE);
        IBinder hostToken = b.getBinder(EXTRAS_HOST_TOKEN);
        int hostDisplayId = b.getInt(EXTRAS_DISPLAY_ID);
        mOnMotionEventDelayMs = b.getInt(EXTRA_ON_MOTIONEVENT_DELAY_MS);

        SurfaceControlViewHost surfaceControlViewHost = getSurfaceControlViewHost(hostToken,
                hostDisplayId);
        sendSurfacePackage(surfaceControlViewHost.getSurfacePackage());
        return null;
    }

    private SurfaceControlViewHost getSurfaceControlViewHost(IBinder hostToken, int hostDisplayId) {
        final Context displayContext = getDisplayContext(hostDisplayId);
        SurfaceControlViewHost surfaceControlViewHost =
                new SurfaceControlViewHost(displayContext, displayContext.getDisplay(), hostToken);

        View embeddedView = new Button(this);
        embeddedView.setOnTouchListener(this::onTouch);
        DisplayMetrics metrics = new DisplayMetrics();
        displayContext.getDisplay().getMetrics(metrics);
        surfaceControlViewHost.setView(embeddedView, metrics.widthPixels, metrics.heightPixels);
        return surfaceControlViewHost;
    }

    private Context getDisplayContext(int hostDisplayId) {
        final DisplayManager displayManager = getSystemService(DisplayManager.class);
        final Display targetDisplay = displayManager.getDisplay(hostDisplayId);
        return createDisplayContext(targetDisplay);
    }

    private void sendSurfacePackage(SurfaceControlViewHost.SurfacePackage surfacePackage) {
        Intent broadcast = new Intent();
        broadcast.setAction(BROADCAST_EMBED_CONTENT);
        broadcast.putExtra(EXTRAS_SURFACE_PACKAGE, surfacePackage);
        sendBroadcast(broadcast);
    }
}
