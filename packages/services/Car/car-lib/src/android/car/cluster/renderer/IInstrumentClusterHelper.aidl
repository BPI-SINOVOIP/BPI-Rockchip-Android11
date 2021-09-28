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
package android.car.cluster.renderer;

import android.content.Intent;
import android.os.Bundle;

/**
 * Helper binder API for InstrumentClusterRenderingService. This contains binder calls to car
 * service.
 *
 * @hide
 */
interface IInstrumentClusterHelper {
    /**
     * Start an activity to specified display / user. The activity is considered as
     * in fixed mode for the display and will be re-launched if the activity crashes, the package
     * is updated or goes to background for whatever reason.
     * Only one activity can exist in fixed mode for the target display and calling this multiple
     * times with different {@code Intent} will lead into making all previous activities into
     * non-fixed normal state (= will not be re-launched.)
     *
     * Do not change binder transaction number.
     */
    boolean startFixedActivityModeForDisplayAndUser(in Intent intent,
            in Bundle activityOptionsBundle, int userId) = 0;
    /**
     * The activity lauched on the display is no longer in fixed mode. Re-launching or finishing
     * should not trigger re-launfhing any more. Note that Activity for non-current user will
     * be auto-stopped and there is no need to call this for user swiching. Note that this does not
     * stop the activity but it will not be re-launched any more.
     *
     * Do not change binder transaction number.
     */
    void stopFixedActivityMode(int displayId) = 1;
}
