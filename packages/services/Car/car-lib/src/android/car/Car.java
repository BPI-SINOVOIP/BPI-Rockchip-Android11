/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static android.car.CarLibLog.TAG_CAR;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.annotation.SdkConstant;
import android.annotation.SdkConstant.SdkConstantType;
import android.annotation.SuppressLint;
import android.annotation.SystemApi;
import android.annotation.TestApi;
import android.app.Activity;
import android.app.Service;
import android.car.annotation.MandatoryFeature;
import android.car.annotation.OptionalFeature;
import android.car.cluster.CarInstrumentClusterManager;
import android.car.cluster.ClusterActivityState;
import android.car.content.pm.CarPackageManager;
import android.car.diagnostic.CarDiagnosticManager;
import android.car.drivingstate.CarDrivingStateManager;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.car.hardware.CarSensorManager;
import android.car.hardware.CarVendorExtensionManager;
import android.car.hardware.cabin.CarCabinManager;
import android.car.hardware.hvac.CarHvacManager;
import android.car.hardware.power.CarPowerManager;
import android.car.hardware.property.CarPropertyManager;
import android.car.hardware.property.ICarProperty;
import android.car.input.CarInputManager;
import android.car.media.CarAudioManager;
import android.car.media.CarMediaManager;
import android.car.navigation.CarNavigationStatusManager;
import android.car.occupantawareness.OccupantAwarenessManager;
import android.car.settings.CarConfigurationManager;
import android.car.storagemonitoring.CarStorageMonitoringManager;
import android.car.test.CarTestManagerBinderWrapper;
import android.car.trust.CarTrustAgentEnrollmentManager;
import android.car.user.CarUserManager;
import android.car.vms.VmsClientManager;
import android.car.vms.VmsSubscriberManager;
import android.car.watchdog.CarWatchdogManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.TransactionTooLargeException;
import android.os.UserHandle;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Objects;

/**
 *   Top level car API for embedded Android Auto deployments.
 *   This API works only for devices with {@link PackageManager#FEATURE_AUTOMOTIVE}
 *   Calling this API on a device with no such feature will lead to an exception.
 */
public final class Car {

    /**
     * Binder service name of car service registered to service manager.
     *
     * @hide
     */
    public static final String CAR_SERVICE_BINDER_SERVICE_NAME = "car_service";

    /**
     * This represents AndroidManifest meta-data to tell that {@code Activity} is optimized for
     * driving distraction.
     *
     * <p>Activities without this meta-data can be blocked while car is in moving / driving state.
     *
     * <p>Note that having this flag does not guarantee that the {@code Activity} will be always
     * allowed for all driving states.
     *
     * <p>For this meta-data, android:value can be {@code true} (=optimized) or {@code false}.
     *
     * <p>Example usage:
     * <xml><meta-data android:name="distractionOptimized" android:value="true"/></xml>
     */
    @SuppressLint("IntentName")
    public static final String META_DATA_DISTRACTION_OPTIMIZED = "distractionOptimized";

    /**
     * This represents AndroidManifest meta-data to tell that {@code Application} requires specific
     * car features to work.
     *
     * <p>Apps like launcher or installer app can use this information to filter out apps
     * not usable in a specific car. This meta-data is not necessary for mandatory features.
     *
     * <p>For this meta-data, android:value should contain the feature name string defined by
     * (@link android.car.annotation.OptionalFeature} or
     * {@link android.car.annotation.ExperimentalFeature} annotations.
     *
     * <p>Example usage:
     * <xml><meta-data android:name="requires-car-feature" android:value="diagnostic"/></xml>
     */
    @SuppressLint("IntentName")
    public static final String META_DATA_REQUIRES_CAR_FEATURE = "requires-car-feature";

    /**
     * Service name for {@link CarSensorManager}, to be used in {@link #getCarManager(String)}.
     *
     * @deprecated  {@link CarSensorManager} is deprecated. Use {@link CarPropertyManager} instead.
     */
    @MandatoryFeature
    @Deprecated
    public static final String SENSOR_SERVICE = "sensor";

    /** Service name for {@link CarInfoManager}, to be used in {@link #getCarManager(String)}. */
    @MandatoryFeature
    public static final String INFO_SERVICE = "info";

    /** Service name for {@link CarAppFocusManager}. */
    @MandatoryFeature
    public static final String APP_FOCUS_SERVICE = "app_focus";

    /** Service name for {@link CarPackageManager} */
    @MandatoryFeature
    public static final String PACKAGE_SERVICE = "package";

    /** Service name for {@link CarAudioManager} */
    @MandatoryFeature
    public static final String AUDIO_SERVICE = "audio";

    /** Service name for {@link CarNavigationStatusManager} */
    @OptionalFeature
    public static final String CAR_NAVIGATION_SERVICE = "car_navigation_service";

    /** Service name for {@link CarOccupantZoneManager} */
    @MandatoryFeature
    public static final String CAR_OCCUPANT_ZONE_SERVICE = "car_occupant_zone_service";

    /**
     * Service name for {@link CarUserManager}
     *
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    @TestApi
    public static final String CAR_USER_SERVICE = "car_user_service";

    /**
     * Service name for {@link CarInstrumentClusterManager}
     *
     * @deprecated CarInstrumentClusterManager is being deprecated
     * @hide
     */
    @MandatoryFeature
    @Deprecated
    public static final String CAR_INSTRUMENT_CLUSTER_SERVICE = "cluster_service";

    /**
     * Service name for {@link CarCabinManager}.
     *
     * @deprecated {@link CarCabinManager} is deprecated. Use {@link CarPropertyManager} instead.
     * @hide
     */
    @MandatoryFeature
    @Deprecated
    @SystemApi
    public static final String CABIN_SERVICE = "cabin";

    /**
     * @hide
     */
    @OptionalFeature
    @SystemApi
    public static final String DIAGNOSTIC_SERVICE = "diagnostic";

    /**
     * Service name for {@link CarHvacManager}
     * @deprecated {@link CarHvacManager} is deprecated. Use {@link CarPropertyManager} instead.
     * @hide
     */
    @MandatoryFeature
    @Deprecated
    @SystemApi
    public static final String HVAC_SERVICE = "hvac";

    /**
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String POWER_SERVICE = "power";

    /**
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String PROJECTION_SERVICE = "projection";

    /**
     * Service name for {@link CarPropertyManager}
     */
    @MandatoryFeature
    public static final String PROPERTY_SERVICE = "property";

    /**
     * Service name for {@link CarVendorExtensionManager}
     *
     * @deprecated {@link CarVendorExtensionManager} is deprecated.
     * Use {@link CarPropertyManager} instead.
     * @hide
     */
    @MandatoryFeature
    @Deprecated
    @SystemApi
    public static final String VENDOR_EXTENSION_SERVICE = "vendor_extension";

    /**
     * @hide
     */
    @MandatoryFeature
    public static final String BLUETOOTH_SERVICE = "car_bluetooth";

