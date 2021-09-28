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
package com.android.wallpaper.picker;

import android.content.Context;
import android.os.Bundle;
import android.os.Message;
import android.os.RemoteException;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.android.wallpaper.R;
import com.android.wallpaper.util.PreviewUtils;
import com.android.wallpaper.util.SurfaceViewUtils;

/** A surface holder callback that renders user's workspace on the passed in surface view. */
public class WorkspaceSurfaceHolderCallback implements SurfaceHolder.Callback {

    private final SurfaceView mWorkspaceSurface;
    private final PreviewUtils mPreviewUtils;

    private Surface mLastSurface;
    private Message mCallback;

    private boolean mNeedsToCleanUp;

    public WorkspaceSurfaceHolderCallback(SurfaceView workspaceSurface, Context context) {
        mWorkspaceSurface = workspaceSurface;
        mPreviewUtils = new PreviewUtils(context,
                context.getString(R.string.grid_control_metadata_name));
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mPreviewUtils.supportsPreview() && mLastSurface != holder.getSurface()) {
            mLastSurface = holder.getSurface();
            Bundle result = renderPreview(mWorkspaceSurface);
            if (result != null) {
                mWorkspaceSurface.setChildSurfacePackage(
                        SurfaceViewUtils.getSurfacePackage(result));
                mCallback = SurfaceViewUtils.getCallback(result);

                if (mNeedsToCleanUp) {
                    cleanUp();
                }
            }
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) { }

    public void cleanUp() {
        if (mCallback != null) {
            try {
                mCallback.replyTo.send(mCallback);
            } catch (RemoteException e) {
                e.printStackTrace();
            } finally {
                mCallback = null;
            }
        } else {
            mNeedsToCleanUp = true;
        }
    }

    public void resetLastSurface() {
        mLastSurface = null;
    }

    protected Bundle renderPreview(SurfaceView workspaceSurface) {
        return mPreviewUtils.renderPreview(
                SurfaceViewUtils.createSurfaceViewRequest(workspaceSurface));
    }
}
