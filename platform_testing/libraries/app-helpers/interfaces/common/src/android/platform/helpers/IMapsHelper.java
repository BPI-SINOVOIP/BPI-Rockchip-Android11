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

package android.platform.helpers;

import android.graphics.Rect;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiObject2;

public interface IMapsHelper extends IAppHelper {
    /**
     * Setup expectation: On the standard Map screen in any setup.
     *
     * Best effort attempt to go to the query screen (if not currently there),
     * does a search, and selects the results.
     */
    public void doSearch(String query);

    /**
     * Setup expectation: Destination is selected.
     *
     * Best effort attempt to go to the directions screen for the selected destination.
     */
    public void getDirections();

    /**
     * Setup expectation: On directions screen.
     *
     * Best effort attempt to start navigation for the selected destination.
     */
    public void startNavigation();

    /**
     * Setup expectation: On navigation screen.
     *
     * Best effort attempt to stop navigation, and go back to the directions screen.
     */
    public void stopNavigation();

    /**
     * Setup expectation: Destination is selected.
     *
     * Goes to the details screen for the selected destination.
     */
    default public void openAddressDetails() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the destination details screen.
     *
     * Flings down the destination details screen.
     */
    default public void flingAddressDetails() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the navigation screen.
     *
     * <p>Click the home button to go to the navigation pip page.
     */
    public default void goToNavigatePip() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Click the navigation pip to go to the navigation page.
     */
    public default void backToNavigationFromPip() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the standard Map screen in any setup.
     *
     * <p>Best effort attempt to go to the query screen (if not currently there),
     * does a search.
     */
    public default void inputSearch(String query) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the search screen.
     *
     * <p>Best effort attempt to go to the query screen (if not currently there),
     * selects the results.
     */
    public default void startSearch() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: Destination is selected.
     *
     * <p>Go to the street view screen for the selected destination.
     */
    public default void openStreetView() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the street view screen.
     *
     * <p>Swipe the street view screen.
     */
    public default void swipeStreetView(Direction direction) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Get the UiObject2 of explore tab home bottom sheet.
     */
    public default UiObject2 getExploreTabHomeBottomSheet() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Scroll the page to view the map.
     */
    public default void scrollPage(Rect bounds, Direction dir, float percent) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Click the button of my location on the home screen.
     */
    public default void clickMyLocationButton() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: On the home screen.
     *
     * <p>Click the button of compass when exist on the home screen.
     */
    public default void clickBaseCompassButton() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
