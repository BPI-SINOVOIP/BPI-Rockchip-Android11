/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.net.ipmemorystore;

import android.net.ipmemorystore.NetworkAttributesParcelable;
import android.net.ipmemorystore.StatusParcelable;

/** {@hide} */
oneway interface IOnNetworkAttributesRetrievedListener {
    /**
     * Network attributes were fetched for the specified L2 key. While the L2 key will never
     * be null, the attributes may be if no data is stored about this L2 key.
     */
     void onNetworkAttributesRetrieved(in StatusParcelable status, in String l2Key,
             in NetworkAttributesParcelable attributes);
}
