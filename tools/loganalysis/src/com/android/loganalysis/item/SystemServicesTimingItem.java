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

package com.android.loganalysis.item;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * An {@link IItem} used to store timing information for system services like System Server, Zygote,
 * System UI, BootAnimation e.t.c.
 */
public class SystemServicesTimingItem extends GenericItem {
    /** Constant for JSON output */
    public static final String COMPONENT = "COMPONENT";
    /** Constant for JSON output */
    public static final String SUBCOMPONENT = "SUBCOMPONENT";
    /** Constant for JSON output */
    public static final String DURATION = "DURATION";
    /** Constant for JSON output */
    public static final String START_TIME = "START_TIME";

    private static final Set<String> ATTRIBUTES =
            new HashSet<>(Arrays.asList(COMPONENT, SUBCOMPONENT, DURATION, START_TIME));

    /** Constructor for {@link SystemServicesTimingItem} */
    public SystemServicesTimingItem() {
        super(ATTRIBUTES);
    }

    /** Get the component name for system services timing */
    public String getComponent() {
        return (String) getAttribute(COMPONENT);
    }

    /** Set the component name for system service timing */
    public void setComponent(String component) {
        setAttribute(COMPONENT, component);
    }

    /** Get the sub-component name for system service timing */
    public String getSubcomponent() {
        return (String) getAttribute(SUBCOMPONENT);
    }

    /** Set the sub-component name for system service timing */
    public void setSubcomponent(String subcomponent) {
        setAttribute(SUBCOMPONENT, subcomponent);
    }

    /** Get the duration value for system service timing */
    public Double getDuration() {
        return (Double) getAttribute(DURATION);
    }

    /** Set the duration value for system service timing */
    public void setDuration(double duration) {
        setAttribute(DURATION, duration);
    }

    /** Get the start time value for system service timing */
    public Double getStartTime() {
        return (Double) getAttribute(START_TIME);
    }

    /** Set the start time value for system service timing */
    public void setStartTime(double startTime) {
        setAttribute(START_TIME, startTime);
    }
}
