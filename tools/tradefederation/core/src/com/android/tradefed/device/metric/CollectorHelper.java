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

package com.android.tradefed.device.metric;

import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.testtype.IRemoteTest;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.List;

/** Helper to do some {@link IMetricCollector} operations needed in several places. */
public class CollectorHelper {

    /**
     * Helper to clone {@link IMetricCollector}s in order for each {@link IRemoteTest} to get a
     * different instance, and avoid internal state and multi-init issues.
     *
     * @param originalCollectors the list of original collectors to be cloned.
     * @return The list of cloned {@link IMetricCollector}.
     */
    public static List<IMetricCollector> cloneCollectors(
            List<IMetricCollector> originalCollectors) {
        List<IMetricCollector> cloneList = new ArrayList<>();
        if (originalCollectors == null) {
            return cloneList;
        }
        for (IMetricCollector collector : originalCollectors) {
            try {
                // TF object should all have a constructore with no args, so this should be safe.
                IMetricCollector clone =
                        collector.getClass().getDeclaredConstructor().newInstance();
                OptionCopier.copyOptionsNoThrow(collector, clone);
                cloneList.add(clone);
            } catch (InstantiationException
                    | IllegalAccessException
                    | InvocationTargetException
                    | NoSuchMethodException e) {
                throw new RuntimeException(e);
            }
        }
        return cloneList;
    }
}
