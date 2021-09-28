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

@file:JvmName("AccessibilityNodeInfoUtils")

package com.android.compatibility.common.util

import android.graphics.Rect
import android.view.accessibility.AccessibilityNodeInfo
import androidx.test.InstrumentationRegistry

val AccessibilityNodeInfo.bounds: Rect get() = Rect().also { getBoundsInScreen(it) }

fun AccessibilityNodeInfo.click() {
    val bounds = bounds
    SystemUtil.runShellCommand("input tap ${bounds.centerX()} ${bounds.centerY()}")
}

fun AccessibilityNodeInfo.depthFirstSearch(
    condition: (AccessibilityNodeInfo) -> Boolean
): AccessibilityNodeInfo? {
    for (child in children) {
        child?.depthFirstSearch(condition)?.let { return it }
    }
    if (condition(this)) return this
    return null
}

fun AccessibilityNodeInfo.lowestCommonAncestor(
    condition1: (AccessibilityNodeInfo) -> Boolean,
    condition2: (AccessibilityNodeInfo) -> Boolean
): AccessibilityNodeInfo? {
    return depthFirstSearch { node ->
        node.depthFirstSearch(condition1) != null &&
                node.depthFirstSearch(condition2) != null
    }
}

val AccessibilityNodeInfo.children: List<AccessibilityNodeInfo?> get() =
    List(childCount) { i -> getChild(i) }

val AccessibilityNodeInfo.textAsString: String? get() = (text as CharSequence?).toString()

@JvmOverloads
fun uiDump(
    ui: AccessibilityNodeInfo? =
        InstrumentationRegistry.getInstrumentation().uiAutomation.rootInActiveWindow
) = buildString { UiDumpUtils.dumpNodes(ui, this) }