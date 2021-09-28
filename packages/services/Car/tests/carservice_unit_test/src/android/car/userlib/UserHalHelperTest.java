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

package android.car.userlib;

import static android.car.userlib.UserHalHelper.CREATE_USER_PROPERTY;
import static android.car.userlib.UserHalHelper.REMOVE_USER_PROPERTY;
import static android.car.userlib.UserHalHelper.SWITCH_USER_PROPERTY;
import static android.car.userlib.UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.ASSOCIATE_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.DISASSOCIATE_ALL_USERS;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationSetValue.DISASSOCIATE_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_1;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_2;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_3;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.CUSTOM_4;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType.KEY_FOB;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue.ASSOCIATED_ANOTHER_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue.ASSOCIATED_CURRENT_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue.NOT_ASSOCIATED_ANY_USER;
import static android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue.UNKNOWN;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.app.ActivityManager;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.car.test.mocks.AbstractExtendedMockitoTestCase.CustomMockitoSessionBuilder;
import android.car.test.mocks.AndroidMockitoHelper;
import android.car.test.util.UserTestingHelper.UserInfoBuilder;
import android.content.pm.UserInfo;
import android.hardware.automotive.vehicle.V2_0.CreateUserRequest;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoRequestType;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponse;
import android.hardware.automotive.vehicle.V2_0.InitialUserInfoResponseAction;
import android.hardware.automotive.vehicle.V2_0.RemoveUserRequest;
import android.hardware.automotive.vehicle.V2_0.SwitchUserMessageType;
import android.hardware.automotive.vehicle.V2_0.SwitchUserRequest;
import android.hardware.automotive.vehicle.V2_0.UserFlags;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationType;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationAssociationValue;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationGetRequest;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationResponse;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetAssociation;
import android.hardware.automotive.vehicle.V2_0.UserIdentificationSetRequest;
import android.hardware.automotive.vehicle.V2_0.UsersInfo;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;

import com.google.common.collect.Range;

import org.junit.Test;
import org.mockito.Mock;

import java.util.ArrayList;
import java.util.List;

public final class UserHalHelperTest extends AbstractExtendedMockitoTestCase {

