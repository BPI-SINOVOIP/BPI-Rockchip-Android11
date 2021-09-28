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
package com.android.customization.picker.grid;

import android.content.Context;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

import com.android.customization.model.grid.GridOption;
import com.android.customization.model.grid.GridOptionsManager;
import com.android.wallpaper.picker.WorkspaceSurfaceHolderCallback;
import com.android.wallpaper.util.SurfaceViewUtils;

/** A class to load the {@link GridOption} preview to the view. */
class GridOptionPreviewer {

    private final GridOptionsManager mGridManager;
    private final ViewGroup mPreviewContainer;

    private SurfaceView mGridOptionSurface;
    private GridOption mGridOption;
    private GridOptionSurfaceHolderCallback mSurfaceCallback;

    GridOptionPreviewer(GridOptionsManager gridManager, ViewGroup previewContainer) {
        mGridManager = gridManager;
        mPreviewContainer = previewContainer;
    }

    /** Loads the Grid option into the container view. */
    public void setGridOption(GridOption gridOption) {
        mGridOption = gridOption;
        if (mGridOption != null) {
            updateWorkspacePreview();
        }
    }

    /** Releases the view resource. */
    public void release() {
        if (mGridOptionSurface != null) {
            mSurfaceCallback.cleanUp();
            mGridOptionSurface = null;
        }
        mPreviewContainer.removeAllViews();
    }

    private void updateWorkspacePreview() {
        // Reattach SurfaceView to trigger #surfaceCreated to update preview for different option.
        mPreviewContainer.removeAllViews();
        if (mSurfaceCallback != null) {
            mSurfaceCallback.resetLastSurface();
        }
        if (mGridOptionSurface == null) {
            mGridOptionSurface = new SurfaceView(mPreviewContainer.getContext());
            mGridOptionSurface.setLayoutParams(new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            mGridOptionSurface.setZOrderMediaOverlay(true);
            mSurfaceCallback = new GridOptionSurfaceHolderCallback(mGridOptionSurface,
                    mGridOptionSurface.getContext());
            mGridOptionSurface.getHolder().addCallback(mSurfaceCallback);
        }
        mPreviewContainer.addView(mGridOptionSurface);
    }

    private class GridOptionSurfaceHolderCallback extends WorkspaceSurfaceHolderCallback {
        private GridOptionSurfaceHolderCallback(SurfaceView workspaceSurface, Context context) {
            super(workspaceSurface, context);
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            if (mGridOption != null) {
                super.surfaceCreated(holder);
            }
        }

        @Override
        protected Bundle renderPreview(SurfaceView workspaceSurface) {
            return mGridManager.renderPreview(
                    SurfaceViewUtils.createSurfaceViewRequest(workspaceSurface),
                    mGridOption.name);
        }
    }
}
