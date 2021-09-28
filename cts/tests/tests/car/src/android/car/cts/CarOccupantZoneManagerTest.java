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
package android.car.cts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static java.util.stream.Collectors.toList;

import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.os.Process;
import android.os.UserHandle;
import android.platform.test.annotations.RequiresDevice;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.Display;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class CarOccupantZoneManagerTest extends CarApiTestBase {

    private OccupantZoneInfo mDriverZoneInfo;

    private CarOccupantZoneManager mCarOccupantZoneManager;

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        mCarOccupantZoneManager =
                (CarOccupantZoneManager) getCar().getCarManager(Car.CAR_OCCUPANT_ZONE_SERVICE);

        // Retrieve the current driver zone info (disregarding the driver's seat - LHD or RHD).
        // Ensures there is only one driver zone info.
        List<OccupantZoneInfo> drivers =
                mCarOccupantZoneManager.getAllOccupantZones().stream().filter(
                        o -> o.occupantType == CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER).collect(
                        toList());
        assertWithMessage("One and only one driver zone info is expected per config")
                .that(drivers).hasSize(1);
        mDriverZoneInfo = drivers.get(0);
    }

    @Test
    public void testDriverUserIdMustBeCurrentUser() {
        int myUid = Process.myUid();
        assertWithMessage("Driver user id must correspond to current user id (Process.myUid: %s)",
                myUid).that(
                UserHandle.getUserId(myUid)).isEqualTo(
                mCarOccupantZoneManager.getUserForOccupant(mDriverZoneInfo));
    }

    @Test
    public void testExpectAtLeastDriverZoneExists() {
        assertWithMessage(
                "Driver zone is expected to exist. Make sure a driver zone is properly defined in"
                        + " config.xml/config_occupant_display_mapping")
                .that(mCarOccupantZoneManager.getAllOccupantZones())
                .contains(mDriverZoneInfo);
    }

    @Test
    public void testDriverHasMainDisplay() {
        assertWithMessage("Driver is expected to be associated with main display")
                .that(mCarOccupantZoneManager.getAllDisplaysForOccupant(mDriverZoneInfo))
                .contains(getDriverDisplay());
    }

    @Test
    public void testDriverDisplayIdIsDefaultDisplay() {
        assertThat(getDriverDisplay().getDisplayId()).isEqualTo(Display.DEFAULT_DISPLAY);
    }

    private Display getDriverDisplay() {
        Display driverDisplay =
                mCarOccupantZoneManager.getDisplayForOccupant(
                        mDriverZoneInfo, CarOccupantZoneManager.DISPLAY_TYPE_MAIN);
        assertWithMessage(
                "No display set for driver. Make sure a default display is set in"
                        + " config.xml/config_occupant_display_mapping")
                .that(driverDisplay)
                .isNotNull();
        return driverDisplay;
    }
}