    /**
     * Service name for {@link VmsClientManager}
     *
     * @hide
     */
    @OptionalFeature
    @SystemApi
    public static final String VEHICLE_MAP_SERVICE = "vehicle_map_service";

    /**
     * Service name for {@link VmsSubscriberManager}
     *
     * @deprecated {@link VmsSubscriberManager} is deprecated. Use {@link VmsClientManager} instead.
     * @hide
     */
    @OptionalFeature
    @Deprecated
    @SystemApi
    public static final String VMS_SUBSCRIBER_SERVICE = "vehicle_map_subscriber_service";

    /**
     * Service name for {@link CarDrivingStateManager}
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String CAR_DRIVING_STATE_SERVICE = "drivingstate";

    /**
     * Service name for {@link CarUxRestrictionsManager}
     */
    public static final String CAR_UX_RESTRICTION_SERVICE = "uxrestriction";

    /** @hide */
    @OptionalFeature
    @SystemApi
    public static final String OCCUPANT_AWARENESS_SERVICE = "occupant_awareness";

    /**
     * Service name for {@link android.car.settings.CarConfigurationManager}
     */
    public static final String CAR_CONFIGURATION_SERVICE = "configuration";

    /**
     * Service name for {@link android.car.media.CarMediaManager}
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String CAR_MEDIA_SERVICE = "car_media";

    /**
     *
     * Service name for {@link android.car.CarBugreportManager}
     * @hide
     */
    @MandatoryFeature
    public static final String CAR_BUGREPORT_SERVICE = "car_bugreport";

    /**
     * @hide
     */
    @OptionalFeature
    @SystemApi
    public static final String STORAGE_MONITORING_SERVICE = "storage_monitoring";

    /**
     * Service name for {@link android.car.trust.CarTrustAgentEnrollmentManager}
     * @hide
     */
    @SystemApi
    public static final String CAR_TRUST_AGENT_ENROLLMENT_SERVICE = "trust_enroll";

    /**
     * Service name for {@link android.car.watchdog.CarWatchdogManager}
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String CAR_WATCHDOG_SERVICE = "car_watchdog";

    /**
     * @hide
     */
    public static final String CAR_INPUT_SERVICE = "android.car.input";

    /**
     * Service for testing. This is system app only feature.
     * Service name for {@link CarTestManager}, to be used in {@link #getCarManager(String)}.
     * @hide
     */
    @MandatoryFeature
    @SystemApi
    public static final String TEST_SERVICE = "car-service-test";

    /** Permission necessary to access car's mileage information.
     *  @hide
     */
    @SystemApi
    public static final String PERMISSION_MILEAGE = "android.car.permission.CAR_MILEAGE";

    /** Permission necessary to access car's energy information. */
    public static final String PERMISSION_ENERGY = "android.car.permission.CAR_ENERGY";

    /**
     * Permission necessary to change value of car's range remaining.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_ADJUST_RANGE_REMAINING =
            "android.car.permission.ADJUST_RANGE_REMAINING";

    /** Permission necessary to access car's VIN information */
    public static final String PERMISSION_IDENTIFICATION =
            "android.car.permission.CAR_IDENTIFICATION";

    /** Permission necessary to access car's speed. */
    public static final String PERMISSION_SPEED = "android.car.permission.CAR_SPEED";

    /** Permission necessary to access car's dynamics state.
     *  @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_DYNAMICS_STATE =
            "android.car.permission.CAR_DYNAMICS_STATE";

    /** Permission necessary to access car's fuel door and ev charge port. */
    public static final String PERMISSION_ENERGY_PORTS = "android.car.permission.CAR_ENERGY_PORTS";

    /**
     * Permission necessary to control car's fuel door and ev charge port.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_ENERGY_PORTS =
            "android.car.permission.CONTROL_CAR_ENERGY_PORTS";
    /**
     * Permission necessary to read car's exterior lights information.
     *  @hide
     */
    @SystemApi
    public static final String PERMISSION_EXTERIOR_LIGHTS =
            "android.car.permission.CAR_EXTERIOR_LIGHTS";

    /**
     * Permission necessary to read car's interior lights information.
     */
    public static final String PERMISSION_READ_INTERIOR_LIGHTS =
            "android.car.permission.READ_CAR_INTERIOR_LIGHTS";

    /** Permission necessary to control car's exterior lights.
     *  @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_EXTERIOR_LIGHTS =
            "android.car.permission.CONTROL_CAR_EXTERIOR_LIGHTS";

    /**
     * Permission necessary to control car's interior lights.
     */
    public static final String PERMISSION_CONTROL_INTERIOR_LIGHTS =
            "android.car.permission.CONTROL_CAR_INTERIOR_LIGHTS";

    /** Permission necessary to access car's powertrain information.*/
    public static final String PERMISSION_POWERTRAIN = "android.car.permission.CAR_POWERTRAIN";

    /**
     * Permission necessary to change car audio volume through {@link CarAudioManager}.
     */
    public static final String PERMISSION_CAR_CONTROL_AUDIO_VOLUME =
            "android.car.permission.CAR_CONTROL_AUDIO_VOLUME";

    /**
     * Permission necessary to change car audio settings through {@link CarAudioManager}.
     */
    public static final String PERMISSION_CAR_CONTROL_AUDIO_SETTINGS =
            "android.car.permission.CAR_CONTROL_AUDIO_SETTINGS";

    /**
     * Permission necessary to receive full audio ducking events from car audio focus handler.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS =
            "android.car.permission.RECEIVE_CAR_AUDIO_DUCKING_EVENTS";

    /**
     * Permission necessary to use {@link CarNavigationStatusManager}.
     */
    public static final String PERMISSION_CAR_NAVIGATION_MANAGER =
            "android.car.permission.CAR_NAVIGATION_MANAGER";

    /**
     * Permission necessary to start activities in the instrument cluster through
     * {@link CarInstrumentClusterManager}
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_INSTRUMENT_CLUSTER_CONTROL =
            "android.car.permission.CAR_INSTRUMENT_CLUSTER_CONTROL";

    /**
     * Application must have this permission in order to be launched in the instrument cluster
     * display.
     *
     * @hide
     */
    public static final String PERMISSION_CAR_DISPLAY_IN_CLUSTER =
            "android.car.permission.CAR_DISPLAY_IN_CLUSTER";

    /** Permission necessary to use {@link CarInfoManager}. */
    public static final String PERMISSION_CAR_INFO = "android.car.permission.CAR_INFO";

    /**
     * Permission necessary to read information of vendor properties' permissions.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_READ_CAR_VENDOR_PERMISSION_INFO =
            "android.car.permission.READ_CAR_VENDOR_PERMISSION_INFO";

    /** Permission necessary to read temperature of car's exterior environment. */
    public static final String PERMISSION_EXTERIOR_ENVIRONMENT =
            "android.car.permission.CAR_EXTERIOR_ENVIRONMENT";

