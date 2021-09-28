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

package com.android.compatibility.common.util;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

import com.android.server.wm.ActivityRecordProto;
import com.android.server.wm.DisplayAreaProto;
import com.android.server.wm.DisplayContentProto;
import com.android.server.wm.RootWindowContainerProto;
import com.android.server.wm.TaskProto;
import com.android.server.wm.WindowContainerChildProto;
import com.android.server.wm.WindowContainerProto;
import com.android.server.wm.WindowManagerServiceDumpProto;
import com.android.server.wm.WindowStateProto;
import com.android.server.wm.WindowTokenProto;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.ITestDevice;

import java.util.ArrayList;
import java.util.List;

public class WindowManagerUtil {
    private static final String SHELL_DUMPSYS_WINDOW = "dumpsys window --proto";

    @Nonnull
    public static WindowManagerServiceDumpProto getDump(@Nonnull ITestDevice device)
            throws Exception {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        device.executeShellCommand(SHELL_DUMPSYS_WINDOW, receiver);
        return WindowManagerServiceDumpProto.parser().parseFrom(receiver.getOutput());
    }

    @Nonnull
    public static List<WindowStateProto> getWindows(@Nonnull ITestDevice device) throws Exception {
        final WindowManagerServiceDumpProto windowManagerServiceDump = getDump(device);
        final RootWindowContainerProto rootWindowContainer =
                windowManagerServiceDump.getRootWindowContainer();
        final WindowContainerProto windowContainer = rootWindowContainer.getWindowContainer();

        final List<WindowStateProto> windows = new ArrayList<>();
        collectWindowStates(windowContainer, windows);

        return windows;
    }

    @Nullable
    public static WindowStateProto getWindowWithTitle(@Nonnull ITestDevice device,
            @Nonnull String expectedTitle) throws Exception {
        List<WindowStateProto> windows = getWindows(device);
        for (WindowStateProto window : windows) {
            if (expectedTitle.equals(window.getIdentifier().getTitle())) {
                return window;
            }
        }

        return null;
    }

    public static boolean hasWindowWithTitle(@Nonnull ITestDevice device,
            @Nonnull String expectedTitle) throws Exception {
        return getWindowWithTitle(device, expectedTitle) != null;
    }

    /**
     * This methods implements a DFS that goes through a tree of window containers and collects all
     * the WindowStateProto-s.
     *
     * WindowContainer is generic class that can hold windows directly or through its children in a
     * hierarchy form. WindowContainer's children are WindowContainer as well. This forms a tree of
     * WindowContainers.
     *
     * There are a few classes that extend WindowContainer: Task, DisplayContent, WindowToken etc.
     * The one we are interested in is WindowState.
     * Since Proto does not have concept of inheritance, {@link TaskProto}, {@link WindowTokenProto}
     * etc hold a reference to a {@link WindowContainerProto} (in java code would be {@code super}
     * reference).
     * {@link WindowContainerProto} may a have a number of children of type
     * {@link WindowContainerChildProto}, which represents a generic child of a WindowContainer: a
     * WindowContainer can have multiple children of different types stored as a
     * {@link WindowContainerChildProto}, but each instance of {@link WindowContainerChildProto} can
     * only contain a single type.
     *
     * For details see /frameworks/base/core/proto/android/server/windowmanagerservice.proto
     */
    private static void collectWindowStates(@Nullable WindowContainerProto windowContainer,
            @Nonnull List<WindowStateProto> out) {
        if (windowContainer == null) return;

        final List<WindowContainerChildProto> children = windowContainer.getChildrenList();
        for (WindowContainerChildProto child : children) {
            if (child.hasWindowContainer()) {
                collectWindowStates(child.getWindowContainer(), out);
            } else if (child.hasDisplayContent()) {
                final DisplayContentProto displayContent = child.getDisplayContent();
                for (WindowTokenProto windowToken : displayContent.getOverlayWindowsList()) {
                    collectWindowStates(windowToken.getWindowContainer(), out);
                }
                if (displayContent.hasRootDisplayArea()) {
                    final DisplayAreaProto displayArea = displayContent.getRootDisplayArea();
                    collectWindowStates(displayArea.getWindowContainer(), out);
                }
                collectWindowStates(displayContent.getWindowContainer(), out);
            } else if (child.hasDisplayArea()) {
                final DisplayAreaProto displayArea = child.getDisplayArea();
                collectWindowStates(displayArea.getWindowContainer(), out);
            } else if (child.hasTask()) {
                final TaskProto task = child.getTask();
                collectWindowStates(task.getWindowContainer(), out);
            } else if (child.hasActivity()) {
                final ActivityRecordProto activity = child.getActivity();
                if (activity.hasWindowToken()) {
                    final WindowTokenProto windowToken = activity.getWindowToken();
                    collectWindowStates(windowToken.getWindowContainer(), out);
                }
            } else if (child.hasWindowToken()) {
                final WindowTokenProto windowToken = child.getWindowToken();
                collectWindowStates(windowToken.getWindowContainer(), out);
            } else if (child.hasWindow()) {
                final WindowStateProto window = child.getWindow();
                // We found a Window!
                out.add(window);
                // ... but still aren't done
                collectWindowStates(window.getWindowContainer(), out);
            }
        }
    }
}
