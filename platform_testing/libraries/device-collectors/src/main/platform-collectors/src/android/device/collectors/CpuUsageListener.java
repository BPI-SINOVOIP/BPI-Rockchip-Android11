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
package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;

import com.android.helpers.CpuUsageHelper;

/**
 * A {@link CpuUsageListener} that captures cpu usage during the test method.
 *
 * Do NOT throw exception anywhere in this class. We don't want to halt the test when metrics
 * collection fails.
 */
@OptionClass(alias = "cpuusage-collector")
public class CpuUsageListener extends BaseCollectionListener<Long> {

    private static final String DISABLE_PER_PACKAGE = "disable_per_pkg";
    private static final String DISABLE_PER_FREQ = "disable_per_freq";
    private static final String DISABLE_TOTAL_PKG = "disable_total_pkg";
    private static final String DISABLE_TOTAL_FREQ = "disable_total_freq";
    private static final String ENABLE_CPU_UTILIZATION = "enable_cpu_utilization";

    public CpuUsageListener() {
        createHelperInstance(new CpuUsageHelper());
    }

    /**
     * Adds the options to filter the cpu usage metrics collected.
     */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        CpuUsageHelper cpuUsageHelper = (CpuUsageHelper) mHelper;
        if ("true".equals(args.getString(DISABLE_PER_PACKAGE))) {
            cpuUsageHelper.setDisablePerPackage();
        }

        if ("true".equals(args.getString(DISABLE_PER_FREQ))) {
            cpuUsageHelper.setDisablePerFrequency();
        }

        if ("true".equals(args.getString(DISABLE_TOTAL_PKG))) {
            cpuUsageHelper.setDisableTotalPackage();
        }

        if ("true".equals(args.getString(DISABLE_TOTAL_FREQ))) {
            cpuUsageHelper.setDisableTotalFrequency();
        }

        if ("true".equals(args.getString(ENABLE_CPU_UTILIZATION))) {
            cpuUsageHelper.setEnableCpuUtilization();
        }
    }
}

