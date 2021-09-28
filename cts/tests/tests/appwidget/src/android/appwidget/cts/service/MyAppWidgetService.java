/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.appwidget.cts.service;

import android.content.Intent;
import android.widget.RemoteViews;
import android.widget.RemoteViewsService;

public class MyAppWidgetService extends RemoteViewsService {
    private static final Object sLock = new Object();

    private static RemoteViewsFactory sFactory;

    public static void setFactory(RemoteViewsFactory factory) {
        synchronized (sLock) {
            sFactory = factory;
        }
    }

    @Override
    public RemoteViewsFactory onGetViewFactory(Intent intent) {
        synchronized (sLock) {
            return sFactory == null ? new DefaultFactory() : sFactory;
        }
    }

    private static class DefaultFactory implements RemoteViewsFactory {
        @Override
        public void onCreate() { }

        @Override
        public void onDataSetChanged() { }

        @Override
        public void onDestroy() { }

        @Override
        public int getCount() {
            return 0;
        }

        @Override
        public RemoteViews getViewAt(int i) {
            return null;
        }

        @Override
        public RemoteViews getLoadingView() {
            return null;
        }

        @Override
        public int getViewTypeCount() {
            return 1;
        }

        @Override
        public long getItemId(int i) {
            return i;
        }

        @Override
        public boolean hasStableIds() {
            return false;
        }
    }
}
