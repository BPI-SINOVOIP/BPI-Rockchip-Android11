/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui.bots;

import android.content.Context;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;

import junit.framework.Assert;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

/**
 * A test helper class that provides support for controlling the UI Breadcrumb
 * programmatically, and making assertions against the state of the UI.
 * <p>
 * Support for working directly with Roots and Directory view can be found in the respective bots.
 */
public class BreadBot extends Bots.BaseBot {

    private final String mBreadCrumbId;

    public BreadBot(UiDevice device, Context context, int timeout) {
        super(device, context, timeout);
        mBreadCrumbId = mTargetPackage + ":id/horizontal_breadcrumb";
    }

    public void clickItem(String label) {
        findHorizontalEntry(label).click();
    }

    public void assertItemsPresent(String... items) {
        Predicate<String> checker = this::hasHorizontalEntry;
        assertItemsPresent(items, checker);
    }

    private void assertItemsPresent(String[] items, Predicate<String> predicate) {
        List<String> absent = new ArrayList<>();
        for (String item : items) {
            if (!predicate.test(item)) {
                absent.add(item);
            }
        }
        if (!absent.isEmpty()) {
            Assert.fail("Expected iteams " + Arrays.asList(items)
                    + ", but missing " + absent);
        }
    }

    private boolean hasHorizontalEntry(String label) {
        return findHorizontalEntry(label) != null;
    }

    @SuppressWarnings("unchecked")
    private UiObject2 findHorizontalEntry(String label) {
        UiObject2 breadcrumb = mDevice.findObject(By.res(mBreadCrumbId));
        return breadcrumb.findObject(By.text(label));
    }
}
