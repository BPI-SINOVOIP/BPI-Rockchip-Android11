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

package android.contentcaptureservice.cts;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.view.ViewStructure;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.Visitor;

/**
 * A view that can be used to emulate custom behavior (like virtual children) on
 * {@link #onProvideContentCaptureStructure(ViewStructure, int)}.
 */
public class CustomView extends View {

    private static final String TAG = CustomView.class.getSimpleName();

    private Visitor<ViewStructure> mDelegate;

    public CustomView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setImportantForContentCapture(View.IMPORTANT_FOR_CONTENT_CAPTURE_YES);
    }

    @Override
    public void onProvideContentCaptureStructure(ViewStructure structure, int flags) {
        if (mDelegate != null) {
            Log.d(TAG, "onProvideContentCaptureStructure(): delegating");
            structure.setClassName(getAccessibilityClassName().toString());
            mDelegate.visit(structure);
            Log.d(TAG, "onProvideContentCaptureStructure(): delegated");
        } else {
            superOnProvideContentCaptureStructure(structure, flags);
        }
    }

    @Override
    public CharSequence getAccessibilityClassName() {
        return CustomView.class.getName();
    }

    void superOnProvideContentCaptureStructure(@NonNull ViewStructure structure, int flags) {
        Log.d(TAG, "calling super.onProvideContentCaptureStructure()");
        super.onProvideContentCaptureStructure(structure, flags);
    }

    void setContentCaptureDelegate(@NonNull Visitor<ViewStructure> delegate) {
        mDelegate = delegate;
    }
}
