/*
 * Copyright (C) 2017 The Android Open Source Project
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

/** An App Helper interface for the Phone. */
public interface IPhoneHelper extends IAppHelper {
    /**
     * Setup expectation: Phone app is open
     *
     * Go to number pad and dial number
     */
    public void dialNumber(String number);

    /**
     * Setup expectation: Number is being called
     *
     * Hang up
     */
    public void hangUp();

    /**
     * Setup expectations: Phone app is open.
     * <p>
     * Goes to the Contacts tab.
     */
    public void navigateToContacts();

    /**
     * Setup expectations: in Contacts tab.
     * <p>
     * Clicks search field and inputs contact to search.
     *
     * @param contact The contact to search.
     */
    public void searchForContact(String contact);

    /**
     * Setup expectations: The search results of contact showed.
     * <p>
     * Selects the contact by the specific index to make a phone call.
     *
     * @param index The index of contact to select.
     */
    public void callContactbyIndex(int index);
}
