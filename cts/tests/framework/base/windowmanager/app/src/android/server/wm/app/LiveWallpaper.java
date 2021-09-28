/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.server.wm.app.Components.TestLiveWallpaperKeys.COMPONENT;
import static android.server.wm.app.Components.TestLiveWallpaperKeys.ENGINE_DISPLAY_ID;
import static android.view.Display.DEFAULT_DISPLAY;

import android.content.Context;
import android.server.wm.TestJournalProvider;
import android.service.wallpaper.WallpaperService;
import android.view.SurfaceHolder;

public class LiveWallpaper extends WallpaperService {

    @Override
    public Engine onCreateEngine() {
        return new Engine(){
            @Override
            public void onCreate(SurfaceHolder surfaceHolder) {
                super.onCreate(surfaceHolder);
                final Context baseContext = getBaseContext();
                final Context displayContext = getDisplayContext();
                final int displayId = displayContext.getDisplayId();

                // Only need to verify that when it is the secondary display.
                if (displayId != DEFAULT_DISPLAY) {
                    final String CURRENT_ENGINE_DISPLAY_ID = ENGINE_DISPLAY_ID + displayId;
                    TestJournalProvider.putExtras(baseContext, COMPONENT, bundle -> {
                        bundle.putBoolean(CURRENT_ENGINE_DISPLAY_ID, true);
                    });
                }
            }
        };
    }
}
