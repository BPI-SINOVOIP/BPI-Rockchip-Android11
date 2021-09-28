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
 * limitations under the License
 */

package android.accessibility.cts.common;

import static android.accessibilityservice.AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;

import static androidx.test.InstrumentationRegistry.getContext;
import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertFalse;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.UiAutomation;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Environment;
import android.support.test.uiautomator.Configurator;
import android.support.test.uiautomator.UiDevice;
import android.text.TextUtils;
import android.util.Log;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import com.android.compatibility.common.util.BitmapUtils;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.time.LocalTime;
import java.util.HashSet;
import java.util.Set;

/**
 * Helper class to dump data for accessibility test cases.
 *
 * It can dump {@code dumpsys accessibility}, accessibility node tree to logcat and/or
 * screenshot for inspect later.
 */
public class AccessibilityDumper {
    private static final String TAG = "AccessibilityDumper";

    /** Dump flag to write the output of {@code dumpsys accessibility} to logcat. */
    public static final int FLAG_DUMPSYS = 0x1;

    /** Dump flag to write the output of {@code uiautomator dump} to logcat. */
    public static final int FLAG_HIERARCHY = 0x2;

    /** Dump flag to save the screenshot to external storage. */
    public static final int FLAG_SCREENSHOT = 0x4;

    /** Dump flag to write the tree of accessility node info to logcat. */
    public static final int FLAG_NODETREE = 0x8;

    /** Default dump flag */
    public static final int FLAG_DUMP_ALL = FLAG_DUMPSYS | FLAG_HIERARCHY | FLAG_SCREENSHOT;

    private static AccessibilityDumper sDumper;

    private int mFlag;

    /** Screenshot filename */
    private String mName;

    /** Root directory matching the directory-key of collector in AndroidTest.xml */
    private File mRoot;

    public static synchronized AccessibilityDumper getInstance() {
        if (sDumper == null) {
            sDumper = new AccessibilityDumper(FLAG_DUMP_ALL);
        }
        return sDumper;
    }

    /**
     * Define the directory to dump/clean and initial dump options
     *
     * @param flag control what to dump
     */
    private AccessibilityDumper(int flag) {
        mRoot = getDumpRoot(getContext().getPackageName());
        mFlag = flag;
    }

    public void dump(int flag) {
        final UiAutomation automation = getUiAutomation();

        if ((flag & FLAG_DUMPSYS) != 0) {
            dumpsysOnLogcat(automation);
        }
        if ((flag & FLAG_HIERARCHY) != 0) {
            dumpHierarchyOnLogcat();
        }
        if ((flag & FLAG_SCREENSHOT) != 0) {
            dumpScreen(automation);
        }
        if ((flag & FLAG_NODETREE) != 0) {
            dumpAccessibilityNodeTreeOnLogcat(automation);
        }
    }

    void dump() {
        dump(mFlag);
    }

    void setName(String name) {
        assertNotEmpty(name);
        mName = name;
    }

    private File getDumpRoot(String directory) {
        return new File(Environment.getExternalStorageDirectory(), directory);
    }

    private void dumpsysOnLogcat(UiAutomation automation) {
        ShellCommandBuilder.create(automation)
            .addCommandPrintOnLogCat("dumpsys accessibility")
            .run();
    }

    private void dumpHierarchyOnLogcat() {
        try(ByteArrayOutputStream os = new ByteArrayOutputStream()) {
            UiDevice.getInstance(getInstrumentation()).dumpWindowHierarchy(os);
            Log.w(TAG, "Window hierarchy:");
            for (String line : os.toString("UTF-8").split("\\n")) {
                Log.w(TAG, line);
            }
        } catch (Exception e) {
            Log.e(TAG, "ERROR: unable to dumping hierarchy on logcat", e);
        }
    }

    private void dumpScreen(UiAutomation automation) {
        assertNotEmpty(mName);
        final Bitmap screenshot = automation.takeScreenshot();
        final String filename = String.format("%s_%s__screenshot.png", mName, LocalTime.now());
        BitmapUtils.saveBitmap(screenshot, mRoot.toString(), filename);
    }

