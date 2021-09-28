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

package com.android.uibench.microbenchmark;

import android.platform.helpers.IAppHelper;

public interface IUiBenchJankHelper extends IAppHelper {
    void clickImage(String imageName);

    void flingDownUp(int flingCount);

    void flingUpDown(int flingCount);

    void openActivityTransition();

    void openBitmapUpload();

    void openClippedListView();

    void openDialogList();

    void openEditTextTyping();

    void openFadingEdgeListView();

    void openFullscreenOverdraw();

    void openGLTextureView();

    void openInflatingEmojiListView();

    void openInflatingHanListView();

    void openInflatingListView();

    void openInflatingLongStringListView();

    void openInvalidate();

    void openInvalidateTree();

    void openLayoutCacheHighHitrate();

    void openLayoutCacheLowHitrate();

    void openLeanbackActivity(
            boolean extraBitmapUpload,
            boolean extraShowFastLane,
            String activityName,
            String expectedText);

    void openNavigationDrawerActivity();

    void openNotificationShade();

    void openSaveLayerInterleaveActivity();

    void openScrollableWebView();

    void openSlowBindRecyclerView();

    void openSlowNestedRecyclerView();

    void openTrivialAnimation();

    void openTrivialListView();

    void openTrivialRecyclerView();

    void openRenderingList();

    void openResizeHWLayer();

    void openWindowInsetsController();

    void scrollDownAndUp(int count);

    void slowSingleFlingDown();

    void swipeRightLeft(int swipeCount);
}