    @Mock
    private UserManager mUm;

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(ActivityManager.class);
    }

    @Test
    public void testHalCallbackStatusToString() {
        assertThat(UserHalHelper.halCallbackStatusToString(-666)).isNotNull();
    }

    @Test
    public void testParseInitialUserInfoRequestType_valid() {
        assertThat(UserHalHelper.parseInitialUserInfoRequestType("FIRST_BOOT"))
                .isEqualTo(InitialUserInfoRequestType.FIRST_BOOT);
        assertThat(UserHalHelper.parseInitialUserInfoRequestType("COLD_BOOT"))
            .isEqualTo(InitialUserInfoRequestType.COLD_BOOT);
        assertThat(UserHalHelper.parseInitialUserInfoRequestType("FIRST_BOOT_AFTER_OTA"))
            .isEqualTo(InitialUserInfoRequestType.FIRST_BOOT_AFTER_OTA);
        assertThat(UserHalHelper.parseInitialUserInfoRequestType("RESUME"))
            .isEqualTo(InitialUserInfoRequestType.RESUME);
    }

    @Test
    public void testParseInitialUserInfoRequestType_unknown() {
        assertThat(UserHalHelper.parseInitialUserInfoRequestType("666")).isEqualTo(666);
    }

    @Test
    public void testParseInitialUserInfoRequestType_invalid() {
        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.parseInitialUserInfoRequestType("NumberNotIAm"));
    }

    @Test
    public void testConvertFlags_nullUser() {
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.convertFlags(null));
    }

    @Test
    public void testConvertFlags() {
        UserInfo user = new UserInfo();

        user.id = UserHandle.USER_SYSTEM;
        assertConvertFlags(UserFlags.SYSTEM, user);

        user.id = 10;
        assertConvertFlags(UserFlags.NONE, user);

        user.flags = UserInfo.FLAG_ADMIN;
        assertThat(user.isAdmin()).isTrue(); // sanity check
        assertConvertFlags(UserFlags.ADMIN, user);

        user.flags = UserInfo.FLAG_EPHEMERAL;
        assertThat(user.isEphemeral()).isTrue(); // sanity check
        assertConvertFlags(UserFlags.EPHEMERAL, user);

        user.userType = UserManager.USER_TYPE_FULL_GUEST;
        assertThat(user.isEphemeral()).isTrue(); // sanity check
        assertThat(user.isGuest()).isTrue(); // sanity check
        assertConvertFlags(UserFlags.GUEST | UserFlags.EPHEMERAL, user);
    }

    @Test
    public void testGetFlags_nullUserManager() {
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.getFlags(null, 10));
    }

    @Test
    public void testGetFlags_noUser() {
        // No need to set anythin as mUm call will return null
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.getFlags(mUm, 10));
    }

    @Test
    public void testGetFlags_ok() {
        UserInfo user = new UserInfo();

        user.id = UserHandle.USER_SYSTEM;
        assertGetFlags(UserFlags.SYSTEM, user);

        user.id = 10;
        assertGetFlags(UserFlags.NONE, user);

        user.flags = UserInfo.FLAG_ADMIN;
        assertThat(user.isAdmin()).isTrue(); // sanity check
        assertGetFlags(UserFlags.ADMIN, user);

        user.flags = UserInfo.FLAG_EPHEMERAL;
        assertThat(user.isEphemeral()).isTrue(); // sanity check
        assertGetFlags(UserFlags.EPHEMERAL, user);

        user.userType = UserManager.USER_TYPE_FULL_GUEST;
        assertThat(user.isEphemeral()).isTrue(); // sanity check
        assertThat(user.isGuest()).isTrue(); // sanity check
        assertGetFlags(UserFlags.GUEST | UserFlags.EPHEMERAL, user);
    }

    @Test
    public void testIsSystem() {
        assertThat(UserHalHelper.isSystem(UserFlags.SYSTEM)).isTrue();
        assertThat(UserHalHelper.isSystem(UserFlags.SYSTEM | 666)).isTrue();
        assertThat(UserHalHelper.isSystem(UserFlags.GUEST)).isFalse();
    }

    @Test
    public void testIsGuest() {
        assertThat(UserHalHelper.isGuest(UserFlags.GUEST)).isTrue();
        assertThat(UserHalHelper.isGuest(UserFlags.GUEST | 666)).isTrue();
        assertThat(UserHalHelper.isGuest(UserFlags.SYSTEM)).isFalse();
    }

    @Test
    public void testIsEphemeral() {
        assertThat(UserHalHelper.isEphemeral(UserFlags.EPHEMERAL)).isTrue();
        assertThat(UserHalHelper.isEphemeral(UserFlags.EPHEMERAL | 666)).isTrue();
        assertThat(UserHalHelper.isEphemeral(UserFlags.GUEST)).isFalse();
    }

    @Test
    public void testIsAdmin() {
        assertThat(UserHalHelper.isAdmin(UserFlags.ADMIN)).isTrue();
        assertThat(UserHalHelper.isAdmin(UserFlags.ADMIN | 666)).isTrue();
        assertThat(UserHalHelper.isAdmin(UserFlags.GUEST)).isFalse();
    }

    @Test
    public void testIsDisabled() {
        assertThat(UserHalHelper.isDisabled(UserFlags.DISABLED)).isTrue();
        assertThat(UserHalHelper.isDisabled(UserFlags.DISABLED | 666)).isTrue();
        assertThat(UserHalHelper.isDisabled(UserFlags.GUEST)).isFalse();
    }

    @Test
    public void testIsProfile() {
        assertThat(UserHalHelper.isProfile(UserFlags.PROFILE)).isTrue();
        assertThat(UserHalHelper.isProfile(UserFlags.PROFILE | 666)).isTrue();
        assertThat(UserHalHelper.isProfile(UserFlags.GUEST)).isFalse();
    }

    @Test
    public void testToUserInfoFlags() {
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.NONE)).isEqualTo(0);
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.EPHEMERAL))
                .isEqualTo(UserInfo.FLAG_EPHEMERAL);
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.ADMIN))
                .isEqualTo(UserInfo.FLAG_ADMIN);
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.EPHEMERAL | UserFlags.ADMIN))
                .isEqualTo(UserInfo.FLAG_EPHEMERAL | UserInfo.FLAG_ADMIN);

        // test flags that should be ignored
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.SYSTEM)).isEqualTo(0);
        assertThat(UserHalHelper.toUserInfoFlags(UserFlags.GUEST)).isEqualTo(0);
        assertThat(UserHalHelper.toUserInfoFlags(1024)).isEqualTo(0);
    }

    private void assertConvertFlags(int expectedFlags, @NonNull UserInfo user) {
        assertWithMessage("flags mismatch: user=%s, flags=%s",
                user.toFullString(), UserHalHelper.userFlagsToString(expectedFlags))
                        .that(UserHalHelper.convertFlags(user)).isEqualTo(expectedFlags);
    }

    private void assertGetFlags(int expectedFlags, @NonNull UserInfo user) {
        when(mUm.getUserInfo(user.id)).thenReturn(user);
        assertWithMessage("flags mismatch: user=%s, flags=%s",
                user.toFullString(), UserHalHelper.userFlagsToString(expectedFlags))
                        .that(UserHalHelper.getFlags(mUm, user.id)).isEqualTo(expectedFlags);
    }

    @Test
    public void testUserFlagsToString() {
        assertThat(UserHalHelper.userFlagsToString(-666)).isNotNull();
    }

    @Test
    public void testCreatePropRequest_withType() {
        int prop = 1;
        int requestId = 2;
        int requestType = 3;
        long before = SystemClock.elapsedRealtime();
        VehiclePropValue propRequest = UserHalHelper.createPropRequest(prop, requestId,
                requestType);
        long after = SystemClock.elapsedRealtime();

        assertThat(propRequest.value.int32Values)
                .containsExactly(requestId, requestType)
                .inOrder();
        assertThat(propRequest.prop).isEqualTo(prop);
        assertThat(propRequest.timestamp).isIn(Range.closed(before, after));
    }

    @Test
    public void testCreatePropRequest() {
        int prop = 1;
        int requestId = 2;
        long before = SystemClock.elapsedRealtime();
        VehiclePropValue propRequest = UserHalHelper.createPropRequest(prop, requestId);
        long after = SystemClock.elapsedRealtime();

        assertThat(propRequest.value.int32Values)
                .containsExactly(requestId)
                .inOrder();
        assertThat(propRequest.prop).isEqualTo(prop);
        assertThat(propRequest.timestamp).isIn(Range.closed(before, after));
    }

    @Test
    public void testAddUsersInfo_nullProp() {
        UsersInfo infos = new UsersInfo();

        assertThrows(NullPointerException.class, () -> UserHalHelper.addUsersInfo(null, infos));
    }

    @Test
    public void testAddUsersInfo_nullCurrentUser() {
        VehiclePropValue propRequest = new VehiclePropValue();

        UsersInfo infos = new UsersInfo();
        infos.currentUser = null;
        assertThrows(NullPointerException.class, () ->
                UserHalHelper.addUsersInfo(propRequest, infos));
    }

    @Test
    public void testAddUsersInfo_mismatchNumberUsers() {
        VehiclePropValue propRequest = new VehiclePropValue();

        UsersInfo infos = new UsersInfo();
        infos.currentUser.userId = 42;
        infos.currentUser.flags = 1;
        infos.numberUsers = 1;
        assertThat(infos.existingUsers).isEmpty();
        assertThrows(IllegalArgumentException.class, () ->
                UserHalHelper.addUsersInfo(propRequest, infos));
    }

    @Test
    public void testAddUsersInfo_success() {
        VehiclePropValue propRequest = new VehiclePropValue();
        propRequest.value.int32Values.add(99);

        UsersInfo infos = new UsersInfo();
        infos.currentUser.userId = 42;
        infos.currentUser.flags = 1;
        infos.numberUsers = 1;

        android.hardware.automotive.vehicle.V2_0.UserInfo userInfo =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        userInfo.userId = 43;
        userInfo.flags = 1;
        infos.existingUsers.add(userInfo);
        UserHalHelper.addUsersInfo(propRequest, infos);

        assertThat(propRequest.value.int32Values)
                .containsExactly(99, 42, 1, 1, 43, 1)
                .inOrder();
    }

    @Test
    public void testAddUserInfo_nullProp() {
        android.hardware.automotive.vehicle.V2_0.UserInfo userInfo =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();

        assertThrows(NullPointerException.class, () -> UserHalHelper.addUserInfo(null, userInfo));
    }

    @Test
    public void testAddUserInfo_nullCurrentUser() {
        VehiclePropValue prop = new VehiclePropValue();

        assertThrows(NullPointerException.class, () -> UserHalHelper.addUserInfo(prop, null));
    }

    @Test
    public void testAddUserInfo_success() {
        VehiclePropValue propRequest = new VehiclePropValue();
        propRequest.value.int32Values.add(99);

        android.hardware.automotive.vehicle.V2_0.UserInfo userInfo =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        userInfo.userId = 42;
        userInfo.flags = 1;

        UserHalHelper.addUserInfo(propRequest, userInfo);

        assertThat(propRequest.value.int32Values)
                .containsExactly(99, 42, 1)
                .inOrder();
    }

    @Test
    public void testIsValidUserIdentificationAssociationType_valid() {
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(KEY_FOB)).isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(CUSTOM_1)).isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(CUSTOM_2)).isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(CUSTOM_3)).isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(CUSTOM_4)).isTrue();
    }

    @Test
    public void testIsValidUserIdentificationAssociationType_invalid() {
        assertThat(UserHalHelper.isValidUserIdentificationAssociationType(CUSTOM_4 + 1)).isFalse();
    }

    @Test
    public void testIsValidUserIdentificationAssociationValue_valid() {
        assertThat(UserHalHelper.isValidUserIdentificationAssociationValue(ASSOCIATED_ANOTHER_USER))
                .isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationValue(ASSOCIATED_CURRENT_USER))
                .isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationValue(NOT_ASSOCIATED_ANY_USER))
                .isTrue();
        assertThat(UserHalHelper.isValidUserIdentificationAssociationValue(UNKNOWN)).isTrue();
    }

    @Test
    public void testIsValidUserIdentificationAssociationValue_invalid() {
        assertThat(UserHalHelper.isValidUserIdentificationAssociationValue(0)).isFalse();
    }

    @Test
    public void testIsValidUserIdentificationAssociationSetValue_valid() {
        assertThat(UserHalHelper
                .isValidUserIdentificationAssociationSetValue(ASSOCIATE_CURRENT_USER)).isTrue();
        assertThat(UserHalHelper
                .isValidUserIdentificationAssociationSetValue(DISASSOCIATE_CURRENT_USER)).isTrue();
        assertThat(UserHalHelper
                .isValidUserIdentificationAssociationSetValue(DISASSOCIATE_ALL_USERS)).isTrue();
    }

    @Test
    public void testIsValidUserIdentificationAssociationSetValue_invalid() {
        assertThat(UserHalHelper.isValidUserIdentificationAssociationSetValue(0)).isFalse();
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toVehiclePropValue((UserIdentificationGetRequest) null));
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_emptyRequest() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_wrongNumberOfAssociations() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.numberAssociationTypes = 1;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_invalidType() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.numberAssociationTypes = 1;
        request.associationTypes.add(CUSTOM_4 + 1);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_missingRequestId() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.userInfo.userId = 42;
        request.userInfo.flags = 108;
        request.numberAssociationTypes = 1;
        request.associationTypes.add(KEY_FOB);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationGetRequestToVehiclePropValue_ok() {
        UserIdentificationGetRequest request = new UserIdentificationGetRequest();
        request.requestId = 1;
        request.userInfo.userId = 42;
        request.userInfo.flags = 108;
        request.numberAssociationTypes = 2;
        request.associationTypes.add(KEY_FOB);
        request.associationTypes.add(CUSTOM_1);

        VehiclePropValue propValue = UserHalHelper.toVehiclePropValue(request);
        assertWithMessage("wrong prop on %s", propValue).that(propValue.prop)
                .isEqualTo(USER_IDENTIFICATION_ASSOCIATION_PROPERTY);
        assertWithMessage("wrong int32values on %s", propValue).that(propValue.value.int32Values)
                .containsExactly(1, 42, 108, 2, KEY_FOB, CUSTOM_1).inOrder();
    }

    @Test
    public void testToUserIdentificationResponse_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toUserIdentificationResponse(null));
    }

    @Test
    public void testToUserIdentificationResponse_invalidPropType() {
        VehiclePropValue prop = new VehiclePropValue();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toUserIdentificationResponse(prop));
    }

    @Test
    public void testToUserIdentificationResponse_invalidSize() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
        // need at least 4: request_id, number associations, type1, value1
        prop.value.int32Values.add(1);
        prop.value.int32Values.add(2);
        prop.value.int32Values.add(3);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toUserIdentificationResponse(prop));
    }

    @Test
    public void testToUserIdentificationResponse_invalidRequest() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
        prop.value.int32Values.add(0);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toUserIdentificationResponse(prop));
    }

    @Test
    public void testToUserIdentificationResponse_invalidType() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(1); // number of associations
        prop.value.int32Values.add(CUSTOM_4 + 1);
        prop.value.int32Values.add(ASSOCIATED_ANOTHER_USER);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toUserIdentificationResponse(prop));
    }

    @Test
    public void testToUserIdentificationResponse_invalidValue() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(1); // number of associations
        prop.value.int32Values.add(KEY_FOB);
        prop.value.int32Values.add(0);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toUserIdentificationResponse(prop));
    }

    @Test
    public void testToUserIdentificationResponse_ok() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.USER_IDENTIFICATION_ASSOCIATION_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(3); // number of associations
        prop.value.int32Values.add(KEY_FOB);
        prop.value.int32Values.add(ASSOCIATED_ANOTHER_USER);
        prop.value.int32Values.add(CUSTOM_1);
        prop.value.int32Values.add(ASSOCIATED_CURRENT_USER);
        prop.value.int32Values.add(CUSTOM_2);
        prop.value.int32Values.add(NOT_ASSOCIATED_ANY_USER);
        prop.value.stringValue = "D'OH!";

        UserIdentificationResponse response = UserHalHelper.toUserIdentificationResponse(prop);

        assertWithMessage("Wrong request id on %s", response)
            .that(response.requestId).isEqualTo(42);
        assertWithMessage("Wrong number of associations on %s", response)
            .that(response.numberAssociation).isEqualTo(3);
        assertAssociation(response, 0, KEY_FOB, ASSOCIATED_ANOTHER_USER);
        assertAssociation(response, 1, CUSTOM_1, ASSOCIATED_CURRENT_USER);
        assertAssociation(response, 2, CUSTOM_2, NOT_ASSOCIATED_ANY_USER);
        assertWithMessage("Wrong error message on %s", response)
            .that(response.errorMessage).isEqualTo("D'OH!");
    }

    @Test
    public void testToInitialUserInfoResponse_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(null));
    }

    @Test
    public void testToInitialUserInfoResponse_invalidPropType() {
        VehiclePropValue prop = new VehiclePropValue();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_invalidSize() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        //      need at least 2: request_id, action_type
        prop.value.int32Values.add(42);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_invalidRequest() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(0);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_invalidAction() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(-1); // InitialUserInfoResponseAction

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_default_ok_noStringValue() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.DEFAULT);

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.DEFAULT);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEmpty();
    }

    @Test
    public void testToInitialUserInfoResponse_default_ok_stringValueWithJustSeparator() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.DEFAULT);
        prop.value.stringValue = "||";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.DEFAULT);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEmpty();
    }

    @Test
    public void testToInitialUserInfoResponse_default_ok_stringValueWithLocale() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.DEFAULT);
        prop.value.stringValue = "esperanto,klingon";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.DEFAULT);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEqualTo("esperanto,klingon");
    }

    @Test
    public void testToInitialUserInfoResponse_default_ok_stringValueWithLocaleWithHalfSeparator() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.DEFAULT);
        prop.value.stringValue = "esperanto|klingon";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.DEFAULT);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEqualTo("esperanto|klingon");
    }

    @Test
    public void testToInitialUserInfoResponse_switch_missingUserId() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.SWITCH);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_switch_ok_noLocale() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.SWITCH);
        prop.value.int32Values.add(108); // user id
        prop.value.int32Values.add(666); // flags - should be ignored

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.SWITCH);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(108);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEmpty();
    }

    @Test
    public void testToInitialUserInfoResponse_switch_ok_withLocale() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.SWITCH);
        prop.value.int32Values.add(108); // user id
        prop.value.int32Values.add(666); // flags - should be ignored
        // add some extra | to make sure they're ignored
        prop.value.stringValue = "esperanto,klingon|||";
        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.SWITCH);
        assertThat(response.userNameToCreate).isEmpty();
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(108);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.NONE);
        assertThat(response.userLocales).isEqualTo("esperanto,klingon");
    }

    @Test
    public void testToInitialUserInfoResponse_create_missingUserId() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.CREATE);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_create_missingFlags() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.CREATE);
        prop.value.int32Values.add(108); // user id

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toInitialUserInfoResponse(prop));
    }

    @Test
    public void testToInitialUserInfoResponse_create_ok_noLocale() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.CREATE);
        prop.value.int32Values.add(666); // user id - not used
        prop.value.int32Values.add(UserFlags.GUEST);
        prop.value.stringValue = "||ElGuesto";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.CREATE);
        assertThat(response.userNameToCreate).isEqualTo("ElGuesto");
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.GUEST);
        assertThat(response.userLocales).isEmpty();
    }

    @Test
    public void testToInitialUserInfoResponse_create_ok_withLocale() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.CREATE);
        prop.value.int32Values.add(666); // user id - not used
        prop.value.int32Values.add(UserFlags.GUEST);
        prop.value.stringValue = "esperanto,klingon||ElGuesto";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.CREATE);
        assertThat(response.userNameToCreate).isEqualTo("ElGuesto");
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.GUEST);
        assertThat(response.userLocales).isEqualTo("esperanto,klingon");
    }

    @Test
    public void testToInitialUserInfoResponse_create_ok_nameAndLocaleWithHalfDelimiter() {
        VehiclePropValue prop = new VehiclePropValue();
        prop.prop = UserHalHelper.INITIAL_USER_INFO_PROPERTY;
        prop.value.int32Values.add(42); // request id
        prop.value.int32Values.add(InitialUserInfoResponseAction.CREATE);
        prop.value.int32Values.add(666); // user id - not used
        prop.value.int32Values.add(UserFlags.GUEST);
        prop.value.stringValue = "esperanto|klingon||El|Guesto";

        InitialUserInfoResponse response = UserHalHelper.toInitialUserInfoResponse(prop);

        assertThat(response).isNotNull();
        assertThat(response.requestId).isEqualTo(42);
        assertThat(response.action).isEqualTo(InitialUserInfoResponseAction.CREATE);
        assertThat(response.userNameToCreate).isEqualTo("El|Guesto");
        assertThat(response.userToSwitchOrCreate.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(response.userToSwitchOrCreate.flags).isEqualTo(UserFlags.GUEST);
        assertThat(response.userLocales).isEqualTo("esperanto|klingon");
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toVehiclePropValue((UserIdentificationSetRequest) null));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_emptyRequest() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_wrongNumberOfAssociations() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.numberAssociations = 1;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_invalidType() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.numberAssociations = 1;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        request.associations.add(association1);
        association1.type = CUSTOM_4 + 1;
        association1.value = ASSOCIATE_CURRENT_USER;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_invalidValue() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.numberAssociations = 1;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        request.associations.add(association1);
        association1.type = KEY_FOB;
        association1.value = -1;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_missingRequestId() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.userInfo.userId = 42;
        request.userInfo.flags = 108;
        request.numberAssociations = 1;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        association1.type = KEY_FOB;
        association1.value = ASSOCIATE_CURRENT_USER;
        request.associations.add(association1);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testUserIdentificationSetRequestToVehiclePropValue_ok() {
        UserIdentificationSetRequest request = new UserIdentificationSetRequest();
        request.requestId = 1;
        request.userInfo.userId = 42;
        request.userInfo.flags = 108;
        request.numberAssociations = 2;
        UserIdentificationSetAssociation association1 = new UserIdentificationSetAssociation();
        association1.type = KEY_FOB;
        association1.value = ASSOCIATE_CURRENT_USER;
        request.associations.add(association1);
        UserIdentificationSetAssociation association2 = new UserIdentificationSetAssociation();
        association2.type = CUSTOM_1;
        association2.value = DISASSOCIATE_CURRENT_USER;
        request.associations.add(association2);

        VehiclePropValue propValue = UserHalHelper.toVehiclePropValue(request);
        assertWithMessage("wrong prop on %s", propValue).that(propValue.prop)
                .isEqualTo(USER_IDENTIFICATION_ASSOCIATION_PROPERTY);
        assertWithMessage("wrong int32values on %s", propValue).that(propValue.value.int32Values)
                .containsExactly(1, 42, 108, 2,
                        KEY_FOB, ASSOCIATE_CURRENT_USER,
                        CUSTOM_1, DISASSOCIATE_CURRENT_USER)
                .inOrder();
    }

    @Test
    public void testRemoveUserRequestToVehiclePropValue_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toVehiclePropValue((RemoveUserRequest) null));
    }

    @Test
    public void testRemoveUserRequestToVehiclePropValue_emptyRequest() {
        RemoveUserRequest request = new RemoveUserRequest();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testRemoveUserRequestToVehiclePropValue_missingRequestId() {
        RemoveUserRequest request = new RemoveUserRequest();
        request.removedUserInfo.userId = 11;
        request.usersInfo.existingUsers.add(request.removedUserInfo);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testRemoveUserRequestToVehiclePropValue_ok() {
        RemoveUserRequest request = new RemoveUserRequest();
        request.requestId = 42;

        android.hardware.automotive.vehicle.V2_0.UserInfo user10 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user10.userId = 10;
        user10.flags = UserFlags.ADMIN;

        // existing users
        request.usersInfo.numberUsers = 1;
        request.usersInfo.existingUsers.add(user10);

        // current user
        request.usersInfo.currentUser = user10;
        // user to remove
        request.removedUserInfo = user10;

        VehiclePropValue propValue = UserHalHelper.toVehiclePropValue(request);

        assertWithMessage("wrong prop on %s", propValue).that(propValue.prop)
                .isEqualTo(REMOVE_USER_PROPERTY);
        assertWithMessage("wrong int32values on %s", propValue).that(propValue.value.int32Values)
                .containsExactly(42, // request id
                        10, UserFlags.ADMIN, // user to remove
                        10, UserFlags.ADMIN, // current user
                        1, // number of users
                        10, UserFlags.ADMIN  // existing user 1
                        ).inOrder();
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toVehiclePropValue((CreateUserRequest) null));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_emptyRequest() {
        CreateUserRequest request = new CreateUserRequest();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_missingRequestId() {
        CreateUserRequest request = new CreateUserRequest();
        request.newUserInfo.userId = 10;
        request.usersInfo.existingUsers.add(request.newUserInfo);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_nullNewUserName() {
        CreateUserRequest request = new CreateUserRequest();
        request.requestId = 42;

        request.newUserInfo.userId = 10;
        request.newUserInfo.flags = UserFlags.ADMIN;
        request.newUserName = null;

        request.usersInfo.numberUsers = 1;
        request.usersInfo.currentUser.userId = request.newUserInfo.userId;
        request.usersInfo.currentUser.flags = request.newUserInfo.flags;
        request.usersInfo.existingUsers.add(request.usersInfo.currentUser);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_usersInfoDoesNotContainNewUser() {
        CreateUserRequest request = new CreateUserRequest();
        request.requestId = 42;
        request.newUserInfo.userId = 10;
        android.hardware.automotive.vehicle.V2_0.UserInfo user =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user.userId = 11;
        request.usersInfo.existingUsers.add(user);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_newUserFlagsMismatch() {
        CreateUserRequest request = new CreateUserRequest();
        request.requestId = 42;
        request.newUserInfo.userId = 10;
        request.newUserInfo.flags = UserFlags.ADMIN;
        android.hardware.automotive.vehicle.V2_0.UserInfo user =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user.userId = 10;
        request.newUserInfo.flags = UserFlags.SYSTEM;
        request.usersInfo.existingUsers.add(user);

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testCreateUserRequestToVehiclePropValue_ok() {
        CreateUserRequest request = new CreateUserRequest();
        request.requestId = 42;

        android.hardware.automotive.vehicle.V2_0.UserInfo user10 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user10.userId = 10;
        user10.flags = UserFlags.ADMIN;
        android.hardware.automotive.vehicle.V2_0.UserInfo user11 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user11.userId = 11;
        user11.flags = UserFlags.SYSTEM;
        android.hardware.automotive.vehicle.V2_0.UserInfo user12 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user12.userId = 12;
        user12.flags = UserFlags.GUEST;

        // existing users
        request.usersInfo.numberUsers = 3;
        request.usersInfo.existingUsers.add(user10);
        request.usersInfo.existingUsers.add(user11);
        request.usersInfo.existingUsers.add(user12);

        // current user
        request.usersInfo.currentUser.userId = 12;
        request.usersInfo.currentUser.flags = UserFlags.GUEST;

        // new user
        request.newUserInfo.userId = 10;
        request.newUserInfo.flags = UserFlags.ADMIN;
        request.newUserName = "Dude";


        VehiclePropValue propValue = UserHalHelper.toVehiclePropValue(request);

        assertWithMessage("wrong prop on %s", propValue).that(propValue.prop)
                .isEqualTo(CREATE_USER_PROPERTY);
        assertWithMessage("wrong int32values on %s", propValue).that(propValue.value.int32Values)
                .containsExactly(42, // request id
                        10, UserFlags.ADMIN, // new user
                        12, UserFlags.GUEST, // current user
                        3, // number of users
                        10, UserFlags.ADMIN,  // existing user 1
                        11, UserFlags.SYSTEM, // existing user 2
                        12, UserFlags.GUEST   // existing user 3
                        ).inOrder();
        assertWithMessage("wrong name %s", propValue).that(propValue.value.stringValue)
                .isEqualTo("Dude");
    }

    @Test
    public void testSwitchUserRequestToVehiclePropValue_null() {
        assertThrows(NullPointerException.class,
                () -> UserHalHelper.toVehiclePropValue((SwitchUserRequest) null));
    }

    @Test
    public void testSwitchUserRequestToVehiclePropValue_emptyRequest() {
        SwitchUserRequest request = new SwitchUserRequest();

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testSwitchUserRequestToVehiclePropValue_missingMessageType() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.requestId = 42;
        android.hardware.automotive.vehicle.V2_0.UserInfo user10 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user10.userId = 10;
        request.usersInfo.numberUsers = 1;
        request.usersInfo.existingUsers.add(user10);
        request.usersInfo.currentUser = user10;
        request.targetUser = user10;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void testSwitchUserRequestToVehiclePropValue_incorrectMessageType() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.requestId = 42;
        request.messageType = -1;
        android.hardware.automotive.vehicle.V2_0.UserInfo user10 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user10.userId = 10;
        request.usersInfo.numberUsers = 1;
        request.usersInfo.existingUsers.add(user10);
        request.usersInfo.currentUser = user10;
        request.targetUser = user10;

        assertThrows(IllegalArgumentException.class,
                () -> UserHalHelper.toVehiclePropValue(request));
    }

    @Test
    public void tesSwitchUserRequestToVehiclePropValue_ok() {
        SwitchUserRequest request = new SwitchUserRequest();
        request.requestId = 42;
        android.hardware.automotive.vehicle.V2_0.UserInfo user10 =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        user10.userId = 10;
        user10.flags = UserFlags.ADMIN;
        // existing users
        request.usersInfo.numberUsers = 1;
        request.usersInfo.existingUsers.add(user10);
        // current user
        request.usersInfo.currentUser = user10;
        // user to remove
        request.targetUser = user10;
        request.messageType = SwitchUserMessageType.ANDROID_SWITCH;

        VehiclePropValue propValue = UserHalHelper.toVehiclePropValue(request);

        assertWithMessage("wrong prop on %s", propValue).that(propValue.prop)
                .isEqualTo(SWITCH_USER_PROPERTY);
        assertWithMessage("wrong int32values on %s", propValue).that(propValue.value.int32Values)
                .containsExactly(42, // request id
                        SwitchUserMessageType.ANDROID_SWITCH, // message type
                        10, UserFlags.ADMIN, // target user
                        10, UserFlags.ADMIN, // current user
                        1, // number of users
                        10, UserFlags.ADMIN  // existing user 1
                        ).inOrder();
    }

    @Test
    public void testNewUsersInfo_nullUm() {
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.newUsersInfo(null, 100));
    }

    @Test
    public void testNewUsersInfo_nullUsers() {
        UsersInfo usersInfo = UserHalHelper.newUsersInfo(mUm, 100);

        assertEmptyUsersInfo(usersInfo);
    }

    @Test
    public void testNewUsersInfo_noUsers() {
        List<UserInfo> users = new ArrayList<>();
        AndroidMockitoHelper.mockUmGetUsers(mUm, users);

        UsersInfo usersInfo = UserHalHelper.newUsersInfo(mUm, 100);

        assertEmptyUsersInfo(usersInfo);
    }

    @Test
    public void testNewUsersInfo_ok() {
        UserInfo user100 = new UserInfoBuilder(100).setFlags(UserInfo.FLAG_ADMIN).build();
        UserInfo user200 = new UserInfoBuilder(200).build();

        AndroidMockitoHelper.mockUmGetUsers(mUm, user100, user200);
        AndroidMockitoHelper.mockAmGetCurrentUser(300); // just to make sure it's not used

        UsersInfo usersInfo = UserHalHelper.newUsersInfo(mUm, 100);

        assertThat(usersInfo).isNotNull();
        assertThat(usersInfo.currentUser.userId).isEqualTo(100);
        assertThat(usersInfo.currentUser.flags).isEqualTo(UserFlags.ADMIN);

        assertThat(usersInfo.numberUsers).isEqualTo(2);
        assertThat(usersInfo.existingUsers).hasSize(2);

        assertThat(usersInfo.existingUsers.get(0).userId).isEqualTo(100);
        assertThat(usersInfo.existingUsers.get(0).flags).isEqualTo(UserFlags.ADMIN);
        assertThat(usersInfo.existingUsers.get(1).userId).isEqualTo(200);
        assertThat(usersInfo.existingUsers.get(1).flags).isEqualTo(UserFlags.NONE);
    }

    @Test
    public void testNewUsersInfo_currentUser_ok() {
        UserInfo user100 = new UserInfoBuilder(100).setFlags(UserInfo.FLAG_ADMIN).build();
        UserInfo user200 = new UserInfoBuilder(200).build();

        AndroidMockitoHelper.mockUmGetUsers(mUm, user100, user200);
        AndroidMockitoHelper.mockAmGetCurrentUser(100);

        UsersInfo usersInfo = UserHalHelper.newUsersInfo(mUm);

        assertThat(usersInfo).isNotNull();
        assertThat(usersInfo.currentUser.userId).isEqualTo(100);
        assertThat(usersInfo.currentUser.flags).isEqualTo(UserFlags.ADMIN);

        assertThat(usersInfo.numberUsers).isEqualTo(2);
        assertThat(usersInfo.existingUsers).hasSize(2);

        assertThat(usersInfo.existingUsers.get(0).userId).isEqualTo(100);
        assertThat(usersInfo.existingUsers.get(0).flags).isEqualTo(UserFlags.ADMIN);
        assertThat(usersInfo.existingUsers.get(1).userId).isEqualTo(200);
        assertThat(usersInfo.existingUsers.get(1).flags).isEqualTo(UserFlags.NONE);
    }

    @Test
    @ExpectWtf
    public void testNewUsersInfo_noCurrentUser() {
        UserInfo user100 = new UserInfoBuilder(100).setFlags(UserInfo.FLAG_ADMIN).build();
        UserInfo user200 = new UserInfoBuilder(200).build();

        AndroidMockitoHelper.mockUmGetUsers(mUm, user100, user200);
        AndroidMockitoHelper.mockAmGetCurrentUser(300);

        UsersInfo usersInfo = UserHalHelper.newUsersInfo(mUm);

        assertThat(usersInfo).isNotNull();
        assertThat(usersInfo.currentUser.userId).isEqualTo(300);
        assertThat(usersInfo.currentUser.flags).isEqualTo(UserFlags.NONE);

        assertThat(usersInfo.numberUsers).isEqualTo(2);
        assertThat(usersInfo.existingUsers).hasSize(2);

        assertThat(usersInfo.existingUsers.get(0).userId).isEqualTo(100);
        assertThat(usersInfo.existingUsers.get(0).flags).isEqualTo(UserFlags.ADMIN);
        assertThat(usersInfo.existingUsers.get(1).userId).isEqualTo(200);
        assertThat(usersInfo.existingUsers.get(1).flags).isEqualTo(UserFlags.NONE);
    }

    @Test
    public void testCheckValidUsersInfo_null() {
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.checkValid(null));
    }

    @Test
    public void testCheckValidUsersInfo_empty() {
        UsersInfo usersInfo = new UsersInfo();
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.checkValid(usersInfo));
    }

    @Test
    public void testCheckValidUsersInfo_sizeMismatch() {
        UsersInfo usersInfo = new UsersInfo();
        usersInfo.numberUsers = 1;
        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.checkValid(usersInfo));
    }

    @Test
    public void testCheckValidUsersInfo_currentUserMissing() {
        UsersInfo usersInfo = new UsersInfo();
        usersInfo.numberUsers = 1;
        usersInfo.currentUser.userId = 10;
        usersInfo.existingUsers.add(new android.hardware.automotive.vehicle.V2_0.UserInfo());

        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.checkValid(usersInfo));
    }

    @Test
    public void testCheckValidUsersInfo_currentUserFlagsMismatch() {
        UsersInfo usersInfo = new UsersInfo();
        usersInfo.numberUsers = 1;
        usersInfo.currentUser.userId = 10;
        usersInfo.currentUser.flags = UserFlags.ADMIN;
        android.hardware.automotive.vehicle.V2_0.UserInfo currentUser =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        currentUser.userId = 10;
        currentUser.flags = UserFlags.SYSTEM;
        usersInfo.existingUsers.add(currentUser);

        assertThrows(IllegalArgumentException.class, () -> UserHalHelper.checkValid(usersInfo));
    }

    @Test
    public void testCheckValidUsersInfo_ok() {
        UsersInfo usersInfo = new UsersInfo();
        usersInfo.numberUsers = 1;
        usersInfo.currentUser.userId = 10;

        android.hardware.automotive.vehicle.V2_0.UserInfo currentUser =
                new android.hardware.automotive.vehicle.V2_0.UserInfo();
        currentUser.userId = 10;
        usersInfo.existingUsers.add(currentUser);

        UserHalHelper.checkValid(usersInfo);
    }

    private static void assertEmptyUsersInfo(UsersInfo usersInfo) {
        assertThat(usersInfo).isNotNull();
        assertThat(usersInfo.currentUser.userId).isEqualTo(UserHandle.USER_NULL);
        assertThat(usersInfo.currentUser.flags).isEqualTo(UserFlags.NONE);
        assertThat(usersInfo.numberUsers).isEqualTo(0);
        assertThat(usersInfo.existingUsers).isEmpty();
    }

    private static void assertAssociation(@NonNull UserIdentificationResponse response, int index,
            int expectedType, int expectedValue) {
        UserIdentificationAssociation actualAssociation = response.associations.get(index);
        if (actualAssociation.type != expectedType) {
            fail("Wrong type for association at index " + index + " on " + response + "; expected "
                    + UserIdentificationAssociationType.toString(expectedType) + ", got "
                    + UserIdentificationAssociationType.toString(actualAssociation.type));
        }
        if (actualAssociation.type != expectedType) {
            fail("Wrong value for association at index " + index + " on " + response + "; expected "
                    + UserIdentificationAssociationValue.toString(expectedValue) + ", got "
                    + UserIdentificationAssociationValue.toString(actualAssociation.value));
        }
    }
}
