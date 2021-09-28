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


public interface IAutoDialHelper extends IAppHelper {

    /** enum class for contact list order type. */
    enum OrderType {
        FIRST_NAME,
        LAST_NAME
    }
    /** enum class for phone call audio channel. */
    enum AudioSource {
        PHONE,
        CAR_SPEAKERS,
    }

    /**
     * Setup expectations: The app is open and the dialpad is open
     *
     * <p>This method is used to dial the phonenumber on dialpad
     *
     * @param phoneNumber phone number to dial.
     */
    void dialANumber(String phoneNumber);

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to end call using softkey.
     */
    void endCall();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to open call history details.
     */
    void openCallHistory();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method dials a contact from the contact list.
     *
     * @param contactName to dial.
     */
    void callContact(String contactName);

    /**
     * Setup expectations: The app is open and in Dialpad.
     *
     * <p>This method is used to delete the number entered on dialpad using backspace
     */
    void deleteDialedNumber();

    /**
     * Setup expectations: The app is open and in Dialpad
     *
     * <p>This method is used to get the number entered on dialing screen.
     */
    String getDialedNumber();

    /**
     * Setup expectations: The app is open and in Dialpad
     *
     * <p>This method is used to get the number entered on dialpad
     */
    String getDialInNumber();

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to get the name of the contact for the ongoing call
     */
    String getDialedContactName();

    /**
     * Setup expectations: The app is open and Call History is open.
     *
     * <p>This method is used to get the most recent call history.
     */
    String getRecentCallHistory();

    /**
     * Setup expectations: The app is open and phonenumber is entered on the dialpad.
     *
     * <p>This method is used to make/receive a call using softkey.
     */
    void makeCall();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to dial a number from a list (Favorites, Call History, Contact).
     *
     * @param contact (number or name) dial.
     */
    void dialFromList(String contact);

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to enter number on the in-call dialpad.
     *
     * @param phoneNumber to dial.
     */
    void inCallDialPad(String phoneNumber);

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to mute the ongoing call.
     */
    void muteCall();

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to unmute the ongoing call.
     */
    void unmuteCall();

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to change voice channel to handset/bluetooth.
     *
     * @param source to switch to.
     */
    void changeAudioSource(AudioSource source);

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to make a call to the first history from Call History.
     */
    void callMostRecentHistory();

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * <p>This method is used to get the contact name being called.
     */
    String getContactName();

    /**
     * Setup expectations: The app is open and there is an ongoing call
     *
     * <p>This method is used to get the contact type (Mobile, Work, Home and etc.) being called.
     */
    String getContactType();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to search a contact in the contact list.
     *
     * @param contact to search.
     */
    void searchContactsByName(String contact);

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to search a number in the contact list.
     *
     * @param number to search.
     */
    void searchContactsByNumber(String number);

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to get the first search result for contact.
     */
    String getFirstSearchResult();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to order the contact list based on first/last name.
     *
     * @param orderType to use.
     */
    void sortContactListBy(OrderType orderType);

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to get the first contact name from contact list.
     */
    String getFirstContactFromContactList();

    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to verify if a contact is added to Favorites.
     *
     * @param contact to check.
     */
    boolean isContactInFavorites(String contact);

    /**
     * Setup expectations: The contact's details page is open.
     *
     * <p>This method is used to close the details page.
     *
     * @param contact Contact's details page to be opened.
     */
    void openDetailsPage(String contact);
    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to check if phone is paired.
     */
    boolean isPhonePaired();
    /**
     * Setup expectations: The app is open.
     *
     * <p>This method is used to open contact list
     */
    void openContacts();

    /**
     * This method is used to open the Phone App
     *
     * @param No parameters.
     */
    void OpenPhoneApp();
}
