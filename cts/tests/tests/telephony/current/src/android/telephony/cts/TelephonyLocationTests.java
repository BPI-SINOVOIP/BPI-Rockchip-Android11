package android.telephony.cts;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.CellInfo;
import android.telephony.CellLocation;
import android.telephony.NetworkRegistrationInfo;
import android.telephony.ServiceState;
import android.telephony.cts.locationaccessingapp.CtsLocationAccessService;
import android.telephony.cts.locationaccessingapp.ICtsLocationAccessControl;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;

import java.util.List;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class TelephonyLocationTests {
    private static final String LOCATION_ACCESS_APP_CURRENT_PACKAGE =
            CtsLocationAccessService.class.getPackage().getName();

    private static final String LOCATION_ACCESS_APP_SDK28_PACKAGE =
            CtsLocationAccessService.class.getPackage().getName() + ".sdk28";

    private static final long TEST_TIMEOUT = 5000;

    private boolean mShouldTest = false;

    @Before
    public void setUp() {
        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        mShouldTest = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
    }

    @Test
    public void testCellLocationFinePermission() {
        if (!mShouldTest) return;
        Runnable cellLocationAccess = () -> {
            try {
                Bundle cellLocationBundle = (Bundle) performLocationAccessCommand(
                        CtsLocationAccessService.COMMAND_GET_CELL_LOCATION);
                CellLocation cellLocation = cellLocationBundle == null ? null :
                        CellLocation.newFromBundle(cellLocationBundle);
                assertTrue(cellLocation == null || cellLocation.isEmpty());
            } catch (SecurityException e) {
                // expected
            }

            try {
                List cis = (List) performLocationAccessCommand(
                        CtsLocationAccessService.COMMAND_GET_CELL_INFO);
                assertTrue(cis == null || cis.isEmpty());
            } catch (SecurityException e) {
                // expected
            }
        };

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE,
                cellLocationAccess, Manifest.permission.ACCESS_FINE_LOCATION);
        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, cellLocationAccess,
                Manifest.permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testServiceStateLocationSanitization() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    ServiceState ss = (ServiceState) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_GET_SERVICE_STATE);
                    assertServiceStateSanitization(ss, true);

                    withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                                ServiceState ss1 = (ServiceState) performLocationAccessCommand(
                                        CtsLocationAccessService.COMMAND_GET_SERVICE_STATE);
                                assertServiceStateSanitization(ss1, false);
                            },
                            Manifest.permission.ACCESS_COARSE_LOCATION);
                },
                Manifest.permission.ACCESS_FINE_LOCATION);

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    ServiceState ss1 = (ServiceState) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_GET_SERVICE_STATE);
                    assertServiceStateSanitization(ss1, false);
                },
                Manifest.permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testServiceStateListeningWithoutPermissions() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    ServiceState ss = (ServiceState) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_GET_SERVICE_STATE_FROM_LISTENER);
                    assertServiceStateSanitization(ss, true);

                    withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                                ServiceState ss1 = (ServiceState) performLocationAccessCommand(
                                        CtsLocationAccessService
                                                .COMMAND_GET_SERVICE_STATE_FROM_LISTENER);
                                assertServiceStateSanitization(ss1, false);
                            },
                            Manifest.permission.ACCESS_COARSE_LOCATION);
                },
                Manifest.permission.ACCESS_FINE_LOCATION);

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    ServiceState ss1 = (ServiceState) performLocationAccessCommand(
                            CtsLocationAccessService
                                    .COMMAND_GET_SERVICE_STATE_FROM_LISTENER);
                    assertServiceStateSanitization(ss1, false);
                },
                Manifest.permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testSdk28ServiceStateListeningWithoutPermissions() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
                    ServiceState ss = (ServiceState) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_GET_SERVICE_STATE_FROM_LISTENER);
                    assertNotNull(ss);
                    assertNotEquals(ss, ss.createLocationInfoSanitizedCopy(false));

                    withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
                                ServiceState ss1 = (ServiceState) performLocationAccessCommand(
                                        CtsLocationAccessService
                                                .COMMAND_GET_SERVICE_STATE_FROM_LISTENER);
                                assertNotNull(ss1);
                                assertNotEquals(ss1, ss1.createLocationInfoSanitizedCopy(true));
                            },
                            Manifest.permission.ACCESS_COARSE_LOCATION);
                },
                Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @Test
    public void testRegistryPermissionsForCellLocation() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    CellLocation cellLocation = (CellLocation) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_LOCATION);
                    assertNull(cellLocation);
                },
                Manifest.permission.ACCESS_FINE_LOCATION);

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    CellLocation cellLocation = (CellLocation) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_LOCATION);
                    assertNull(cellLocation);
                },
                Manifest.permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testSdk28RegistryPermissionsForCellLocation() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
                    CellLocation cellLocation = (CellLocation) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_LOCATION);
                    assertNull(cellLocation);
                },
                Manifest.permission.ACCESS_COARSE_LOCATION);
    }

    @Test
    public void testRegistryPermissionsForCellInfo() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    CellLocation cellLocation = (CellLocation) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_INFO);
                    assertNull(cellLocation);
                },
                Manifest.permission.ACCESS_FINE_LOCATION);

        withRevokedPermission(LOCATION_ACCESS_APP_CURRENT_PACKAGE, () -> {
                    CellLocation cellLocation = (CellLocation) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_INFO);
                    assertNull(cellLocation);
                },
                Manifest.permission.ACCESS_BACKGROUND_LOCATION);
    }

    @Test
    public void testSdk28RegistryPermissionsForCellInfo() {
        if (!mShouldTest) return;

        withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
                    List<CellInfo> cis = (List<CellInfo>) performLocationAccessCommand(
                            CtsLocationAccessService.COMMAND_LISTEN_CELL_INFO);
                    assertTrue(cis == null || cis.isEmpty());
                },
                Manifest.permission.ACCESS_COARSE_LOCATION);
    }

    @Test
    public void testSdk28CellLocation() {
        if (!mShouldTest) return;

        // Verify that a target-sdk 28 app can access cell location with ACCESS_COARSE_LOCATION, but
        // not with no location permissions at all.
        withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
            try {
                performLocationAccessCommandSdk28(
                        CtsLocationAccessService.COMMAND_GET_CELL_LOCATION);
            } catch (SecurityException e) {
                fail("SDK28 should have access to cell location with coarse permission");
            }

            withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
                try {
                    Bundle cellLocationBundle = (Bundle) performLocationAccessCommandSdk28(
                            CtsLocationAccessService.COMMAND_GET_CELL_LOCATION);
                    CellLocation cellLocation = cellLocationBundle == null ? null :
                            CellLocation.newFromBundle(cellLocationBundle);
                    assertTrue(cellLocation == null || cellLocation.isEmpty());
                } catch (SecurityException e) {
                    // expected
                }
            }, Manifest.permission.ACCESS_COARSE_LOCATION);
        }, Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @Test
    public void testSdk28CellInfoUpdate() {
        if (!mShouldTest) return;

        // Verify that a target-sdk 28 app still requires fine location access
        // to call requestCellInfoUpdate
        withRevokedPermission(LOCATION_ACCESS_APP_SDK28_PACKAGE, () -> {
            try {
                List<CellInfo> cis = (List<CellInfo>) performLocationAccessCommandSdk28(
                        CtsLocationAccessService.COMMAND_REQUEST_CELL_INFO_UPDATE);
                assertTrue(cis == null || cis.isEmpty());
            } catch (SecurityException e) {
                // expected
            }
        }, Manifest.permission.ACCESS_FINE_LOCATION);
    }

    private ICtsLocationAccessControl getLocationAccessAppControl() {
        Intent bindIntent = new Intent(CtsLocationAccessService.CONTROL_ACTION);
        bindIntent.setComponent(new ComponentName(
                LOCATION_ACCESS_APP_CURRENT_PACKAGE,
                CtsLocationAccessService.class.getName()));

        return bindLocationAccessControl(bindIntent);
    }

    private ICtsLocationAccessControl getLocationAccessAppControlSdk28() {
        Intent bindIntent = new Intent(CtsLocationAccessService.CONTROL_ACTION);
        bindIntent.setComponent(new ComponentName(
                LOCATION_ACCESS_APP_SDK28_PACKAGE,
                CtsLocationAccessService.class.getName()));

        return bindLocationAccessControl(bindIntent);
    }

    private ICtsLocationAccessControl bindLocationAccessControl(Intent bindIntent) {
        LinkedBlockingQueue<ICtsLocationAccessControl> pipe =
                new LinkedBlockingQueue<>();
        InstrumentationRegistry.getContext().bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                pipe.offer(ICtsLocationAccessControl.Stub.asInterface(service));
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {

            }
        }, Context.BIND_AUTO_CREATE);

        try {
            return pipe.poll(TEST_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            fail("interrupted");
        }
        fail("Unable to connect to location access test app");
        return null;
    }

    private Object performLocationAccessCommand(String command) {
        int tries = 0;
        while (tries < 5) {
            ICtsLocationAccessControl control = getLocationAccessAppControl();
            try {
                List ret = control.performCommand(command);
                if (!ret.isEmpty()) return ret.get(0);
            } catch (RemoteException e) {
                tries++;
            }
        }
        fail("Too many remote exceptions");
        return null;
    }

    private Object performLocationAccessCommandSdk28(String command) {
        ICtsLocationAccessControl control = getLocationAccessAppControlSdk28();
        try {
            List ret = control.performCommand(command);
            if (!ret.isEmpty()) return ret.get(0);
        } catch (RemoteException e) {
            fail("Remote exception");
        }
        return null;
    }

    private void withRevokedPermission(String packageName, Runnable r, String permission) {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().revokeRuntimePermission(packageName, permission);
        try {
            r.run();
        } finally {
            InstrumentationRegistry.getInstrumentation()
                    .getUiAutomation().grantRuntimePermission(packageName, permission);
        }
    }

    private void assertServiceStateSanitization(ServiceState state, boolean sanitizedForFineOnly) {
        if (state == null) return;

        if (state.getNetworkRegistrationInfoList() != null) {
            for (NetworkRegistrationInfo nrs : state.getNetworkRegistrationInfoList()) {
                assertNull(nrs.getCellIdentity());
            }
        }

        if (sanitizedForFineOnly) return;

        assertTrue(TextUtils.isEmpty(state.getOperatorAlphaLong()));
        assertTrue(TextUtils.isEmpty(state.getOperatorAlphaShort()));
        assertTrue(TextUtils.isEmpty(state.getOperatorNumeric()));
    }

}
