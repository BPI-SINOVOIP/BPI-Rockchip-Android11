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
 * limitations under the License
 */

package com.android.inputmethod.leanback;

import java.util.Locale;

public class LeanbackLocales {

    /*
     * -Locales are organized into groups based on keyboard layout (e.g. qwerty, qwertz, azerty)
     * -In each group, the most specific layouts (those that specify language AND country)
     *  are listed first
     * -The list of locales are named as such: <keyboard layout>_<country|language>_zone
     *   (Note: the naming of the list is made as generic as possible, for example if there is only
     *    one list for a given keyboard layout then the country, language and zone is omitted)
     *   (Note: country is preferred over language because it is more specific, if no country is
     *    specified, use language)
     *   (Note: zone refers to US vs EU, which specifies which type of currency to use
     *    dollar, euro, or pound)
     */

    /**************************** QWERTY ****************************/
    // QWERTY (EN GB)
    public static final Locale BRITISH_ENGLISH = new Locale("en", "GB");
    public static final Locale[] QWERTY_GB = new Locale[] {BRITISH_ENGLISH};

    // QWERTY (EN IN)
    public static final Locale INDIAN_ENGLISH = new Locale("en", "IN");
    public static final Locale[] QWERTY_IN = new Locale[] {INDIAN_ENGLISH};

    // QWERTY (ES EU)
    public static final Locale SPAIN_SPANISH = new Locale("es", "ES");
    public static final Locale GALIC_SPANISH = new Locale("gl", "ES");
    public static final Locale BASQUE_SPANISH = new Locale("eu", "ES");
    public static final Locale[] QWERTY_ES_EU = new Locale[] {
        SPAIN_SPANISH, GALIC_SPANISH, BASQUE_SPANISH};

    // QWERTY (ES US)
    public static final Locale OTHER_SPANISH = new Locale("es", "");
    public static final Locale[] QWERTY_ES_US = new Locale[] {OTHER_SPANISH};

    // QWERTY (AZ)
    public static final Locale AZERBAIJANI = new Locale("az", "");
    public static final Locale[] QWERTY_AZ = new Locale[] {AZERBAIJANI};

    // QWERTY (CA)
    public static final Locale CATALAN = new Locale("ca", "");
    public static final Locale[] QWERTY_CA = new Locale[] {CATALAN};

    // QWERTY (DA)
    public static final Locale DANISH = new Locale("da", "");
    public static final Locale[] QWERTY_DA = new Locale[] {DANISH};

    // QWERTY (ET)
    public static final Locale ESTONIAN = new Locale("et", "");
    public static final Locale[] QWERTY_ET = new Locale[] {ESTONIAN};

    // QWERTY (FI)
    public static final Locale FINNISH = new Locale("fi", "");
    public static final Locale[] QWERTY_FI = new Locale[] {FINNISH};

    // QWERTY (NB)
    public static final Locale NORWEGIAN = new Locale("nb", "");
    public static final Locale[] QWERTY_NB = new Locale[] {NORWEGIAN};

    // QWERTY (SV)
    public static final Locale SWEDISH = new Locale("sv", "");
    public static final Locale[] QWERTY_SV = new Locale[] {SWEDISH};

    // QWERTY (US)
    public static final Locale ENGLISH = Locale.ENGLISH;
    public static final Locale CANADIAN_FRENCH = Locale.CANADA_FRENCH;
    public static final Locale[] QWERTY_US = new Locale[] {ENGLISH, CANADIAN_FRENCH};


    /**************************** QWERTZ ****************************/

    // QWERTZ (CH)
    public static final Locale SWISS_GERMAN = new Locale("de", "CH");
    public static final Locale SWISS_ITALIAN = new Locale("it", "CH");
    public static final Locale[] QWERTZ_CH = new Locale[] { SWISS_GERMAN, SWISS_ITALIAN};

    // QWERTZ
    public static final Locale GERMAN = new Locale("de", "");
    public static final Locale CROATIAN = new Locale("hr", "");
    public static final Locale CZECH = new Locale("cs", "");
    public static final Locale SWISS_FRENCH = new Locale("fr", "CH");
    public static final Locale HUNGARIAN = new Locale("hu", "");
    public static final Locale SERBIAN = new Locale("sr", "");
    public static final Locale SLOVENIAN = new Locale("sl", "");
    public static final Locale ALBANIANIAN = new Locale("sq", "");
    public static final Locale[] QWERTZ = new Locale[] { GERMAN, CROATIAN, CZECH, SWISS_FRENCH,
        SWISS_ITALIAN, HUNGARIAN,SERBIAN, SLOVENIAN, ALBANIANIAN};


    /**************************** AZERTY ****************************/

    // AZERTY
    public static final Locale FRENCH = Locale.FRENCH;
    public static final Locale BELGIAN_DUTCH = new Locale("nl", "BE");
    public static final Locale[] AZERTY = new Locale[] {FRENCH, BELGIAN_DUTCH};

}
