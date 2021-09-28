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

package android.platform.helpers;

public interface IAutoDialContactDetailsHelper extends IAppHelper {
    /** enum class for contact number types. */
    enum ContactType {
        WORK,
        HOME,
        MOBILE
    }

    /**
     * Setup expectations: The contact's details page is open.
     *
     * <p>This method is used to add/remove a contact to Favorites.
     */
    void addRemoveFavoriteContact();

    /**
     * Setup expectations: The contact's details page is open.
     *
     * <p>This method is used to call the contact from detail page.
     */
    void makeCallFromDetailsPageByType(ContactType type);

    /**
     * Setup expectations: The contact's details page is open.
     *
     * <p>This method is used to close the details page.
     */
    void closeDetailsPage();
}
