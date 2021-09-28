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

package com.android.car;

import android.annotation.MainThread;
import android.annotation.Nullable;
import android.app.ActivityManager;
import android.car.Car;
import android.car.CarFeatures;
import android.car.ICar;
import android.car.cluster.renderer.IInstrumentClusterNavigation;
import android.car.user.CarUserManager;
import android.car.userlib.CarUserManagerHelper;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.hardware.automotive.vehicle.V2_0.IVehicle;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.os.ResultReceiver;
import android.os.ShellCallback;
import android.os.Trace;
import android.os.UserManager;
import android.util.EventLog;
import android.util.Log;
import android.util.Slog;
import android.util.TimingsTraceLog;

import com.android.car.am.FixedActivityService;
import com.android.car.audio.CarAudioService;
import com.android.car.cluster.InstrumentClusterService;
import com.android.car.garagemode.GarageModeService;
import com.android.car.hal.VehicleHal;
import com.android.car.pm.CarPackageManagerService;
import com.android.car.stats.CarStatsService;
import com.android.car.systeminterface.SystemInterface;
import com.android.car.trust.CarTrustedDeviceService;
import com.android.car.user.CarUserNoticeService;
import com.android.car.user.CarUserService;
import com.android.car.vms.VmsBrokerService;
import com.android.car.watchdog.CarWatchdogService;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.car.EventLogTags;
import com.android.internal.car.ICarServiceHelper;
import com.android.internal.os.IResultReceiver;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class ICarImpl extends ICar.Stub {

    public static final String INTERNAL_INPUT_SERVICE = "internal_input";
    public static final String INTERNAL_SYSTEM_ACTIVITY_MONITORING_SERVICE =
            "system_activity_monitoring";

    private static final int INITIAL_VHAL_GET_RETRY = 2;

    private final Context mContext;
    private final VehicleHal mHal;

    private final CarFeatureController mFeatureController;

    private final SystemInterface mSystemInterface;

    private final SystemActivityMonitoringService mSystemActivityMonitoringService;
    private final CarPowerManagementService mCarPowerManagementService;
    private final CarPackageManagerService mCarPackageManagerService;
    private final CarInputService mCarInputService;
    private final CarDrivingStateService mCarDrivingStateService;
    private final CarUxRestrictionsManagerService mCarUXRestrictionsService;
    private final OccupantAwarenessService mOccupantAwarenessService;
    private final CarAudioService mCarAudioService;
    private final CarProjectionService mCarProjectionService;
    private final CarPropertyService mCarPropertyService;
    private final CarNightService mCarNightService;
    private final AppFocusService mAppFocusService;
    private final FixedActivityService mFixedActivityService;
    private final GarageModeService mGarageModeService;
    private final InstrumentClusterService mInstrumentClusterService;
    private final CarLocationService mCarLocationService;
    private final SystemStateControllerService mSystemStateControllerService;
    private final CarBluetoothService mCarBluetoothService;
    private final PerUserCarServiceHelper mPerUserCarServiceHelper;
    private final CarDiagnosticService mCarDiagnosticService;
    private final CarStorageMonitoringService mCarStorageMonitoringService;
    private final CarConfigurationService mCarConfigurationService;
    private final CarTrustedDeviceService mCarTrustedDeviceService;
    private final CarMediaService mCarMediaService;
    private final CarUserManagerHelper mUserManagerHelper;
    private final CarUserService mCarUserService;
    private final CarOccupantZoneService mCarOccupantZoneService;
    private final CarUserNoticeService mCarUserNoticeService;
    private final VmsBrokerService mVmsBrokerService;
    private final CarBugreportManagerService mCarBugreportManagerService;
    private final CarStatsService mCarStatsService;
    private final CarExperimentalFeatureServiceController mCarExperimentalFeatureServiceController;
    private final CarWatchdogService mCarWatchdogService;

    private final CarServiceBase[] mAllServices;

    private static final String TAG = "ICarImpl";
    private static final String VHAL_TIMING_TAG = "VehicleHalTiming";
    private static final boolean DBG = true; // TODO(b/154033860): STOPSHIP if true

    private TimingsTraceLog mBootTiming;

    private final Object mLock = new Object();

    /** Test only service. Populate it only when necessary. */
    @GuardedBy("mLock")
    private CarTestService mCarTestService;

    @GuardedBy("mLock")
    private ICarServiceHelper mICarServiceHelper;

    private final String mVehicleInterfaceName;

    public ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,
            CanBusErrorNotifier errorNotifier, String vehicleInterfaceName) {
        this(serviceContext, vehicle, systemInterface, errorNotifier, vehicleInterfaceName,
                /* carUserService= */ null, /* carWatchdogService= */ null);
    }

    @VisibleForTesting
    ICarImpl(Context serviceContext, IVehicle vehicle, SystemInterface systemInterface,
            CanBusErrorNotifier errorNotifier, String vehicleInterfaceName,
            @Nullable CarUserService carUserService,
            @Nullable CarWatchdogService carWatchdogService) {
        mContext = serviceContext;
        mSystemInterface = systemInterface;
        mHal = new VehicleHal(serviceContext, vehicle);
        // Do this before any other service components to allow feature check. It should work
        // even without init. For that, vhal get is retried as it can be too early.
        VehiclePropValue disabledOptionalFeatureValue = mHal.getIfAvailableOrFailForEarlyStage(
                    VehicleProperty.DISABLED_OPTIONAL_FEATURES, INITIAL_VHAL_GET_RETRY);
        String[] disabledFeaturesFromVhal = null;
        if (disabledOptionalFeatureValue != null) {
            String disabledFeatures = disabledOptionalFeatureValue.value.stringValue;
            if (disabledFeatures != null && !disabledFeatures.isEmpty()) {
                disabledFeaturesFromVhal = disabledFeatures.split(",");
            }
        }
        if (disabledFeaturesFromVhal == null) {
            disabledFeaturesFromVhal = new String[0];
        }
        Resources res = mContext.getResources();
        String[] defaultEnabledFeatures = res.getStringArray(
                R.array.config_allowed_optional_car_features);
        mFeatureController = new CarFeatureController(serviceContext, defaultEnabledFeatures,
                disabledFeaturesFromVhal , mSystemInterface.getSystemCarDir());
        CarLocalServices.addService(CarFeatureController.class, mFeatureController);
        mVehicleInterfaceName = vehicleInterfaceName;
        mUserManagerHelper = new CarUserManagerHelper(serviceContext);
        if (carUserService != null) {
            mCarUserService = carUserService;
        } else {
            UserManager userManager =
                    (UserManager) serviceContext.getSystemService(Context.USER_SERVICE);
            int maxRunningUsers = res.getInteger(
                    com.android.internal.R.integer.config_multiuserMaxRunningUsers);
            mCarUserService = new CarUserService(serviceContext, mHal.getUserHal(),
                    mUserManagerHelper, userManager, ActivityManager.getService(), maxRunningUsers);
        }
        mCarOccupantZoneService = new CarOccupantZoneService(serviceContext);
        mSystemActivityMonitoringService = new SystemActivityMonitoringService(serviceContext);
        mCarPowerManagementService = new CarPowerManagementService(mContext, mHal.getPowerHal(),
                systemInterface, mCarUserService);
        if (mFeatureController.isFeatureEnabled(CarFeatures.FEATURE_CAR_USER_NOTICE_SERVICE)) {
            mCarUserNoticeService = new CarUserNoticeService(serviceContext);
        } else {
            mCarUserNoticeService = null;
        }
        mCarPropertyService = new CarPropertyService(serviceContext, mHal.getPropertyHal());
        mCarDrivingStateService = new CarDrivingStateService(serviceContext, mCarPropertyService);
        mCarUXRestrictionsService = new CarUxRestrictionsManagerService(serviceContext,
                mCarDrivingStateService, mCarPropertyService);
        if (mFeatureController.isFeatureEnabled(Car.OCCUPANT_AWARENESS_SERVICE)) {
            mOccupantAwarenessService = new OccupantAwarenessService(serviceContext);
        } else {
            mOccupantAwarenessService = null;
        }
        mCarPackageManagerService = new CarPackageManagerService(serviceContext,
                mCarUXRestrictionsService,
                mSystemActivityMonitoringService);
        mPerUserCarServiceHelper = new PerUserCarServiceHelper(serviceContext, mCarUserService);
        mCarBluetoothService = new CarBluetoothService(serviceContext, mPerUserCarServiceHelper);
        mCarInputService = new CarInputService(serviceContext, mHal.getInputHal(), mCarUserService);
        mCarProjectionService = new CarProjectionService(
                serviceContext, null /* handler */, mCarInputService, mCarBluetoothService);
        mGarageModeService = new GarageModeService(mContext);
        mAppFocusService = new AppFocusService(serviceContext, mSystemActivityMonitoringService);
        mCarAudioService = new CarAudioService(serviceContext);
        mCarNightService = new CarNightService(serviceContext, mCarPropertyService);
        mFixedActivityService = new FixedActivityService(serviceContext);
        mInstrumentClusterService = new InstrumentClusterService(serviceContext,
                mAppFocusService, mCarInputService);
        mSystemStateControllerService = new SystemStateControllerService(
                serviceContext, mCarAudioService, this);
        mCarStatsService = new CarStatsService(serviceContext);
        mCarStatsService.init();
        if (mFeatureController.isFeatureEnabled(Car.VEHICLE_MAP_SERVICE)) {
            mVmsBrokerService = new VmsBrokerService(mContext, mCarStatsService);
        } else {
            mVmsBrokerService = null;
        }
        if (mFeatureController.isFeatureEnabled(Car.DIAGNOSTIC_SERVICE)) {
            mCarDiagnosticService = new CarDiagnosticService(serviceContext,
                    mHal.getDiagnosticHal());
        } else {
            mCarDiagnosticService = null;
        }
        if (mFeatureController.isFeatureEnabled(Car.STORAGE_MONITORING_SERVICE)) {
            mCarStorageMonitoringService = new CarStorageMonitoringService(serviceContext,
                    systemInterface);
        } else {
            mCarStorageMonitoringService = null;
        }
        mCarConfigurationService =
                new CarConfigurationService(serviceContext, new JsonReaderImpl());
        mCarLocationService = new CarLocationService(serviceContext);
        mCarTrustedDeviceService = new CarTrustedDeviceService(serviceContext);
        mCarMediaService = new CarMediaService(serviceContext, mCarUserService);
        mCarBugreportManagerService = new CarBugreportManagerService(serviceContext);
        if (!Build.IS_USER) {
            mCarExperimentalFeatureServiceController = new CarExperimentalFeatureServiceController(
                    serviceContext);
        } else {
            mCarExperimentalFeatureServiceController = null;
        }
        if (carWatchdogService == null) {
            mCarWatchdogService = new CarWatchdogService(serviceContext);
        } else {
            mCarWatchdogService = carWatchdogService;
        }

        CarLocalServices.addService(CarPowerManagementService.class, mCarPowerManagementService);
        CarLocalServices.addService(CarPropertyService.class, mCarPropertyService);
        CarLocalServices.addService(CarUserService.class, mCarUserService);
        CarLocalServices.addService(CarTrustedDeviceService.class, mCarTrustedDeviceService);
        CarLocalServices.addService(CarUserNoticeService.class, mCarUserNoticeService);
        CarLocalServices.addService(SystemInterface.class, mSystemInterface);
        CarLocalServices.addService(CarDrivingStateService.class, mCarDrivingStateService);
        CarLocalServices.addService(PerUserCarServiceHelper.class, mPerUserCarServiceHelper);
        CarLocalServices.addService(FixedActivityService.class, mFixedActivityService);
        CarLocalServices.addService(VmsBrokerService.class, mVmsBrokerService);
        CarLocalServices.addService(CarOccupantZoneService.class, mCarOccupantZoneService);
        CarLocalServices.addService(AppFocusService.class, mAppFocusService);

        // Be careful with order. Service depending on other service should be inited later.
        List<CarServiceBase> allServices = new ArrayList<>();
        allServices.add(mFeatureController);
        allServices.add(mCarUserService);
        allServices.add(mSystemActivityMonitoringService);
        allServices.add(mCarPowerManagementService);
        allServices.add(mCarPropertyService);
        allServices.add(mCarDrivingStateService);
        allServices.add(mCarOccupantZoneService);
        allServices.add(mCarUXRestrictionsService);
        addServiceIfNonNull(allServices, mOccupantAwarenessService);
        allServices.add(mCarPackageManagerService);
        allServices.add(mCarInputService);
        allServices.add(mGarageModeService);
        addServiceIfNonNull(allServices, mCarUserNoticeService);
        allServices.add(mAppFocusService);
        allServices.add(mCarAudioService);
        allServices.add(mCarNightService);
        allServices.add(mFixedActivityService);
        allServices.add(mInstrumentClusterService);
        allServices.add(mSystemStateControllerService);
        allServices.add(mPerUserCarServiceHelper);
        allServices.add(mCarBluetoothService);
        allServices.add(mCarProjectionService);
        addServiceIfNonNull(allServices, mCarDiagnosticService);
        addServiceIfNonNull(allServices, mCarStorageMonitoringService);
        allServices.add(mCarConfigurationService);
        addServiceIfNonNull(allServices, mVmsBrokerService);
        allServices.add(mCarTrustedDeviceService);
        allServices.add(mCarMediaService);
        allServices.add(mCarLocationService);
        allServices.add(mCarBugreportManagerService);
        allServices.add(mCarWatchdogService);
        // Always put mCarExperimentalFeatureServiceController in last.
        addServiceIfNonNull(allServices, mCarExperimentalFeatureServiceController);
        mAllServices = allServices.toArray(new CarServiceBase[allServices.size()]);
    }

    private void addServiceIfNonNull(List<CarServiceBase> services, CarServiceBase service) {
        if (service != null) {
            services.add(service);
        }
    }

    @MainThread
    void init() {
        mBootTiming = new TimingsTraceLog(VHAL_TIMING_TAG, Trace.TRACE_TAG_HAL);
        traceBegin("VehicleHal.init");
        mHal.init();
        traceEnd();
        traceBegin("CarService.initAllServices");
        for (CarServiceBase service : mAllServices) {
            service.init();
        }
        traceEnd();
    }

    void release() {
        // release done in opposite order from init
        for (int i = mAllServices.length - 1; i >= 0; i--) {
            mAllServices[i].release();
        }
        mHal.release();
    }

    void vehicleHalReconnected(IVehicle vehicle) {
        EventLog.writeEvent(EventLogTags.CAR_SERVICE_VHAL_RECONNECTED, mAllServices.length);
        mHal.vehicleHalReconnected(vehicle);
        for (CarServiceBase service : mAllServices) {
            service.vehicleHalReconnected();
        }
    }

    @Override
    public void setCarServiceHelper(IBinder helper) {
        EventLog.writeEvent(EventLogTags.CAR_SERVICE_SET_CAR_SERVICE_HELPER,
                Binder.getCallingPid());
        assertCallingFromSystemProcess();
        ICarServiceHelper carServiceHelper = ICarServiceHelper.Stub.asInterface(helper);
        synchronized (mLock) {
            mICarServiceHelper = carServiceHelper;
        }
        mSystemInterface.setCarServiceHelper(carServiceHelper);
        mCarOccupantZoneService.setCarServiceHelper(carServiceHelper);
    }

    @Override
    public void onUserLifecycleEvent(int eventType, long timestampMs, int fromUserId,
            int toUserId) {
        assertCallingFromSystemProcess();
        EventLog.writeEvent(EventLogTags.CAR_SERVICE_ON_USER_LIFECYCLE, eventType, fromUserId,
                toUserId);
        if (DBG) {
            Log.d(TAG, "onUserLifecycleEvent("
                    + CarUserManager.lifecycleEventTypeToString(eventType) + ", " + toUserId + ")");
        }
        mCarUserService.onUserLifecycleEvent(eventType, timestampMs, fromUserId, toUserId);
    }

    @Override
    public void onFirstUserUnlocked(int userId, long timestampMs, long duration,
            int halResponseTime) {
        mCarUserService.onFirstUserUnlocked(userId, timestampMs, duration, halResponseTime);
    }

    @Override
    public void getInitialUserInfo(int requestType, int timeoutMs, IBinder binder) {
        IResultReceiver receiver = IResultReceiver.Stub.asInterface(binder);
        mCarUserService.getInitialUserInfo(requestType, timeoutMs, receiver);
    }

    @Override
    public void setInitialUser(int userId) {
        EventLog.writeEvent(EventLogTags.CAR_SERVICE_SET_INITIAL_USER, userId);
        if (DBG) Log.d(TAG, "setInitialUser(): " + userId);
        mCarUserService.setInitialUser(userId);
    }

    @Override
    public boolean isFeatureEnabled(String featureName) {
        return mFeatureController.isFeatureEnabled(featureName);
    }

    @Override
    public int enableFeature(String featureName) {
        // permission check inside the controller
        return mFeatureController.enableFeature(featureName);
    }

    @Override
    public int disableFeature(String featureName) {
        // permission check inside the controller
        return mFeatureController.disableFeature(featureName);
    }

    @Override
    public List<String> getAllEnabledFeatures() {
        // permission check inside the controller
        return mFeatureController.getAllEnabledFeatures();
    }

    @Override
    public List<String> getAllPendingDisabledFeatures() {
        // permission check inside the controller
        return mFeatureController.getAllPendingDisabledFeatures();
    }

    @Override
    public List<String> getAllPendingEnabledFeatures() {
        // permission check inside the controller
        return mFeatureController.getAllPendingEnabledFeatures();
    }

    @Override
    public String getCarManagerClassForFeature(String featureName) {
        if (mCarExperimentalFeatureServiceController == null) {
            return null;
        }
        return mCarExperimentalFeatureServiceController.getCarManagerClassForFeature(featureName);
    }

    static void assertCallingFromSystemProcess() {
        int uid = Binder.getCallingUid();
        if (uid != Process.SYSTEM_UID) {
            throw new SecurityException("Only allowed from system");
        }
    }

    /**
     * Assert if binder call is coming from system process like system server or if it is called
     * from its own process even if it is not system. The latter can happen in test environment.
     * Note that car service runs as system user but test like car service test will not.
     */
    static void assertCallingFromSystemProcessOrSelf() {
        int uid = Binder.getCallingUid();
        int pid = Binder.getCallingPid();
        if (uid != Process.SYSTEM_UID && pid != Process.myPid()) {
            throw new SecurityException("Only allowed from system or self");
        }
    }

    @Override
    public IBinder getCarService(String serviceName) {
        if (!mFeatureController.isFeatureEnabled(serviceName)) {
            Log.w(CarLog.TAG_SERVICE, "getCarService for disabled service:" + serviceName);
            return null;
        }
        switch (serviceName) {
            case Car.AUDIO_SERVICE:
                return mCarAudioService;
            case Car.APP_FOCUS_SERVICE:
                return mAppFocusService;
            case Car.PACKAGE_SERVICE:
                return mCarPackageManagerService;
            case Car.DIAGNOSTIC_SERVICE:
                assertAnyDiagnosticPermission(mContext);
                return mCarDiagnosticService;
            case Car.POWER_SERVICE:
                assertPowerPermission(mContext);
                return mCarPowerManagementService;
            case Car.CABIN_SERVICE:
            case Car.HVAC_SERVICE:
            case Car.INFO_SERVICE:
            case Car.PROPERTY_SERVICE:
            case Car.SENSOR_SERVICE:
            case Car.VENDOR_EXTENSION_SERVICE:
                return mCarPropertyService;
            case Car.CAR_NAVIGATION_SERVICE:
                assertNavigationManagerPermission(mContext);
                IInstrumentClusterNavigation navService =
                        mInstrumentClusterService.getNavigationService();
                return navService == null ? null : navService.asBinder();
            case Car.CAR_INSTRUMENT_CLUSTER_SERVICE:
                assertClusterManagerPermission(mContext);
                return mInstrumentClusterService.getManagerService();
            case Car.PROJECTION_SERVICE:
                return mCarProjectionService;
            case Car.VEHICLE_MAP_SERVICE:
                assertAnyVmsPermission(mContext);
                return mVmsBrokerService;
            case Car.VMS_SUBSCRIBER_SERVICE:
                assertVmsSubscriberPermission(mContext);
                return mVmsBrokerService;
            case Car.TEST_SERVICE: {
                assertPermission(mContext, Car.PERMISSION_CAR_TEST_SERVICE);
                synchronized (mLock) {
                    if (mCarTestService == null) {
                        mCarTestService = new CarTestService(mContext, this);
                    }
                    return mCarTestService;
                }
            }
            case Car.BLUETOOTH_SERVICE:
                return mCarBluetoothService;
            case Car.STORAGE_MONITORING_SERVICE:
                assertPermission(mContext, Car.PERMISSION_STORAGE_MONITORING);
                return mCarStorageMonitoringService;
            case Car.CAR_DRIVING_STATE_SERVICE:
                assertDrivingStatePermission(mContext);
                return mCarDrivingStateService;
            case Car.CAR_UX_RESTRICTION_SERVICE:
                return mCarUXRestrictionsService;
            case Car.OCCUPANT_AWARENESS_SERVICE:
                return mOccupantAwarenessService;
            case Car.CAR_CONFIGURATION_SERVICE:
                return mCarConfigurationService;
            case Car.CAR_TRUST_AGENT_ENROLLMENT_SERVICE:
                assertTrustAgentEnrollmentPermission(mContext);
                return mCarTrustedDeviceService.getCarTrustAgentEnrollmentService();
            case Car.CAR_MEDIA_SERVICE:
                return mCarMediaService;
            case Car.CAR_OCCUPANT_ZONE_SERVICE:
                return mCarOccupantZoneService;
            case Car.CAR_BUGREPORT_SERVICE:
                return mCarBugreportManagerService;
            case Car.CAR_USER_SERVICE:
                return mCarUserService;
            case Car.CAR_WATCHDOG_SERVICE:
                return mCarWatchdogService;
            case Car.CAR_INPUT_SERVICE:
                return mCarInputService;
            default:
                IBinder service = null;
                if (mCarExperimentalFeatureServiceController != null) {
                    service = mCarExperimentalFeatureServiceController.getCarService(serviceName);
                }
                if (service == null) {
                    Log.w(CarLog.TAG_SERVICE, "getCarService for unknown service:"
                            + serviceName);
                }
                return service;
        }
    }

    @Override
    public int getCarConnectionType() {
        return Car.CONNECTION_TYPE_EMBEDDED;
    }

    public CarServiceBase getCarInternalService(String serviceName) {
        switch (serviceName) {
            case INTERNAL_INPUT_SERVICE:
                return mCarInputService;
            case INTERNAL_SYSTEM_ACTIVITY_MONITORING_SERVICE:
                return mSystemActivityMonitoringService;
            default:
                Log.w(CarLog.TAG_SERVICE, "getCarInternalService for unknown service:" +
                        serviceName);
                return null;
        }
    }

    public static void assertVehicleHalMockPermission(Context context) {
        assertPermission(context, Car.PERMISSION_MOCK_VEHICLE_HAL);
    }

    public static void assertNavigationManagerPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_NAVIGATION_MANAGER);
    }

    public static void assertClusterManagerPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_INSTRUMENT_CLUSTER_CONTROL);
    }

    public static void assertPowerPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_POWER);
    }

    public static void assertProjectionPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_PROJECTION);
    }

    /** Verify the calling context has the {@link Car#PERMISSION_CAR_PROJECTION_STATUS} */
    public static void assertProjectionStatusPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_PROJECTION_STATUS);
    }

    public static void assertAnyDiagnosticPermission(Context context) {
        assertAnyPermission(context,
                Car.PERMISSION_CAR_DIAGNOSTIC_READ_ALL,
                Car.PERMISSION_CAR_DIAGNOSTIC_CLEAR);
    }

    public static void assertDrivingStatePermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_DRIVING_STATE);
    }

    /**
     * Verify the calling context has either {@link Car#PERMISSION_VMS_SUBSCRIBER} or
     * {@link Car#PERMISSION_VMS_PUBLISHER}
     */
    public static void assertAnyVmsPermission(Context context) {
        assertAnyPermission(context,
                Car.PERMISSION_VMS_SUBSCRIBER,
                Car.PERMISSION_VMS_PUBLISHER);
    }

    public static void assertVmsPublisherPermission(Context context) {
        assertPermission(context, Car.PERMISSION_VMS_PUBLISHER);
    }

    public static void assertVmsSubscriberPermission(Context context) {
        assertPermission(context, Car.PERMISSION_VMS_SUBSCRIBER);
    }

    /**
     * Ensures the caller has the permission to enroll a Trust Agent.
     */
    public static void assertTrustAgentEnrollmentPermission(Context context) {
        assertPermission(context, Car.PERMISSION_CAR_ENROLL_TRUST);
    }

    public static void assertPermission(Context context, String permission) {
        if (context.checkCallingOrSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
            throw new SecurityException("requires " + permission);
        }
    }

    /**
     * Checks to see if the caller has a permission.
     *
     * @return boolean TRUE if caller has the permission.
     */
    public static boolean hasPermission(Context context, String permission) {
        return context.checkCallingOrSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
    }

    public static void assertAnyPermission(Context context, String... permissions) {
        for (String permission : permissions) {
            if (context.checkCallingOrSelfPermission(permission)
                    == PackageManager.PERMISSION_GRANTED) {
                return;
            }
        }
        throw new SecurityException("requires any of " + Arrays.toString(permissions));
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        if (mContext.checkCallingOrSelfPermission(android.Manifest.permission.DUMP)
                != PackageManager.PERMISSION_GRANTED) {
            writer.println("Permission Denial: can't dump CarService from from pid="
                    + Binder.getCallingPid() + ", uid=" + Binder.getCallingUid()
                    + " without permission " + android.Manifest.permission.DUMP);
            return;
        }

        if (args == null || args.length == 0 || (args.length > 0 && "-a".equals(args[0]))) {
            writer.println("*Dump car service*");
            dumpAllServices(writer);
            dumpAllHals(writer);
        } else if ("--list".equals(args[0])) {
            dumpListOfServices(writer);
            return;
        } else if ("--services".equals(args[0])) {
            if (args.length < 2) {
                writer.println("Must pass services to dump when using --services");
                return;
            }
            int length = args.length - 1;
            String[] services = new String[length];
            System.arraycopy(args, 1, services, 0, length);
            dumpIndividualServices(writer, services);
            return;
        } else if ("--metrics".equals(args[0])) {
            // Strip the --metrics flag when passing dumpsys arguments to CarStatsService
            // allowing for nested flag selection
            mCarStatsService.dump(fd, writer, Arrays.copyOfRange(args, 1, args.length));
        } else if ("--vms-hal".equals(args[0])) {
            mHal.getVmsHal().dumpMetrics(fd);
        } else if ("--hal".equals(args[0])) {
            if (args.length == 1) {
                dumpAllHals(writer);
                return;
            }
            int length = args.length - 1;
            String[] halNames = new String[length];
            System.arraycopy(args, 1, halNames, 0, length);
            mHal.dumpSpecificHals(writer, halNames);

        } else if ("--list-hals".equals(args[0])) {
            mHal.dumpListHals(writer);
            return;
        } else if ("--user-metrics".equals(args[0])) {
            mCarUserService.dumpUserMetrics(writer);
        } else if ("--first-user-metrics".equals(args[0])) {
            mCarUserService.dumpFirstUserUnlockDuration(writer);
        } else if ("--help".equals(args[0])) {
            showDumpHelp(writer);
        } else {
            execShellCmd(args, writer);
        }
    }

    private void dumpAllHals(PrintWriter writer) {
        writer.println("*Dump Vehicle HAL*");
        writer.println("Vehicle HAL Interface: " + mVehicleInterfaceName);
        try {
            // TODO dump all feature flags by creating a dumpable interface
            mHal.dump(writer);
        } catch (Exception e) {
            writer.println("Failed dumping: " + mHal.getClass().getName());
            e.printStackTrace(writer);
        }
    }

    private void showDumpHelp(PrintWriter writer) {
        writer.println("Car service dump usage:");
        writer.println("[NO ARG]");
        writer.println("\t  dumps everything (all services and HALs)");
        writer.println("--help");
        writer.println("\t  shows this help");
        writer.println("--list");
        writer.println("\t  lists the name of all services");
        writer.println("--list-hals");
        writer.println("\t  lists the name of all HALs");
        writer.println("--services <SVC1> [SVC2] [SVCN]");
        writer.println("\t  dumps just the specific services, where SVC is just the service class");
        writer.println("\t  name (like CarUserService)");
        writer.println("--vms-hal");
        writer.println("\t  dumps the VMS HAL metrics");
        writer.println("--hal [HAL1] [HAL2] [HALN]");
        writer.println("\t  dumps just the specified HALs (or all of them if none specified),");
        writer.println("\t  where HAL is just the class name (like UserHalService)");
        writer.println("--user-metrics");
        writer.println("\t  dumps user switching and stopping metrics ");
        writer.println("--first-user-metrics");
        writer.println("\t  dumps how long it took to unlock first user since Android started\n");
        writer.println("\t  (or -1 if not unlocked)");
        writer.println("-h");
        writer.println("\t  shows commands usage (NOTE: commands are not available on USER builds");
        writer.println("[ANYTHING ELSE]");
        writer.println("\t  runs the given command (use --h to see the available commands)");
    }

    @Override
    public void onShellCommand(FileDescriptor in, FileDescriptor out, FileDescriptor err,
            String[] args, ShellCallback callback, ResultReceiver resultReceiver)
                    throws RemoteException {
        newCarShellCommand().exec(this, in, out, err, args, callback, resultReceiver);
    }

    private CarShellCommand newCarShellCommand() {
        return new CarShellCommand(mContext, mHal, mCarAudioService, mCarPackageManagerService,
                mCarProjectionService, mCarPowerManagementService, mCarTrustedDeviceService,
                mFixedActivityService, mFeatureController, mCarInputService, mCarNightService,
                mSystemInterface, mGarageModeService, mCarUserService, mCarOccupantZoneService);
    }

    private void dumpListOfServices(PrintWriter writer) {
        for (CarServiceBase service : mAllServices) {
            writer.println(service.getClass().getName());
        }
    }

    private void dumpAllServices(PrintWriter writer) {
        writer.println("*Dump all services*");
        for (CarServiceBase service : mAllServices) {
            dumpService(service, writer);
        }
        if (mCarTestService != null) {
            dumpService(mCarTestService, writer);
        }
    }

    private void dumpIndividualServices(PrintWriter writer, String... serviceNames) {
        for (String serviceName : serviceNames) {
            writer.println("** Dumping " + serviceName + "\n");
            CarServiceBase service = getCarServiceBySubstring(serviceName);
            if (service == null) {
                writer.println("No such service!");
            } else {
                dumpService(service, writer);
            }
            writer.println();
        }
    }

    @Nullable
    private CarServiceBase getCarServiceBySubstring(String className) {
        return Arrays.asList(mAllServices).stream()
                .filter(s -> s.getClass().getSimpleName().equals(className))
                .findFirst().orElse(null);
    }

    private void dumpService(CarServiceBase service, PrintWriter writer) {
        try {
            service.dump(writer);
        } catch (Exception e) {
            writer.println("Failed dumping: " + service.getClass().getName());
            e.printStackTrace(writer);
        }
    }

    void execShellCmd(String[] args, PrintWriter writer) {
        newCarShellCommand().exec(args, writer);
    }

    @MainThread
    private void traceBegin(String name) {
        Slog.i(TAG, name);
        mBootTiming.traceBegin(name);
    }

    @MainThread
    private void traceEnd() {
        mBootTiming.traceEnd();
    }
}
