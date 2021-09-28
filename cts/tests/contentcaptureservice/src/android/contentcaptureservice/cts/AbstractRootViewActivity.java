/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.DoubleVisitor;

/**
 * Base class for classes that have a {@code root_view} root view.
 */
abstract class AbstractRootViewActivity extends AbstractContentCaptureActivity {

    private static final String TAG = AbstractRootViewActivity.class.getSimpleName();

    private static DoubleVisitor<AbstractRootViewActivity, LinearLayout> sRootViewVisitor;
    private static DoubleVisitor<AbstractRootViewActivity, LinearLayout> sOnAnimationVisitor;

    private LinearLayout mRootView;

    /**
     * Sets a visitor called when the activity is created.
     */
    static void onRootView(@NonNull DoubleVisitor<AbstractRootViewActivity, LinearLayout> visitor) {
        sRootViewVisitor = visitor;
    }

    /**
     * Sets a visitor to be called on {@link Activity#onEnterAnimationComplete()}.
     */
    static void onAnimationComplete(
            @NonNull DoubleVisitor<AbstractRootViewActivity, LinearLayout> visitor) {
        sOnAnimationVisitor = visitor;
    }

    @Override
    protected final void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentViewOnCreate(savedInstanceState);

        mRootView = findViewById(R.id.root_view);

        Log.d(TAG, "onCreate(): parents for " + getClass() + ": rootView=" + mRootView
                + "\ngrandParent=" + getGrandParent()
                + "\ngrandGrandParent=" + getGrandGrandParent());

        if (sRootViewVisitor != null) {
            Log.d(TAG, "Applying visitor to " + this + "/" + mRootView);
            try {
                sRootViewVisitor.visit(this, mRootView);
            } finally {
                sRootViewVisitor = null;
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        Log.d(TAG, "AutofillIds for " + getClass() + ": "
                + " rootView=" + getRootView().getAutofillId()
                + ", grandParent=" + getGrandParent().getAutofillId()
                + ", grandGrandParent=" + getGrandGrandParent().getAutofillId());
    }

    @Override
    public void onEnterAnimationComplete() {
        if (sOnAnimationVisitor != null) {
            Log.i(TAG, "onEnterAnimationComplete(): applying visitor on " + this);
            try {
                sOnAnimationVisitor.visit(this, mRootView);
            } finally {
                sOnAnimationVisitor = null;
            }
        } else {
            Log.i(TAG, "onEnterAnimationComplete(): no visitor on " + this);
        }
    }

    public LinearLayout getRootView() {
        return mRootView;
    }

    // TODO(b/122315042): remove this method when not needed anymore
    @NonNull
    public ViewGroup getGrandParent() {
        return (ViewGroup) mRootView.getParent();
    }

    // TODO(b/122315042): remove this method when not needed anymore
    @NonNull
    public ViewGroup getGrandGrandParent() {
        return (ViewGroup) getGrandParent().getParent();
    }

    /**
     * The real "onCreate" method that should be extended by subclasses.
     *
     */
    protected abstract void setContentViewOnCreate(Bundle savedInstanceState);
}
