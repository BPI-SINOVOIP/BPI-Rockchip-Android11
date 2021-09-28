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

package android.server.wm;

import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import android.content.Context;
import android.view.Display;

/**  Base class for window context tests */
class WindowContextTestBase extends MultiDisplayTestBase {

    Context createWindowContext(int displayId) {
        return createWindowContext(displayId, TYPE_APPLICATION_OVERLAY);
    }

    Context createWindowContext(int displayId, int type) {
        final Display display = mDm.getDisplay(displayId);
        return mContext.createDisplayContext(display).createWindowContext(
                type, null /* options */);
    }
}
