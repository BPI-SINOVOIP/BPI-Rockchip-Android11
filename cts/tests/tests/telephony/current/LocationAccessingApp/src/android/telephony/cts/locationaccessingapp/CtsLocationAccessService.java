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
 * limitations under the License
 */

package android.telephony.cts.locationaccessingapp;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.CellInfo;
import android.telephony.CellLocation;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.TelephonyManager;
import android.telephony.cdma.CdmaCellLocation;
import android.telephony.gsm.GsmCellLocation;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class CtsLocationAccessService extends Service {
    public static final String CONTROL_ACTION =
            "android.telephony.cts.locationaccessingapp.ACTION_CONTROL";

    public static final String COMMAND_GET_SERVICE_STATE = "get_service_state";
    public static final String COMMAND_GET_CELL_INFO = "get_cell_info";
    public static final String COMMAND_GET_CELL_LOCATION = "get_cell_location";
    public static final String COMMAND_GET_SERVICE_STATE_FROM_LISTENER =
            "get_service_state_from_listener";
    public static final String COMMAND_LISTEN_CELL_LOCATION = "listen_cell_location";
    public static final String COMMAND_LISTEN_CELL_INFO = "listen_cell_info";
    public static final String COMMAND_REQUEST_CELL_INFO_UPDATE = "request_cell_info_update";

    private static final long LISTENER_TIMEOUT = 1000;

    private ICtsLocationAccessControl mBinder = new ICtsLocationAccessControl.Stub() {
        @Override
        public List performCommand(String command) throws RemoteException {
            Object result = null;
            switch (command) {
                case COMMAND_GET_SERVICE_STATE:
                    result = mTelephonyManager.getServiceState();
                    break;
                case COMMAND_GET_CELL_INFO:
                    result = mTelephonyManager.getAllCellInfo();
                    break;
                case COMMAND_GET_CELL_LOCATION:
                    result = new Bundle();
                    CellLocation cellLocation = mTelephonyManager.getCellLocation();
                    if (cellLocation instanceof GsmCellLocation) {
                        ((GsmCellLocation) cellLocation).fillInNotifierBundle((Bundle) result);
                    } else if (cellLocation instanceof CdmaCellLocation) {
                        ((CdmaCellLocation) cellLocation).fillInNotifierBundle((Bundle) result);
                    } else if (cellLocation == null) {
                        result = null;
                    } else {
                        throw new RuntimeException("Unexpected celllocation type");
                    }
                    break;
                case COMMAND_GET_SERVICE_STATE_FROM_LISTENER:
                    result = listenForServiceState();
                    break;
                case COMMAND_LISTEN_CELL_INFO:
                    result = listenForCellInfo();
                    break;
                case COMMAND_LISTEN_CELL_LOCATION:
                    result = listenForCellLocation();
                    break;
                case COMMAND_REQUEST_CELL_INFO_UPDATE:
                    result = requestCellInfoUpdate();
            }
            return Collections.singletonList(result);
        }
    };
    private TelephonyManager mTelephonyManager;

    @Override
    public IBinder onBind(Intent intent) {
        mTelephonyManager = getSystemService(TelephonyManager.class);
        return mBinder.asBinder();
    }

    private List<CellInfo> listenForCellInfo() {
        LinkedBlockingQueue<List<CellInfo>> queue = new LinkedBlockingQueue<>();
        HandlerThread handlerThread = new HandlerThread("Telephony location CTS");
        handlerThread.start();
        Executor executor = new Handler(handlerThread.getLooper())::post;
        PhoneStateListener listener = new PhoneStateListener(executor) {
            @Override
            public void onCellInfoChanged(List<CellInfo> cis) {
                if (cis == null) cis = Collections.emptyList();
                queue.offer(cis);
            }
        };

        mTelephonyManager.listen(listener, PhoneStateListener.LISTEN_SERVICE_STATE);

        try {
            return queue.poll(LISTENER_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException("interrupted");
        } finally {
            handlerThread.quit();
        }
    }

    private CellLocation listenForCellLocation() {
        LinkedBlockingQueue<CellLocation> queue = new LinkedBlockingQueue<>();
        HandlerThread handlerThread = new HandlerThread("Telephony location CTS");
        handlerThread.start();
        Executor executor = new Handler(handlerThread.getLooper())::post;
        PhoneStateListener listener = new PhoneStateListener(executor) {
            @Override
            public void onCellLocationChanged(CellLocation cellLocation) {
                if (cellLocation == null) return;
                queue.offer(cellLocation);
            }
        };

        mTelephonyManager.listen(listener, PhoneStateListener.LISTEN_SERVICE_STATE);

        try {
            return queue.poll(LISTENER_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException("interrupted");
        } finally {
            handlerThread.quit();
        }
    }

    private ServiceState listenForServiceState() {
        LinkedBlockingQueue<ServiceState> queue = new LinkedBlockingQueue<>();
        HandlerThread handlerThread = new HandlerThread("Telephony location CTS");
        handlerThread.start();
        Executor executor = new Handler(handlerThread.getLooper())::post;
        PhoneStateListener listener = new PhoneStateListener(executor) {
            @Override
            public void onServiceStateChanged(ServiceState ss) {
                if (ss == null) ss = new ServiceState();
                queue.offer(ss);
            }
        };

        mTelephonyManager.listen(listener, PhoneStateListener.LISTEN_SERVICE_STATE);

        try {
            ServiceState ss = queue.poll(LISTENER_TIMEOUT, TimeUnit.MILLISECONDS);
            if (ss == null) {
                throw new RuntimeException("Timed out waiting for service state");
            }
            return ss;
        } catch (InterruptedException e) {
            throw new RuntimeException("interrupted");
        } finally {
            handlerThread.quit();
        }
    }

    private List<CellInfo> requestCellInfoUpdate() {
        LinkedBlockingQueue<List<CellInfo>> queue = new LinkedBlockingQueue<>();
        HandlerThread handlerThread = new HandlerThread("Telephony location CTS");
        handlerThread.start();
        Executor executor = new Handler(handlerThread.getLooper())::post;
        TelephonyManager.CellInfoCallback cb = new TelephonyManager.CellInfoCallback() {
            @Override
            public void onCellInfo(List<CellInfo> cellInfo) {
                queue.offer(cellInfo);
            }
        };

        mTelephonyManager.requestCellInfoUpdate(executor, cb);

        try {
            List<CellInfo> ci = queue.poll(LISTENER_TIMEOUT, TimeUnit.MILLISECONDS);
            if (ci == null) {
                throw new RuntimeException("Timed out waiting for cell info");
            }
            return ci;
        } catch (InterruptedException e) {
            throw new RuntimeException("interrupted");
        } finally {
            handlerThread.quit();
        }
    }
}
