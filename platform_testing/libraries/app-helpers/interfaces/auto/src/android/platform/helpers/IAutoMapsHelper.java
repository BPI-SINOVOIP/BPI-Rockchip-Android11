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

public interface IAutoMapsHelper extends IAppHelper {

    /**
     * Setup expectations: Maps app is open
     *
     * <p>This method is used to verify whether search bar is present.
     */
    boolean isSearchBarPresent();

    /**
     * Setup expectations: Maps app is open
     *
     * This method is used to search an address.
     *
     */
    void search(String address);

    /**
     * Setup expectations: Maps app is open
     *
     * This method is used to start navigation.
     *
     */
    void startNavigation();

    /**
     * Setup expectations: Maps app is open
     *
     * This method is used to stop navigation.
     *
     */
    void stopNavigation();

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Swipe upwards by <code>percent</code> percent of maps region in <code>durationMs</code>
     * milliseconds.
     *
     * @param percent The percentage of the scrollable region by which to swipe up.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void swipeUp(float percent, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Swipe downwards by <code>percent</code> percent of maps region in <code>durationMs</code>
     * milliseconds.
     *
     * @param percent The percentage of the scrollable region by which to swipe down.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void swipeDown(float percent, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Swipe leftwards by <code>percent</code> percent of maps region in <code>durationMs</code>
     * milliseconds.
     *
     * @param percent The percentage of the scrollable region by which to swipe left.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void swipeLeft(float percent, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Swipe rightwards by <code>percent</code> percent of maps region in <code>durationMs</code>
     * milliseconds.
     *
     * @param percent The percentage of the scrollable region by which to swipe right.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void swipeRight(float percent, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Scale by <code>percent</code> percent in <code>durationMs</code> milliseconds. When <code>
     * percent</code> is less 100, it scales down, and vice versa. For example, when percent is 50,
     * it will scale down so that objects look half the size as before, and when percent is 200, it
     * will scale up and objects look twice as large.
     *
     * @param percent The percentage by which to scale up/down.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void scale(float percent, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Rotate by <code>angle</code> degrees in <code>durationMs</code> milliseconds, rotation is
     * around the center of the maps region. Positive angle for clockwise rotation, and negative
     * angle for counter-clockwise rotation.
     *
     * @param angle The angle in degree by which to rotate to screen.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void rotate(float angle, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Tilt up by <code>angle</code> degrees in <code>durationMs</code> milliseconds.
     *
     * @param angle The angle in degree by which to tilt up the screen's view angle.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void tiltUp(float angle, long durationMs);

    /**
     * Setup expectations: Maps app is open
     *
     * <p>Tilt down by <code>angle</code> degrees in <code>durationMs</code> milliseconds.
     *
     * @param angle The angle in degree by which to tilt down the screen's view angle.
     * @param durationMs The duration it takes to perform this gesture in milliseconds.
     */
    void tiltDown(float angle, long durationMs);
}