    /** Dump hierarchy compactly and include nodes not visible to user */
    private void dumpAccessibilityNodeTreeOnLogcat(UiAutomation automation) {
        final Set<AccessibilityNodeInfo> roots = new HashSet<>();
        for (AccessibilityWindowInfo window : automation.getWindows()) {
            AccessibilityNodeInfo root = window.getRoot();
            if (root == null) {
                Log.w(TAG, String.format("Skipping null root node for window: %s",
                        window.toString()));
            } else {
                roots.add(root);
            }
        }
        if (roots.isEmpty()) {
            Log.w(TAG, "No node of windows to dump");
        } else {
            Log.w(TAG, "Accessibility nodes hierarchy:");
            for (AccessibilityNodeInfo root : roots) {
                dumpTreeWithPrefix(root, "");
            }
        }
    }

    private static void dumpTreeWithPrefix(AccessibilityNodeInfo node, String prefix) {
        final StringBuilder nodeText = new StringBuilder(prefix);
        appendNodeText(nodeText, node);
        Log.v(TAG, nodeText.toString());
        final int count = node.getChildCount();
        for (int i = 0; i < count; i++) {
            AccessibilityNodeInfo child = node.getChild(i);
            if (child != null) {
                dumpTreeWithPrefix(child, "-" + prefix);
            } else {
                Log.i(TAG, String.format("%sNull child %d/%d", prefix, i, count));
            }
        }
    }

    private static void appendNodeText(StringBuilder out, AccessibilityNodeInfo node) {
        final CharSequence txt = node.getText();
        final CharSequence description = node.getContentDescription();
        final String viewId = node.getViewIdResourceName();

        if (!TextUtils.isEmpty(description)) {
            out.append(escape(description));
        } else if (!TextUtils.isEmpty(txt)) {
            out.append('"').append(escape(txt)).append('"');
        }
        if (!TextUtils.isEmpty(viewId)) {
            out.append("(").append(viewId).append(")");
        }
        out.append("+").append(node.getClassName());
        out.append("+ \t<");
        out.append(node.isCheckable()       ? "C" : ".");
        out.append(node.isChecked()         ? "c" : ".");
        out.append(node.isClickable()       ? "K" : ".");
        out.append(node.isEnabled()         ? "E" : ".");
        out.append(node.isFocusable()       ? "F" : ".");
        out.append(node.isFocused()         ? "f" : ".");
        out.append(node.isLongClickable()   ? "L" : ".");
        out.append(node.isPassword()        ? "P" : ".");
        out.append(node.isScrollable()      ? "S" : ".");
        out.append(node.isSelected()        ? "s" : ".");
        out.append(node.isVisibleToUser()   ? "V" : ".");
        out.append("> ");
        final Rect bounds = new Rect();
        node.getBoundsInScreen(bounds);
        out.append(bounds.toShortString());
    }

    /**
     * Produce a displayable string from a CharSequence
     */
    private static String escape(CharSequence s) {
        final StringBuilder out = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if ((c < 127) || (c == 0xa0) || ((c >= 0x2000) && (c < 0x2070))) {
                out.append(c);
            } else {
                out.append("\\u").append(Integer.toHexString(c));
            }
        }
        return out.toString();
    }

    private void assertNotEmpty(String name) {
        assertFalse("Expected non empty name.", TextUtils.isEmpty(name));
    }

    private UiAutomation getUiAutomation() {
        // Reuse UiAutomation from UiAutomator with the same flag
        Configurator.getInstance().setUiAutomationFlags(
                FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        final UiAutomation automation = getInstrumentation().getUiAutomation(
                FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        // Dump window info & node tree
        final AccessibilityServiceInfo info = automation.getServiceInfo();
        if (info != null && ((info.flags & FLAG_RETRIEVE_INTERACTIVE_WINDOWS) == 0)) {
            info.flags |= FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
            automation.setServiceInfo(info);
        }
        return automation;
    }
}
