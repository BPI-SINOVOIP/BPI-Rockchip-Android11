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

package com.android.car.apps.common;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;

/**
 * Helper methods for addresses and navigation.
 */
public class NavigationUtils {

    /**
     * Returns the search location intent.
     *
     * @param address should be a location as either a place name or address.
     */
    public static Intent getViewAddressIntent(Resources res, String address) {
        String formattedAddress = String.format(res.getString(R.string.config_address_uri_format),
                Uri.encode(address));
        Uri addressUri = Uri.parse(formattedAddress);
        return new Intent(Intent.ACTION_VIEW, addressUri);
    }

    /**
     * Returns the search location intent.
     *
     * @param address should be a location as either a place name or address.
     */
    public static Intent getViewAddressIntent(Context context, String address) {
        Resources resources = context.getResources();
        return getViewAddressIntent(resources, address);
    }

    /**
     * Returns the navigation intent.
     *
     * @param address should be a location as either a place name or address.
     */
    public static Intent getNavigationIntent(Resources res, String address) {
        String formattedAddress = String.format(
                res.getString(R.string.config_navigation_uri_format), Uri.encode(address));
        Uri addressUri = Uri.parse(formattedAddress);
        return new Intent(Intent.ACTION_VIEW, addressUri);
    }

    /**
     * Returns the navigation Intent.
     *
     * @param address should be a location as either a place name or address.
     */
    public static Intent getNavigationIntent(Context context, String address) {
        Resources resources = context.getResources();
        return getNavigationIntent(resources, address);
    }
}
