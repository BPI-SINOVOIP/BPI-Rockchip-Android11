/*
 * Copyright 2021 The Android Open Source Project
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

package com.android.car.rotary;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A helper class to support apps using {@link android.view.SurfaceView} for off-process rendering.
 * <p>
 * There are two kinds of apps involved in the off-process rendering process: the client apps and
 * the host app. A client app holds a {@link android.view.SurfaceView} and delegates its rendering
 * process to the host app. The host app uses the data provided by the client app to render the app
 * content in the surface provided by the SurfaceView.
 * <p>
 * Although both the client app and the host app have independent <strong>view</strong> hierarchies,
 * their <strong>node</strong> hierarchies are connected. The node hierarchy of the host app is
 * embedded into the node hierarchy of the client app. To be more specific, the root node of the
 * host app is the only child of the SurfaceView node, which is a leaf node of the client app.
 */
class SurfaceViewHelper {

    /** The intent action to be used by the host app to bind to the RendererService. */
    private static final String RENDER_ACTION = "android.car.template.host.RendererService";

    /** Package names of the client apps. */
    private final Set<CharSequence> mClientApps = new HashSet<>();

    /** Package name of the host app. */
    @Nullable
    @VisibleForTesting
    String mHostApp;

    /** Initializes the package name of the host app. */
    void initHostApp(@NonNull PackageManager packageManager) {
        List<ResolveInfo> rendererServices = packageManager.queryIntentServices(
                new Intent(RENDER_ACTION), PackageManager.GET_RESOLVED_FILTER);
        if (rendererServices == null || rendererServices.isEmpty()) {
            L.v("No host app found");
            return;
        }
        mHostApp = rendererServices.get(0).serviceInfo.packageName;
        L.v("Host app has been initialized: " + mHostApp);
    }

    /** Clears the package name of the host app if the given {@code packageName} matches. */
    void clearHostApp(@NonNull String packageName) {
        if (packageName.equals(mHostApp)) {
            mHostApp = null;
            L.v("Host app has been set to null");
        }
    }

    /** Adds the package name of the client app. */
    void addClientApp(@NonNull CharSequence clientAppPackageName) {
        mClientApps.add(clientAppPackageName);
    }

    /** Returns whether the given {@code node} represents a view of the host app. */
    boolean isHostNode(@NonNull AccessibilityNodeInfo node) {
        return !TextUtils.isEmpty(mHostApp) && mHostApp.equals(node.getPackageName());
    }

    /** Returns whether the given {@code node} represents a view of the client app. */
    boolean isClientNode(@NonNull AccessibilityNodeInfo node) {
        return mClientApps.contains(node.getPackageName());
    }

    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        writer.println("    hostApp: " + mHostApp);
        writer.println("    clientApps: " + mClientApps);
    }
}
