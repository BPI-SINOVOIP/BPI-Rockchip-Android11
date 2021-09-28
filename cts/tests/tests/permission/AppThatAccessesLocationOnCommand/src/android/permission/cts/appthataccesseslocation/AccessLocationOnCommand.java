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

package android.permission.cts.appthataccesseslocation;

import static android.location.Criteria.ACCURACY_FINE;

import android.app.Service;
import android.content.Intent;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

public class AccessLocationOnCommand extends Service {
    // Longer than the STATE_SETTLE_TIME in AppOpsManager
    private static final long BACKGROUND_ACCESS_SETTLE_TIME = 11000;

    private IAccessLocationOnCommand.Stub mBinder = new IAccessLocationOnCommand.Stub() {
        public void accessLocation() {
            new Handler(Looper.getMainLooper()).postDelayed(() -> {
                Criteria crit = new Criteria();
                crit.setAccuracy(ACCURACY_FINE);

                AccessLocationOnCommand.this.getSystemService(LocationManager.class)
                        .requestSingleUpdate(crit, new LocationListener() {
                            @Override
                            public void onLocationChanged(Location location) {
                            }

                            @Override
                            public void onStatusChanged(String provider, int status,
                                    Bundle extras) {
                            }

                            @Override
                            public void onProviderEnabled(String provider) {
                            }

                            @Override
                            public void onProviderDisabled(String provider) {
                            }
                        }, null);
            }, BACKGROUND_ACCESS_SETTLE_TIME);
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return true;
    }
}
