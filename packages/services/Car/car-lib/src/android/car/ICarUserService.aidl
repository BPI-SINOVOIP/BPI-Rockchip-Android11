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

package android.car;

import android.content.pm.UserInfo;
import android.car.user.UserCreationResult;
import android.car.user.UserRemovalResult;
import android.car.user.UserIdentificationAssociationResponse;
import android.car.user.UserSwitchResult;
import com.android.internal.infra.AndroidFuture;
import com.android.internal.os.IResultReceiver;

/** @hide */
interface ICarUserService {
    AndroidFuture<UserCreationResult> createDriver(@nullable String name, boolean admin);
    UserInfo createPassenger(@nullable String name, int driverId);
    void switchDriver(int driverId, in AndroidFuture<UserSwitchResult> receiver);
    void switchUser(int tagerUserId, int timeoutMs, in AndroidFuture<UserSwitchResult> receiver);
    void setUserSwitchUiCallback(in IResultReceiver callback);
    void createUser(@nullable String name, String userType, int flags, int timeoutMs,
      in AndroidFuture<UserCreationResult> receiver);
    UserRemovalResult removeUser(int userId);
    List<UserInfo> getAllDrivers();
    List<UserInfo> getPassengers(int driverId);
    boolean startPassenger(int passengerId, int zoneId);
    boolean stopPassenger(int passengerId);
    void setLifecycleListenerForUid(in IResultReceiver listener);
    void resetLifecycleListenerForUid();
    void getInitialUserInfo(int requestType, int timeoutMs, in IResultReceiver receiver);
    UserIdentificationAssociationResponse getUserIdentificationAssociation(in int[] types);
    void setUserIdentificationAssociation(int timeoutMs, in int[] types, in int[] values,
      in AndroidFuture<UserIdentificationAssociationResponse> result);
    boolean isUserHalUserAssociationSupported();
}