    /**
     * Permission necessary to access car specific communication channel.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_VENDOR_EXTENSION =
            "android.car.permission.CAR_VENDOR_EXTENSION";

    /**
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_APP_BLOCKING =
            "android.car.permission.CONTROL_APP_BLOCKING";

    /**
     * Permission necessary to access car's engine information.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_ENGINE_DETAILED =
            "android.car.permission.CAR_ENGINE_DETAILED";

    /**
     * Permission necessary to access car's tire pressure information.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_TIRES = "android.car.permission.CAR_TIRES";

    /**
     * Permission necessary to access car's steering angle information.
     */
    public static final String PERMISSION_READ_STEERING_STATE =
            "android.car.permission.READ_CAR_STEERING";

    /**
     * Permission necessary to read and write display units for distance, fuel volume, tire pressure
     * and ev battery.
     */
    public static final String PERMISSION_READ_DISPLAY_UNITS =
            "android.car.permission.READ_CAR_DISPLAY_UNITS";
    /**
     * Permission necessary to control display units for distance, fuel volume, tire pressure
     * and ev battery.
     */
    public static final String PERMISSION_CONTROL_DISPLAY_UNITS =
            "android.car.permission.CONTROL_CAR_DISPLAY_UNITS";

    /**
     * Permission necessary to control car's door.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_DOORS =
            "android.car.permission.CONTROL_CAR_DOORS";

    /**
     * Permission necessary to control car's windows.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_WINDOWS =
            "android.car.permission.CONTROL_CAR_WINDOWS";

    /**
     * Permission necessary to control car's seats.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_SEATS =
            "android.car.permission.CONTROL_CAR_SEATS";

    /**
     * Permission necessary to control car's mirrors.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_MIRRORS =
            "android.car.permission.CONTROL_CAR_MIRRORS";

    /**
     * Permission necessary to access Car HVAC APIs.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_CLIMATE =
            "android.car.permission.CONTROL_CAR_CLIMATE";

    /**
     * Permission necessary to access Car POWER APIs.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_POWER = "android.car.permission.CAR_POWER";

    /**
     * Permission necessary to access Car PROJECTION system APIs.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_PROJECTION = "android.car.permission.CAR_PROJECTION";

    /**
     * Permission necessary to access projection status.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_PROJECTION_STATUS =
            "android.car.permission.ACCESS_CAR_PROJECTION_STATUS";

    /**
     * Permission necessary to mock vehicle hal for testing.
     * @hide
     * @deprecated mocking vehicle HAL in car service is no longer supported.
     */
    @Deprecated
    @SystemApi
    public static final String PERMISSION_MOCK_VEHICLE_HAL =
            "android.car.permission.CAR_MOCK_VEHICLE_HAL";

    /**
     * Permission necessary to access CarTestService.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_TEST_SERVICE =
            "android.car.permission.CAR_TEST_SERVICE";

    /**
     * Permission necessary to access CarDrivingStateService to get a Car's driving state.
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_DRIVING_STATE =
            "android.car.permission.CAR_DRIVING_STATE";

    /**
     * Permission necessary to access VMS client service.
     *
     * @hide
     */
    public static final String PERMISSION_BIND_VMS_CLIENT =
            "android.car.permission.BIND_VMS_CLIENT";

    /**
     * Permissions necessary to access VMS publisher APIs.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_VMS_PUBLISHER = "android.car.permission.VMS_PUBLISHER";

    /**
     * Permissions necessary to access VMS subscriber APIs.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_VMS_SUBSCRIBER = "android.car.permission.VMS_SUBSCRIBER";

    /**
     * Permissions necessary to read diagnostic information, including vendor-specific bits.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_DIAGNOSTIC_READ_ALL =
            "android.car.permission.CAR_DIAGNOSTICS";

    /**
     * Permissions necessary to clear diagnostic information.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_DIAGNOSTIC_CLEAR =
            "android.car.permission.CLEAR_CAR_DIAGNOSTICS";

    /**
     * Permission necessary to configure UX restrictions through {@link CarUxRestrictionsManager}.
     *
     * @hide
     */
    public static final String PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION =
            "android.car.permission.CAR_UX_RESTRICTIONS_CONFIGURATION";

    /**
     * Permission necessary to listen to occupant awareness state {@link OccupantAwarenessManager}.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE =
            "android.car.permission.READ_CAR_OCCUPANT_AWARENESS_STATE";

    /**
     * Permission necessary to modify occupant awareness graph.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_OCCUPANT_AWARENESS_SYSTEM =
            "android.car.permission.CONTROL_CAR_OCCUPANT_AWARENESS_SYSTEM";

    /**
     * Permissions necessary to clear diagnostic information.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_STORAGE_MONITORING =
            "android.car.permission.STORAGE_MONITORING";

    /**
     * Permission necessary to enroll a device as a trusted authenticator device.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CAR_ENROLL_TRUST =
            "android.car.permission.CAR_ENROLL_TRUST";

    /**
     * Permission necessary to dynamically enable / disable optional car features.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_CONTROL_CAR_FEATURES =
            "android.car.permission.CONTROL_CAR_FEATURES";

    /**
     * Permission necessary to be car watchdog clients.
     *
     * @hide
     */
    @SystemApi
    public static final String PERMISSION_USE_CAR_WATCHDOG =
            "android.car.permission.USE_CAR_WATCHDOG";

    /** Type of car connection: platform runs directly in car. */
    public static final int CONNECTION_TYPE_EMBEDDED = 5;


