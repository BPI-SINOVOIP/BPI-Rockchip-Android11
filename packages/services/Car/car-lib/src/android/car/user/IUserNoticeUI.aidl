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

package android.car.user;

import android.car.user.IUserNotice;

/**
 * Binder for CarUserNoticeService/CarService to pass IUserNotice binder to UserNotice UI.
 * UserNotice UI implements this binder.
 * @hide
*/
oneway interface IUserNoticeUI {
    /**
     * CarUserNoticeService will use this call to pass IUserNotice binder which can be used
     * to notify dismissal of UI dialog.
     */
    void setCallbackBinder(in IUserNotice binder);
}
