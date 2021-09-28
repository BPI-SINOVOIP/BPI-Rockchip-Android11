/*
 * Copyright (C) 2021 The Android Open Source Project
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

import static android.view.accessibility.AccessibilityWindowInfo.UNDEFINED_WINDOW_ID;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.Display;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.annotation.Nullable;

/**
 * A builder which builds a mock {@link AccessibilityWindowInfo}. Unlike real windows, mock windows
 * don't need to be recycled.
 */
class WindowBuilder {
    /** The ID of the window. */
    private int mId = UNDEFINED_WINDOW_ID;
    /** The root node in the window's hierarchy. */
    private AccessibilityNodeInfo mRoot;
    /** The bounds of this window in the screen. */
    private Rect mBoundsInScreen;
    /** The window type, if specified. */
    private int mType;
    /** The display ID, if specified. */
    private int mDisplayId = Display.DEFAULT_DISPLAY;

    AccessibilityWindowInfo build() {
        AccessibilityWindowInfo window = mock(AccessibilityWindowInfo.class);
        when(window.getId()).thenReturn(mId);
        when(window.getRoot())
                .thenReturn(MockNodeCopierProvider.get().copy(mRoot))
                .thenReturn(MockNodeCopierProvider.get().copy(mRoot))
                .thenReturn(MockNodeCopierProvider.get().copy(mRoot))
                .thenReturn(MockNodeCopierProvider.get().copy(mRoot))
                .thenThrow(new RuntimeException(
                        "Exceeded the maximum calls. Please add more parameters"));
        if (mBoundsInScreen != null) {
            // Mock AccessibilityWindowInfo#getBoundsInScreen(Rect).
            doAnswer(invocation -> {
                Object[] args = invocation.getArguments();
                ((Rect) args[0]).set(mBoundsInScreen);
                return null;
            }).when(window).getBoundsInScreen(any(Rect.class));
        }
        when(window.getType()).thenReturn(mType);
        when(window.getDisplayId()).thenReturn(mDisplayId);
        return window;
    }

    WindowBuilder setId(int id) {
        mId = id;
        return this;
    }

    WindowBuilder setRoot(@Nullable AccessibilityNodeInfo root) {
        mRoot = MockNodeCopierProvider.get().copy(root);
        return this;
    }

    WindowBuilder setBoundsInScreen(@Nullable Rect boundsInScreen) {
        mBoundsInScreen = boundsInScreen;
        return this;
    }

    WindowBuilder setType(int type) {
        mType = type;
        return this;
    }

    WindowBuilder setDisplayId(int displayId) {
        mDisplayId = displayId;
        return this;
    }
}
