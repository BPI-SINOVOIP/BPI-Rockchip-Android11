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

import android.support.test.uiautomator.Direction;

public interface IContactsHelper extends IAppHelper {
    /**
     * Setup expectation: Contacts is open
     * <p>
     * Goes to main screen of Contacts
     */
    public default void goToMainScreen() {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: Contacts is open
     * <p>
     * Clicks search field and inputs contact to search.
     *
     * @param contact The contact to search.
     */
    public default void searchForContact(String contact) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectation: Contacts is open
     *
     * <p>Clicks search field and inputs contact to search. Provide alternative ways to input
     * contact.
     *
     * @param contact The contact to search.
     * @param useKeyboard Use KeyEvent to input contact if true, use UiObject2 setText otherwise.
     */
    public default void searchForContact(String contact, boolean useKeyboard) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: The search results of contact showed.
     * <p>
     * Selects the contact by the specific index.
     *
     * @param index The index of contact to select.
     */
    public default void chooseContactByIndex(int index) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }

    /**
     * Setup expectations: Contacts is open.
     *
     * <p>Fling contacts page.
     *
     * @param direction The direction to fling the page.
     */
    public default void flingPage(Direction direction) {
        throw new UnsupportedOperationException("Not yet implemented.");
    }
}
