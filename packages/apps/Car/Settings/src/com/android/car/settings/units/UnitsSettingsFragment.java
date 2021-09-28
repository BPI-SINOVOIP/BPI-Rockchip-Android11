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

package com.android.car.settings.units;

import android.car.Car;
import android.car.VehiclePropertyIds;
import android.car.VehicleUnit;
import android.car.hardware.property.CarPropertyManager;
import android.content.Context;
import android.provider.SearchIndexableResource;

import androidx.annotation.LayoutRes;

import com.android.car.settings.R;
import com.android.car.settings.common.CarSettingActivities;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;
import com.android.settingslib.search.SearchIndexableRaw;

import java.util.ArrayList;
import java.util.List;

/** Fragment to host Units-related preferences. */
@SearchIndexable
public class UnitsSettingsFragment extends SettingsFragment {

    @Override
    @LayoutRes
    protected int getPreferenceScreenResId() {
        return R.xml.units_fragment;
    }

    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.units_fragment,
                    CarSettingActivities.UnitsSettingsActivity.class) {

                private CarPropertyManager mCarPropertyManager;

                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(Context context,
                        boolean enabled) {
                    return null;
                }

                @Override
                public List<SearchIndexableRaw> getRawDataToIndex(Context context,
                        boolean enabled) {
                    List<SearchIndexableRaw> rawData = new ArrayList<>();
                    Car car = Car.createCar(context);
                    mCarPropertyManager = (CarPropertyManager) car.getCarManager(
                            Car.PROPERTY_SERVICE);
                    if (mCarPropertyManager != null) {
                        boolean hasUnits = false;
                        if (isValidVehicleProperty(
                                VehiclePropertyIds.VEHICLE_SPEED_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_speed),
                                    context.getString(R.string.units_speed_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (isValidVehicleProperty(VehiclePropertyIds.DISTANCE_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_distance),
                                    context.getString(R.string.units_distance_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (isValidVehicleProperty(VehiclePropertyIds.FUEL_VOLUME_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_fuel_consumption),
                                    context.getString(R.string.units_fuel_consumption_title),
                                    context.getString(R.string.units_settings)));
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_volume),
                                    context.getString(R.string.units_volume_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (isValidVehicleProperty(VehiclePropertyIds.EV_BATTERY_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_energy_consumption),
                                    context.getString(R.string.units_energy_consumption_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (isValidVehicleProperty(
                                VehiclePropertyIds.HVAC_TEMPERATURE_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_temperature),
                                    context.getString(R.string.units_temperature_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (isValidVehicleProperty(
                                VehiclePropertyIds.TIRE_PRESSURE_DISPLAY_UNITS)) {
                            hasUnits = true;
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.pk_units_pressure),
                                    context.getString(R.string.units_pressure_title),
                                    context.getString(R.string.units_settings)));
                        }
                        if (hasUnits) {
                            rawData.add(createRawDataEntry(context,
                                    context.getString(R.string.psk_units),
                                    context.getString(R.string.units_settings),
                                    context.getString(R.string.units_settings)));
                        }
                    }
                    car.disconnect();
                    return rawData;
                }

                private boolean isValidVehicleProperty(int propertyId) {
                    return mCarPropertyManager.getIntProperty(propertyId, /* area= */ 0)
                            != VehicleUnit.SHOULD_NOT_USE;
                }
            };
}
