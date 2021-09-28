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

public class GenericTimingItem extends GenericItem {
    /** Constant for JSON output */
    public static final String NAME = "NAME";
    /** Constant for JSON output */
    public static final String START_TIME = "START_TIME";
    /** Constant for JSON output */
    public static final String END_TIME = "END_TIME";

    private static final Set<String> ATTRIBUTES =
            new HashSet<>(Arrays.asList(NAME, END_TIME, START_TIME));

    public GenericTimingItem() {
        super(ATTRIBUTES);
    }

    protected GenericTimingItem(Set<String> attributes) {
        super(getAllAttributes(attributes));
    }

    /** Get the name of the generic timing item */
    public String getName() {
        return (String) getAttribute(NAME);
    }

    /** Set the name of the generic timing item */
    public void setName(String name) {
        setAttribute(NAME, name);
    }

    /** Get the duration value for the generic timing, it is timestamp in milliseconds */
    public Double getDuration() {
        return (Double) getAttribute(END_TIME) - (Double) getAttribute(START_TIME);
    }

    /** Get the start time value for the generic timing, it is timestamp in milliseconds */
    public Double getStartTime() {
        return (Double) getAttribute(START_TIME);
    }

    /** Get the end time value for the generic timing, it is timestamp in milliseconds */
    public Double getEndTime() {
        return (Double) getAttribute(END_TIME);
    }

    /** Set the start and end time value for the generic timing, it is timestamp in milliseconds */
    public void setStartAndEnd(double startTime, double endTime) {
        setAttribute(START_TIME, startTime);
        setAttribute(END_TIME, endTime);
    }

    /** Combine an array of attributes with the internal list of attributes. */
    private static Set<String> getAllAttributes(Set<String> attributes) {
        Set<String> allAttributes = new HashSet<String>(ATTRIBUTES);
        allAttributes.addAll(attributes);
        return allAttributes;
    }
}