    /** @hide */
    @IntDef({CONNECTION_TYPE_EMBEDDED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ConnectionType {}

    /**
     * Activity Action: Provide media playing through a media template app.
     * <p>Input: String extra mapped by {@link android.app.SearchManager#QUERY} is the query
     * used to start the media. String extra mapped by {@link #CAR_EXTRA_MEDIA_COMPONENT} is the
     * component name of the media app which user wants to play media on.
     * <p>Output: nothing.
     */
    @SdkConstant(SdkConstantType.ACTIVITY_INTENT_ACTION)
    public static final String CAR_INTENT_ACTION_MEDIA_TEMPLATE =
            "android.car.intent.action.MEDIA_TEMPLATE";

    /**
     * Used as a string extra field with {@link #CAR_INTENT_ACTION_MEDIA_TEMPLATE} to specify the
     * MediaBrowserService that user wants to start the media on.
     */
    public static final String CAR_EXTRA_MEDIA_COMPONENT =
            "android.car.intent.extra.MEDIA_COMPONENT";

    /**
     *
     * @deprecated Use{@link #CAR_EXTRA_MEDIA_COMPONENT} instead.
     * @removed Using this for specifying MediaBrowserService was not supported since API level 29
     * and above. Apps must use {@link #CAR_EXTRA_MEDIA_COMPONENT} instead.
     */
    @Deprecated
    public static final String CAR_EXTRA_MEDIA_PACKAGE = "android.car.intent.extra.MEDIA_PACKAGE";

    /**
     * Used as a string extra field of media session to specify the service corresponding to the
     * session.
     */
    public static final String CAR_EXTRA_BROWSE_SERVICE_FOR_SESSION =
            "android.media.session.BROWSE_SERVICE";

    /** @hide */
    public static final String CAR_SERVICE_INTERFACE_NAME = "android.car.ICar";

    private static final String CAR_SERVICE_PACKAGE = "com.android.car";

    private static final String CAR_SERVICE_CLASS = "com.android.car.CarService";

    /**
     * Category used by navigation applications to indicate which activity should be launched on
     * the instrument cluster when such application holds
     * {@link CarAppFocusManager#APP_FOCUS_TYPE_NAVIGATION} focus.
     *
     * @hide
     */
    public static final String CAR_CATEGORY_NAVIGATION = "android.car.cluster.NAVIGATION";

    /**
     * When an activity is launched in the cluster, it will receive {@link ClusterActivityState} in
     * the intent's extra under this key, containing instrument cluster information such as
     * unobscured area, visibility, etc.
     *
     * @hide
     */
    @SystemApi
    public static final String CAR_EXTRA_CLUSTER_ACTIVITY_STATE =
            "android.car.cluster.ClusterActivityState";


    /**
     * Callback to notify the Lifecycle of car service.
     *
     * <p>Access to car service should happen
     * after {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} call with
     * {@code ready} set {@code true}.</p>
     *
     * <p>When {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} is
     * called with ready set to false, access to car service should stop until car service is ready
     * again from {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} call
     * with {@code ready} set to {@code true}.</p>
     */
    public interface CarServiceLifecycleListener {
        /**
         * Car service has gone through status change.
         *
         * <p>This is always called in the main thread context.</p>
         *
         * @param car {@code Car} object that was originally associated with this lister from
         *            {@link #createCar(Context, Handler, long, Car.CarServiceLifecycleListener)}
         *            call.
         * @param ready When {@code true, car service is ready and all accesses are ok.
         *              Otherwise car service has crashed or killed and will be restarted.
         */
        void onLifecycleChanged(@NonNull Car car, boolean ready);
    }

    /**
     * {@link #createCar(Context, Handler, long, CarServiceLifecycleListener)}'s
     * waitTimeoutMs value to use to wait forever inside the call until car service is ready.
     */
    public static final long CAR_WAIT_TIMEOUT_WAIT_FOREVER = -1;

    /**
     * {@link #createCar(Context, Handler, long, CarServiceLifecycleListener)}'s
     * waitTimeoutMs value to use to skip any waiting inside the call.
     */
    public static final long CAR_WAIT_TIMEOUT_DO_NOT_WAIT = 0;

    private static final long CAR_SERVICE_BIND_RETRY_INTERVAL_MS = 500;
    private static final long CAR_SERVICE_BIND_MAX_RETRY = 20;

    private static final long CAR_SERVICE_BINDER_POLLING_INTERVAL_MS = 50;
    private static final long CAR_SERVICE_BINDER_POLLING_MAX_RETRY = 100;

    private static final int STATE_DISCONNECTED = 0;
    private static final int STATE_CONNECTING = 1;
    private static final int STATE_CONNECTED = 2;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "STATE_", value = {
            STATE_DISCONNECTED,
            STATE_CONNECTING,
            STATE_CONNECTED,
    })
    @Target({ElementType.TYPE_USE})
    public @interface StateTypeEnum {}

    /**
     * The enabling request was successful and requires reboot to take effect.
     * @hide
     */
    @SystemApi
    public static final int FEATURE_REQUEST_SUCCESS = 0;
    /**
     * The requested feature is already enabled or disabled as requested. No need to reboot the
     * system.
     * @hide
     */
    @SystemApi
    public static final int FEATURE_REQUEST_ALREADY_IN_THE_STATE = 1;
    /**
     * The requested feature is mandatory cannot be enabled or disabled. It is always enabled.
     * @hide
     */
    @SystemApi
    public static final int FEATURE_REQUEST_MANDATORY = 2;
    /**
     * The requested feature is not available and cannot be enabled or disabled.
     * @hide
     */
    @SystemApi
    public static final int FEATURE_REQUEST_NOT_EXISTING = 3;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "FEATURE_REQUEST_", value = {
            FEATURE_REQUEST_SUCCESS,
            FEATURE_REQUEST_ALREADY_IN_THE_STATE,
            FEATURE_REQUEST_MANDATORY,
            FEATURE_REQUEST_NOT_EXISTING,
    })
    @Target({ElementType.TYPE_USE})
    public @interface FeaturerRequestEnum {}

    private static final boolean DBG = false;

    private final Context mContext;

    private final Exception mConstructionStack;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private ICar mService;
    @GuardedBy("mLock")
    private boolean mServiceBound;

    @GuardedBy("mLock")
    @StateTypeEnum
    private int mConnectionState;
    @GuardedBy("mLock")
    private int mConnectionRetryCount;

    private final Runnable mConnectionRetryRunnable = new Runnable() {
        @Override
        public void run() {
            startCarService();
        }
    };

    private final Runnable mConnectionRetryFailedRunnable = new Runnable() {
        @Override
        public void run() {
            mServiceConnectionListener.onServiceDisconnected(new ComponentName(CAR_SERVICE_PACKAGE,
                    CAR_SERVICE_CLASS));
        }
    };

