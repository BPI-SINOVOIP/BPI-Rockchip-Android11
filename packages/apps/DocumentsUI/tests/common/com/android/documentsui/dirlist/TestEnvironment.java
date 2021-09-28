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

package com.android.documentsui.dirlist;

import android.content.Context;
import android.database.Cursor;

import com.android.documentsui.ActionHandler;
import com.android.documentsui.Model;
import com.android.documentsui.base.Features;
import com.android.documentsui.base.State;
import com.android.documentsui.testing.TestEnv;

public final class TestEnvironment implements DocumentsAdapter.Environment {
    private final Context testContext;
    private final TestEnv mEnv;
    private final ActionHandler mActionHandler;

    public TestEnvironment(Context testContext, TestEnv env, ActionHandler actionHandler) {
        this.testContext = testContext;
        mEnv = env;
        mActionHandler = actionHandler;
    }

    @Override
    public Features getFeatures() {
        return mEnv.features;
    }

    @Override
    public ActionHandler getActionHandler() {
        return mActionHandler;
    }

    @Override
    public boolean isSelected(String id) {
        return false;
    }

    @Override
    public boolean isDocumentEnabled(String mimeType, int flags) {
        return true;
    }

    @Override
    public void initDocumentHolder(DocumentHolder holder) {
    }

    @Override
    public Model getModel() {
        return mEnv.model;
    }

    @Override
    public State getDisplayState() {
        return mEnv.state;
    }

    @Override
    public boolean isInSearchMode() {
        return false;
    }

    @Override
    public Context getContext() {
        return testContext;
    }

    @Override
    public int getColumnCount() {
        return 4;
    }

    @Override
    public void onBindDocumentHolder(DocumentHolder holder, Cursor cursor) {
    }

    @Override
    public String getCallingAppName() {
        return "unknown";
    }
}
