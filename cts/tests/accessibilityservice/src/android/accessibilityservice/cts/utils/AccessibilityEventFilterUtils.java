/**
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.accessibilityservice.cts.utils;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.both;

import android.app.UiAutomation;
import android.app.UiAutomation.AccessibilityEventFilter;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityWindowInfo;

import androidx.annotation.NonNull;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;

import java.util.List;
import java.util.function.BiPredicate;

/**
 * Utility class for creating AccessibilityEventFilters
 */
public class AccessibilityEventFilterUtils {
    public static AccessibilityEventFilter filterForEventType(int eventType) {
        return (new AccessibilityEventTypeMatcher(eventType))::matches;
    }

    public static AccessibilityEventFilter filterWindowsChangedWithChangeTypes(int changes) {
        return (both(new AccessibilityEventTypeMatcher(AccessibilityEvent.TYPE_WINDOWS_CHANGED))
                        .and(new WindowChangesMatcher(changes)))::matches;
    }

    public static AccessibilityEventFilter filterForEventTypeWithResource(int eventType,
            String ResourceName) {
        TypeSafeMatcher<AccessibilityEvent> matchResourceName = new PropertyMatcher<>(
                ResourceName, "Resource name",
                (event, expect) -> event.getSource() != null
                        && event.getSource().getViewIdResourceName().equals(expect));
        return (both(new AccessibilityEventTypeMatcher(eventType)).and(matchResourceName))::matches;
    }

    public static AccessibilityEventFilter filterWindowsChangeTypesAndWindowTitle(
            @NonNull UiAutomation uiAutomation, int changeTypes, @NonNull String title) {
        return allOf(new AccessibilityEventTypeMatcher(AccessibilityEvent.TYPE_WINDOWS_CHANGED),
                new WindowChangesMatcher(changeTypes),
                new WindowTitleMatcher(uiAutomation, title))::matches;
    }

    public static AccessibilityEventFilter filterWindowsChangTypesAndWindowId(int windowId,
            int changeTypes) {
        return allOf(new AccessibilityEventTypeMatcher(AccessibilityEvent.TYPE_WINDOWS_CHANGED),
                new WindowChangesMatcher(changeTypes),
                new WindowIdMatcher(windowId))::matches;
    }

    public static class AccessibilityEventTypeMatcher extends TypeSafeMatcher<AccessibilityEvent> {
        private int mType;

        public AccessibilityEventTypeMatcher(int type) {
            super();
            mType = type;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            return event.getEventType() == mType;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("Matching to type " + mType);
        }
    }

    public static class WindowChangesMatcher extends TypeSafeMatcher<AccessibilityEvent> {
        private int mWindowChanges;

        public WindowChangesMatcher(int windowChanges) {
            super();
            mWindowChanges = windowChanges;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            return (event.getWindowChanges() & mWindowChanges) == mWindowChanges;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("With window change type " + mWindowChanges);
        }
    }

    public static class ContentChangesMatcher extends TypeSafeMatcher<AccessibilityEvent> {
        private int mContentChanges;

        public ContentChangesMatcher(int contentChanges) {
            super();
            mContentChanges = contentChanges;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            return (event.getContentChangeTypes() & mContentChanges) == mContentChanges;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("With content change type " + mContentChanges);
        }
    }

    public static class PropertyMatcher<T> extends TypeSafeMatcher<AccessibilityEvent> {
        private T mProperty;
        private String mDescription;
        private BiPredicate<AccessibilityEvent, T> mComparator;

        public PropertyMatcher(T property, String description,
                BiPredicate<AccessibilityEvent, T> comparator) {
            super();
            mProperty = property;
            mDescription = description;
            mComparator = comparator;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            return mComparator.test(event, mProperty);
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("Matching to " + mDescription + " " + mProperty.toString());
        }
    }

    public static class WindowTitleMatcher extends TypeSafeMatcher<AccessibilityEvent> {
        private final UiAutomation mUiAutomation;
        private final String mTitle;

        public WindowTitleMatcher(@NonNull UiAutomation uiAutomation, @NonNull String title) {
            super();
            mUiAutomation = uiAutomation;
            mTitle = title;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            final List<AccessibilityWindowInfo> windows = mUiAutomation.getWindows();
            final int eventWindowId = event.getWindowId();
            for (AccessibilityWindowInfo info : windows) {
                if (eventWindowId == info.getId() && mTitle.equals(info.getTitle())) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("With window title " + mTitle);
        }
    }

    public static class WindowIdMatcher extends TypeSafeMatcher<AccessibilityEvent> {
        private int mWindowId;

        public WindowIdMatcher(int windowId) {
            super();
            mWindowId = windowId;
        }

        @Override
        protected boolean matchesSafely(AccessibilityEvent event) {
            return event.getWindowId() == mWindowId;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("With window Id " + mWindowId);
        }
    }
}
