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

package android.platform.helpers;

import android.graphics.Rect;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiObject2;

public interface INewsHelper extends IAppHelper {
    /**
     * Setup expectation: News app is opened.
     *
     * <p>Scroll the page by specified direction.
     */
    public void scrollPage(Direction direction);

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Get the UiObject2 of News scroll container.
     */
    public default UiObject2 getNewsScrollContainer() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Scroll the page to view the news.
     */
    public default void scrollPage(Rect bounds, Direction dir, float percent) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
