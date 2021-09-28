/**
 * Copyright (C) 2019 The Android Open Source Project
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.accessibilityservice.cts;

import static android.view.accessibility.AccessibilityEvent.TYPE_GESTURE_DETECTION_END;
import static android.view.accessibility.AccessibilityEvent.TYPE_GESTURE_DETECTION_START;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_CLICKED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_LONG_CLICKED;

import android.view.accessibility.AccessibilityEvent;

/**
 * This accessibility service stub collects all events relating to touch exploration rather than
 * just the few collected by GestureDetectionStubAccessibilityService
 */
public class TouchExplorationStubAccessibilityService
        extends GestureDetectionStubAccessibilityService {
    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        synchronized (mLock) {
            switch (event.getEventType()) {
                case TYPE_GESTURE_DETECTION_START:
                case TYPE_GESTURE_DETECTION_END:
                case TYPE_VIEW_ACCESSIBILITY_FOCUSED:
                case TYPE_VIEW_CLICKED:
                case TYPE_VIEW_LONG_CLICKED:
                    mCollectedEvents.add(event.getEventType());
            }
        }
        super.onAccessibilityEvent(event);
    }
}