    private final ServiceConnection mServiceConnectionListener =
            new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            synchronized (mLock) {
                ICar newService = ICar.Stub.asInterface(service);
                if (newService == null) {
                    Log.wtf(TAG_CAR, "null binder service", new RuntimeException());
                    return;  // should not happen.
                }
                if (mService != null && mService.asBinder().equals(newService.asBinder())) {
                    // already connected.
                    return;
                }
                mConnectionState = STATE_CONNECTED;
                mService = newService;
            }
            if (mStatusChangeCallback != null) {
                mStatusChangeCallback.onLifecycleChanged(Car.this, true);
            } else if (mServiceConnectionListenerClient != null) {
                mServiceConnectionListenerClient.onServiceConnected(name, service);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            // Car service can pick up feature changes after restart.
            mFeatures.resetCache();
            synchronized (mLock) {
                if (mConnectionState  == STATE_DISCONNECTED) {
                    // can happen when client calls disconnect before onServiceDisconnected call.
                    return;
                }
                handleCarDisconnectLocked();
            }
            if (mStatusChangeCallback != null) {
                mStatusChangeCallback.onLifecycleChanged(Car.this, false);
            } else if (mServiceConnectionListenerClient != null) {
                mServiceConnectionListenerClient.onServiceDisconnected(name);
            } else {
                // This client does not handle car service restart, so should be terminated.
                finishClient();
            }
        }
    };

    @Nullable
    private final ServiceConnection mServiceConnectionListenerClient;

    /** Can be added after ServiceManager.getService call */
    @Nullable
    private final CarServiceLifecycleListener mStatusChangeCallback;

    @GuardedBy("mLock")
    private final HashMap<String, CarManagerBase> mServiceMap = new HashMap<>();

    /** Handler for generic event dispatching. */
    private final Handler mEventHandler;

    private final Handler mMainThreadEventHandler;

    private final CarFeatures mFeatures = new CarFeatures();

    /**
     * A factory method that creates Car instance for all Car API access.
     *
     * <p>Instance created with this should be disconnected from car service by calling
     * {@link #disconnect()} before the passed {code Context} is released.
     *
     * @param context App's Context. This should not be null. If you are passing
     *                {@link ContextWrapper}, make sure that its base Context is non-null as well.
     *                Otherwise it will throw {@link java.lang.NullPointerException}.
     * @param serviceConnectionListener listener for monitoring service connection.
     * @param handler the handler on which the callback should execute, or null to execute on the
     * service's main thread. Note: the service connection listener will be always on the main
     * thread regardless of the handler given.
     * @return Car instance if system is in car environment and returns {@code null} otherwise.
     *
     * @deprecated use {@link #createCar(Context, Handler)} instead.
     */
    @Deprecated
    public static Car createCar(Context context, ServiceConnection serviceConnectionListener,
            @Nullable Handler handler) {
        assertNonNullContext(context);
        if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
            Log.e(TAG_CAR, "FEATURE_AUTOMOTIVE not declared while android.car is used");
            return null;
        }
        try {
            return new Car(context, /* service= */ null , serviceConnectionListener,
                    /* statusChangeListener= */ null, handler);
        } catch (IllegalArgumentException e) {
            // Expected when car service loader is not available.
        }
        return null;
    }

    /**
     * A factory method that creates Car instance for all Car API access using main thread {@code
     * Looper}.
     *
     * <p>Instance created with this should be disconnected from car service by calling
     * {@link #disconnect()} before the passed {code Context} is released.
     *
     * @see #createCar(Context, ServiceConnection, Handler)
     *
     * @deprecated use {@link #createCar(Context, Handler)} instead.
     */
    @Deprecated
    public static Car createCar(Context context, ServiceConnection serviceConnectionListener) {
        return createCar(context, serviceConnectionListener, null);
    }

    /**
     * Creates new {@link Car} object which connected synchronously to Car Service and ready to use.
     *
     * <p>Instance created with this should be disconnected from car service by calling
     * {@link #disconnect()} before the passed {code Context} is released.
     *
     * @param context application's context
     *
     * @return Car object if operation succeeded, otherwise null.
     */
    @Nullable
    public static Car createCar(Context context) {
        return createCar(context, (Handler) null);
    }

    /**
     * Creates new {@link Car} object which connected synchronously to Car Service and ready to use.
     *
     * <p>Instance created with this should be disconnected from car service by calling
     * {@link #disconnect()} before the passed {code Context} is released.
     *
     * @param context App's Context. This should not be null. If you are passing
     *                {@link ContextWrapper}, make sure that its base Context is non-null as well.
     *                Otherwise it will throw {@link java.lang.NullPointerException}.
     * @param handler the handler on which the manager's callbacks will be executed, or null to
     * execute on the application's main thread.
     *
     * @return Car object if operation succeeded, otherwise null.
     */
    @Nullable
    public static Car createCar(Context context, @Nullable Handler handler) {
        assertNonNullContext(context);
        Car car = null;
        IBinder service = null;
        boolean started = false;
        int retryCount = 0;
        while (true) {
            service = ServiceManager.getService(CAR_SERVICE_BINDER_SERVICE_NAME);
            if (car == null) {
                // service can be still null. The constructor is safe for null service.
                car = new Car(context, ICar.Stub.asInterface(service),
                        null /*serviceConnectionListener*/, null /*statusChangeListener*/, handler);
            }
            if (service != null) {
                if (!started) {  // specialization for most common case.
                    // Do this to crash client when car service crashes.
                    car.startCarService();
                    return car;
                }
                break;
            }
            if (!started) {
                car.startCarService();
                started = true;
            }
            retryCount++;
            if (retryCount > CAR_SERVICE_BINDER_POLLING_MAX_RETRY) {
                Log.e(TAG_CAR, "cannot get car_service, waited for car service (ms):"
                                + CAR_SERVICE_BINDER_POLLING_INTERVAL_MS
                                * CAR_SERVICE_BINDER_POLLING_MAX_RETRY,
                        new RuntimeException());
                return null;
            }
            try {
                Thread.sleep(CAR_SERVICE_BINDER_POLLING_INTERVAL_MS);
            } catch (InterruptedException e) {
                Log.e(CarLibLog.TAG_CAR, "interrupted while waiting for car_service",
                        new RuntimeException());
                return null;
            }
        }
        // Can be accessed from mServiceConnectionListener in main thread.
        synchronized (car) {
            if (car.mService == null) {
                car.mService = ICar.Stub.asInterface(service);
                Log.w(TAG_CAR,
                        "waited for car_service (ms):"
                                + CAR_SERVICE_BINDER_POLLING_INTERVAL_MS * retryCount,
                        new RuntimeException());
            }
            car.mConnectionState = STATE_CONNECTED;
        }
        return car;
    }

    /**
     * Creates new {@link Car} object with {@link CarServiceLifecycleListener}.
     *
     * <p>Instance created with this should be disconnected from car service by calling
     * {@link #disconnect()} before the passed {code Context} is released.
     *
     * <p> If car service is ready inside this call and if the caller is running in the main thread,
     * {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} will be called
     * with ready set to be true. Otherwise,
     * {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} will be called
     * from the main thread later. </p>
     *
     * <p>This call can block up to specified waitTimeoutMs to wait for car service to be ready.
     * If car service is not ready within the given time, it will return a Car instance in
     * disconnected state. Blocking main thread forever can lead into getting ANR (Application Not
     * Responding) killing from system and should not be used if the app is supposed to survive
     * across the crash / restart of car service. It can be still useful in case the app cannot do
     * anything without car service being ready. In any waiting, if the thread is getting
     * interrupted, it will return immediately.
     * </p>
     *
     * <p>Note that returned {@link Car} object is not guaranteed to be connected when there is
     * a limited timeout. Regardless of returned car being connected or not, it is recommended to
     * implement all car related initialization inside
     * {@link CarServiceLifecycleListener#onLifecycleChanged(Car, boolean)} and avoid the
     * needs to check if returned {@link Car} is connected or not from returned {@link Car}.</p>
     *
     * @param context App's Context. This should not be null. If you are passing
     *                {@link ContextWrapper}, make sure that its base Context is non-null as well.
     *                Otherwise it will throw {@link java.lang.NullPointerException}.
     * @param handler dispatches all Car*Manager events to this Handler. Exception is
     *                {@link CarServiceLifecycleListener} which will be always dispatched to main
     *                thread. Passing null leads into dispatching all Car*Manager callbacks to main
     *                thread as well.
     * @param waitTimeoutMs Setting this to {@link #CAR_WAIT_TIMEOUT_DO_NOT_WAIT} will guarantee
     *                      that the API does not wait for the car service at all. Setting this to
     *                      to {@link #CAR_WAIT_TIMEOUT_WAIT_FOREVER} will block the call forever
     *                      until the car service is ready. Setting any positive value will be
     *                      interpreted as timeout value.
     */
    @NonNull
    public static Car createCar(@NonNull Context context,
            @Nullable Handler handler, long waitTimeoutMs,
            @NonNull CarServiceLifecycleListener statusChangeListener) {
        assertNonNullContext(context);
        Objects.requireNonNull(statusChangeListener);
        Car car = null;
        IBinder service = null;
        boolean started = false;
        int retryCount = 0;
        long maxRetryCount = 0;
        if (waitTimeoutMs > 0) {
            maxRetryCount = waitTimeoutMs / CAR_SERVICE_BINDER_POLLING_INTERVAL_MS;
            // at least wait once if it is positive value.
            if (maxRetryCount == 0) {
                maxRetryCount = 1;
            }
        }
        boolean isMainThread = Looper.myLooper() == Looper.getMainLooper();
        while (true) {
            service = ServiceManager.getService(CAR_SERVICE_BINDER_SERVICE_NAME);
            if (car == null) {
                // service can be still null. The constructor is safe for null service.
                car = new Car(context, ICar.Stub.asInterface(service), null, statusChangeListener,
                        handler);
            }
            if (service != null) {
                if (!started) {  // specialization for most common case : car service already ready
                    car.dispatchCarReadyToMainThread(isMainThread);
                    // Needs this for CarServiceLifecycleListener. Note that ServiceConnection
                    // will skip the callback as valid mService is set already.
                    car.startCarService();
                    return car;
                }
                // service available after starting.
                break;
            }
            if (!started) {
                car.startCarService();
                started = true;
            }
            retryCount++;
            if (waitTimeoutMs < 0 && retryCount >= CAR_SERVICE_BINDER_POLLING_MAX_RETRY
                    && retryCount % CAR_SERVICE_BINDER_POLLING_MAX_RETRY == 0) {
                // Log warning if car service is not alive even for waiting forever case.
                Log.w(TAG_CAR, "car_service not ready, waited for car service (ms):"
                                + retryCount * CAR_SERVICE_BINDER_POLLING_INTERVAL_MS,
                        new RuntimeException());
            } else if (waitTimeoutMs >= 0 && retryCount > maxRetryCount) {
                if (waitTimeoutMs > 0) {
                    Log.w(TAG_CAR, "car_service not ready, waited for car service (ms):"
                                    + waitTimeoutMs,
                            new RuntimeException());
                }
                return car;
            }

            try {
                Thread.sleep(CAR_SERVICE_BINDER_POLLING_INTERVAL_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                Log.w(TAG_CAR, "interrupted", new RuntimeException());
                return car;
            }
        }
        // Can be accessed from mServiceConnectionListener in main thread.
        synchronized (car.mLock) {
            Log.w(TAG_CAR,
                    "waited for car_service (ms):"
                            + retryCount * CAR_SERVICE_BINDER_POLLING_INTERVAL_MS,
                    new RuntimeException());
            // ServiceConnection has handled everything.
            if (car.mService != null) {
                return car;
            }
            // mService check in ServiceConnection prevents calling
            // onLifecycleChanged. So onLifecycleChanged should be called explicitly
            // but do it outside lock.
            car.mService = ICar.Stub.asInterface(service);
            car.mConnectionState = STATE_CONNECTED;
        }
        car.dispatchCarReadyToMainThread(isMainThread);
        return car;
    }

    private static void assertNonNullContext(Context context) {
        Objects.requireNonNull(context);
        if (context instanceof ContextWrapper
                && ((ContextWrapper) context).getBaseContext() == null) {
            throw new NullPointerException(
                    "ContextWrapper with null base passed as Context, forgot to set base Context?");
        }
    }

    private void dispatchCarReadyToMainThread(boolean isMainThread) {
        if (isMainThread) {
            mStatusChangeCallback.onLifecycleChanged(this, true);
        } else {
            // should dispatch to main thread.
            mMainThreadEventHandler.post(
                    () -> mStatusChangeCallback.onLifecycleChanged(this, true));
        }
    }

    private Car(Context context, @Nullable ICar service,
            @Nullable ServiceConnection serviceConnectionListener,
            @Nullable CarServiceLifecycleListener statusChangeListener,
            @Nullable Handler handler) {
        mContext = context;
        mEventHandler = determineEventHandler(handler);
        mMainThreadEventHandler = determineMainThreadEventHandler(mEventHandler);

        mService = service;
        if (service != null) {
            mConnectionState = STATE_CONNECTED;
        } else {
            mConnectionState = STATE_DISCONNECTED;
        }
        mServiceConnectionListenerClient = serviceConnectionListener;
        mStatusChangeCallback = statusChangeListener;
        // Store construction stack so that client can get help when it crashes when car service
        // crashes.
        if (serviceConnectionListener == null && statusChangeListener == null) {
            mConstructionStack = new RuntimeException();
        } else {
            mConstructionStack = null;
        }
    }

    /**
     * Car constructor when ICar binder is already available. The binder can be null.
     * @hide
     */
    public Car(Context context, @Nullable ICar service, @Nullable Handler handler) {
        this(context, service, null /*serviceConnectionListener*/, null /*statusChangeListener*/,
                handler);
    }

    private static Handler determineMainThreadEventHandler(Handler eventHandler) {
        Looper mainLooper = Looper.getMainLooper();
        return (eventHandler.getLooper() == mainLooper) ? eventHandler : new Handler(mainLooper);
    }

    private static Handler determineEventHandler(@Nullable Handler handler) {
        if (handler == null) {
            Looper looper = Looper.getMainLooper();
            handler = new Handler(looper);
        }
        return handler;
    }

    /**
     * Connect to car service. This can be called while it is disconnected.
     * @throws IllegalStateException If connection is still on-going from previous
     *         connect call or it is already connected
     *
     * @deprecated this method is not need if this object is created via
     * {@link #createCar(Context, Handler)}.
     */
    @Deprecated
    public void connect() throws IllegalStateException {
        synchronized (mLock) {
            if (mConnectionState != STATE_DISCONNECTED) {
                throw new IllegalStateException("already connected or connecting");
            }
            mConnectionState = STATE_CONNECTING;
            startCarService();
        }
    }

    private void handleCarDisconnectLocked() {
        if (mConnectionState == STATE_DISCONNECTED) {
            // can happen when client calls disconnect with onServiceDisconnected already called.
            return;
        }
        mEventHandler.removeCallbacks(mConnectionRetryRunnable);
        mMainThreadEventHandler.removeCallbacks(mConnectionRetryFailedRunnable);
        mConnectionRetryCount = 0;
        tearDownCarManagersLocked();
        mService = null;
        mConnectionState = STATE_DISCONNECTED;
    }

    /**
     * Disconnect from car service. This can be called while disconnected. Once disconnect is
     * called, all Car*Managers from this instance becomes invalid, and
     * {@link Car#getCarManager(String)} will return different instance if it is connected again.
     */
    public void disconnect() {
        synchronized (mLock) {
            handleCarDisconnectLocked();
            if (mServiceBound) {
                mContext.unbindService(mServiceConnectionListener);
                mServiceBound = false;
            }
        }
    }

    /**
     * Tells if it is connected to the service or not. This will return false if it is still
     * connecting.
     * @return
     */
    public boolean isConnected() {
        synchronized (mLock) {
            return mService != null;
        }
    }

    /**
     * Tells if this instance is already connecting to car service or not.
     * @return
     */
    public boolean isConnecting() {
        synchronized (mLock) {
            return mConnectionState == STATE_CONNECTING;
        }
    }

    /** @hide */
    @VisibleForTesting
    public ServiceConnection getServiceConnectionListener() {
        return mServiceConnectionListener;
    }

    /**
     * Get car specific service as in {@link Context#getSystemService(String)}. Returned
     * {@link Object} should be type-casted to the desired service.
     * For example, to get sensor service,
     * SensorManagerService sensorManagerService = car.getCarManager(Car.SENSOR_SERVICE);
     * @param serviceName Name of service that should be created like {@link #SENSOR_SERVICE}.
     * @return Matching service manager or null if there is no such service.
     */
    @Nullable
    public Object getCarManager(String serviceName) {
        CarManagerBase manager;
        synchronized (mLock) {
            if (mService == null) {
                Log.w(TAG_CAR, "getCarManager not working while car service not ready");
                return null;
            }
            manager = mServiceMap.get(serviceName);
            if (manager == null) {
                try {
                    IBinder binder = mService.getCarService(serviceName);
                    if (binder == null) {
                        Log.w(TAG_CAR, "getCarManager could not get binder for service:"
                                + serviceName);
                        return null;
                    }
                    manager = createCarManagerLocked(serviceName, binder);
                    if (manager == null) {
                        Log.w(TAG_CAR, "getCarManager could not create manager for service:"
                                        + serviceName);
                        return null;
                    }
                    mServiceMap.put(serviceName, manager);
                } catch (RemoteException e) {
                    handleRemoteExceptionFromCarService(e);
                }
            }
        }
        return manager;
    }

    /**
     * Return the type of currently connected car.
     * @return
     */
    @ConnectionType
    public int getCarConnectionType() {
        return CONNECTION_TYPE_EMBEDDED;
    }

    /**
     * Checks if {code featureName} is enabled in this car.
     *
     * <p>For optional features, this can return false if the car cannot support it. Optional
     * features should be used only when they are supported.</p>
     *
     * <p>For mandatory features, this will always return true.
     */
    public boolean isFeatureEnabled(@NonNull String featureName) {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return false;
            }
            service = mService;
        }
        return mFeatures.isFeatureEnabled(service, featureName);
    }

    /**
     * Enables the requested car feature. It becomes no-op if the feature is already enabled. The
     * change take effects after reboot.
     *
     * @return true if the feature is enabled or was enabled before.
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(PERMISSION_CONTROL_CAR_FEATURES)
    @FeaturerRequestEnum
    public int enableFeature(@NonNull String featureName) {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return FEATURE_REQUEST_NOT_EXISTING;
            }
            service = mService;
        }
        try {
            return service.enableFeature(featureName);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, FEATURE_REQUEST_NOT_EXISTING);
        }
    }

    /**
     * Disables the requested car feature. It becomes no-op if the feature is already disabled. The
     * change take effects after reboot.
     *
     * @return true if the request succeeds or if it was already disabled.
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(PERMISSION_CONTROL_CAR_FEATURES)
    @FeaturerRequestEnum
    public int disableFeature(@NonNull String featureName) {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return FEATURE_REQUEST_NOT_EXISTING;
            }
            service = mService;
        }
        try {
            return service.disableFeature(featureName);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, FEATURE_REQUEST_NOT_EXISTING);
        }
    }

    /**
     * Returns all =enabled features at the moment including mandatory, optional, and
     * experimental features.
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(PERMISSION_CONTROL_CAR_FEATURES)
    @NonNull public List<String> getAllEnabledFeatures() {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return Collections.EMPTY_LIST;
            }
            service = mService;
        }
        try {
            return service.getAllEnabledFeatures();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Returns the list of disabled features which are not effective yet. Those features will be
     * disabled when system restarts later.
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(PERMISSION_CONTROL_CAR_FEATURES)
    @NonNull public List<String> getAllPendingDisabledFeatures() {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return Collections.EMPTY_LIST;
            }
            service = mService;
        }
        try {
            return service.getAllPendingDisabledFeatures();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Returns the list of enabled features which are not effective yet. Those features will be
     * enabled when system restarts later.
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(PERMISSION_CONTROL_CAR_FEATURES)
    @NonNull public List<String> getAllPendingEnabledFeatures() {
        ICar service;
        synchronized (mLock) {
            if (mService == null) {
                return Collections.EMPTY_LIST;
            }
            service = mService;
        }
        try {
            return service.getAllPendingEnabledFeatures();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /** @hide */
    public Context getContext() {
        return mContext;
    }

    /** @hide */
    @VisibleForTesting
    public Handler getEventHandler() {
        return mEventHandler;
    }

    /** @hide */
    @VisibleForTesting
    public <T> T handleRemoteExceptionFromCarService(RemoteException e, T returnValue) {
        handleRemoteExceptionFromCarService(e);
        return returnValue;
    }

    /** @hide */
    void handleRemoteExceptionFromCarService(RemoteException e) {
        if (e instanceof TransactionTooLargeException) {
            Log.w(TAG_CAR, "Car service threw TransactionTooLargeException", e);
            throw new CarTransactionException(e, "Car service threw TransactionTooLargException");
        } else {
            Log.w(TAG_CAR, "Car service has crashed", e);
        }
    }


    private void finishClient() {
        if (mContext == null) {
            throw new IllegalStateException("Car service has crashed, null Context");
        }
        if (mContext instanceof Activity) {
            Activity activity = (Activity) mContext;
            if (!activity.isFinishing()) {
                Log.w(TAG_CAR,
                        "Car service crashed, client not handling it, finish Activity, created "
                                + "from " + mConstructionStack);
                activity.finish();
            }
            return;
        } else if (mContext instanceof Service) {
            Service service = (Service) mContext;
            killClient(service.getPackageName() + "," + service.getClass().getSimpleName());
        } else {
            killClient(/* clientInfo= */ null);
        }
    }

    private void killClient(@Nullable String clientInfo) {
        Log.w(TAG_CAR, "**Car service has crashed. Client(" + clientInfo + ") is not handling it."
                        + " Client should use Car.createCar(..., CarServiceLifecycleListener, .."
                        + ".) to handle it properly. Check pritned callstack to check where other "
                        + "version of Car.createCar() was called. Killing the client process**",
                mConstructionStack);
        Process.killProcess(Process.myPid());
    }

    /** @hide */
    public static <T> T handleRemoteExceptionFromCarService(Service service, RemoteException e,
            T returnValue) {
        handleRemoteExceptionFromCarService(service, e);
        return returnValue;
    }

    /** @hide */
    public static  void handleRemoteExceptionFromCarService(Service service, RemoteException e) {
        if (e instanceof TransactionTooLargeException) {
            Log.w(TAG_CAR, "Car service threw TransactionTooLargeException, client:"
                    + service.getPackageName() + ","
                    + service.getClass().getSimpleName(), e);
            throw new CarTransactionException(e, "Car service threw TransactionTooLargeException, "
                + "client: %s, %s", service.getPackageName(), service.getClass().getSimpleName());
        } else {
            Log.w(TAG_CAR, "Car service has crashed, client:"
                    + service.getPackageName() + ","
                    + service.getClass().getSimpleName(), e);
            service.stopSelf();
        }
    }

    @Nullable
    private CarManagerBase createCarManagerLocked(String serviceName, IBinder binder) {
        CarManagerBase manager = null;
        switch (serviceName) {
            case AUDIO_SERVICE:
                manager = new CarAudioManager(this, binder);
                break;
            case SENSOR_SERVICE:
                manager = new CarSensorManager(this, binder);
                break;
            case INFO_SERVICE:
                manager = new CarInfoManager(this, binder);
                break;
            case APP_FOCUS_SERVICE:
                manager = new CarAppFocusManager(this, binder);
                break;
            case PACKAGE_SERVICE:
                manager = new CarPackageManager(this, binder);
                break;
            case CAR_OCCUPANT_ZONE_SERVICE:
                manager = new CarOccupantZoneManager(this, binder);
                break;
            case CAR_NAVIGATION_SERVICE:
                manager = new CarNavigationStatusManager(this, binder);
                break;
            case CABIN_SERVICE:
                manager = new CarCabinManager(this, binder);
                break;
            case DIAGNOSTIC_SERVICE:
                manager = new CarDiagnosticManager(this, binder);
                break;
            case HVAC_SERVICE:
                manager = new CarHvacManager(this, binder);
                break;
            case POWER_SERVICE:
                manager = new CarPowerManager(this, binder);
                break;
            case PROJECTION_SERVICE:
                manager = new CarProjectionManager(this, binder);
                break;
            case PROPERTY_SERVICE:
                manager = new CarPropertyManager(this, ICarProperty.Stub.asInterface(binder));
                break;
            case VENDOR_EXTENSION_SERVICE:
                manager = new CarVendorExtensionManager(this, binder);
                break;
            case CAR_INSTRUMENT_CLUSTER_SERVICE:
                manager = new CarInstrumentClusterManager(this, binder);
                break;
            case TEST_SERVICE:
                /* CarTestManager exist in static library. So instead of constructing it here,
                 * only pass binder wrapper so that CarTestManager can be constructed outside. */
                manager = new CarTestManagerBinderWrapper(this, binder);
                break;
            case VEHICLE_MAP_SERVICE:
                manager = new VmsClientManager(this, binder);
                break;
            case VMS_SUBSCRIBER_SERVICE:
                manager = VmsSubscriberManager.wrap(this,
                        (VmsClientManager) getCarManager(VEHICLE_MAP_SERVICE));
                break;
            case BLUETOOTH_SERVICE:
                manager = new CarBluetoothManager(this, binder);
                break;
            case STORAGE_MONITORING_SERVICE:
                manager = new CarStorageMonitoringManager(this, binder);
                break;
            case CAR_DRIVING_STATE_SERVICE:
                manager = new CarDrivingStateManager(this, binder);
                break;
            case CAR_UX_RESTRICTION_SERVICE:
                manager = new CarUxRestrictionsManager(this, binder);
                break;
            case OCCUPANT_AWARENESS_SERVICE:
                manager = new OccupantAwarenessManager(this, binder);
                break;
            case CAR_CONFIGURATION_SERVICE:
                manager = new CarConfigurationManager(this, binder);
                break;
            case CAR_TRUST_AGENT_ENROLLMENT_SERVICE:
                manager = new CarTrustAgentEnrollmentManager(this, binder);
                break;
            case CAR_MEDIA_SERVICE:
                manager = new CarMediaManager(this, binder);
                break;
            case CAR_BUGREPORT_SERVICE:
                manager = new CarBugreportManager(this, binder);
                break;
            case CAR_USER_SERVICE:
                manager = new CarUserManager(this, binder);
                break;
            case CAR_WATCHDOG_SERVICE:
                manager = new CarWatchdogManager(this, binder);
                break;
            case CAR_INPUT_SERVICE:
                manager = new CarInputManager(this, binder);
                break;
            default:
                // Experimental or non-existing
                String className = null;
                try {
                    className = mService.getCarManagerClassForFeature(serviceName);
                } catch (RemoteException e) {
                    handleRemoteExceptionFromCarService(e);
                    return null;
                }
                if (className == null) {
                    Log.e(TAG_CAR, "Cannot construct CarManager for service:" + serviceName
                            + " : no class defined");
                    return null;
                }
                manager = constructCarManager(className, binder);
                break;
        }
        return manager;
    }

    private CarManagerBase constructCarManager(String className, IBinder binder) {
        try {
            // Should use class loader for the Context as class loader for car api does not
            // see the class.
            ClassLoader loader = mContext.getClassLoader();
            Class managerClass = loader.loadClass(className);
            Constructor constructor = managerClass.getConstructor(Car.class, IBinder.class);
            CarManagerBase manager = (CarManagerBase) constructor.newInstance(this, binder);
            return manager;
        } catch (ClassNotFoundException | IllegalAccessException | NoSuchMethodException
                | InstantiationException | InvocationTargetException e) {
            Log.e(TAG_CAR, "Cannot construct CarManager, class:" + className, e);
            return null;
        }
    }

    private void startCarService() {
        Intent intent = new Intent();
        intent.setPackage(CAR_SERVICE_PACKAGE);
        intent.setAction(Car.CAR_SERVICE_INTERFACE_NAME);
        boolean bound = mContext.bindServiceAsUser(intent, mServiceConnectionListener,
                Context.BIND_AUTO_CREATE, UserHandle.CURRENT_OR_SELF);
        synchronized (mLock) {
            if (!bound) {
                mConnectionRetryCount++;
                if (mConnectionRetryCount > CAR_SERVICE_BIND_MAX_RETRY) {
                    Log.w(TAG_CAR, "cannot bind to car service after max retry");
                    mMainThreadEventHandler.post(mConnectionRetryFailedRunnable);
                } else {
                    mEventHandler.postDelayed(mConnectionRetryRunnable,
                            CAR_SERVICE_BIND_RETRY_INTERVAL_MS);
                }
            } else {
                mEventHandler.removeCallbacks(mConnectionRetryRunnable);
                mMainThreadEventHandler.removeCallbacks(mConnectionRetryFailedRunnable);
                mConnectionRetryCount = 0;
                mServiceBound = true;
            }
        }
    }

    private void tearDownCarManagersLocked() {
        // All disconnected handling should be only doing its internal cleanup.
        for (CarManagerBase manager: mServiceMap.values()) {
            manager.onCarDisconnected();
        }
        mServiceMap.clear();
    }
}
