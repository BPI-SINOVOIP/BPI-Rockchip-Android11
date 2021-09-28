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

package com.android.loganalysis.parser;


import com.android.loganalysis.item.GenericTimingItem;
import com.android.loganalysis.item.IItem;
import com.android.loganalysis.item.SystemServicesTimingItem;

import java.io.BufferedReader;
import java.io.IOException;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * An {@link IParser} to parse boot metrics from logcat. It currently assumes "threadtime" format of
 * logcat. It will parse duration metrics for some system services like System Server, Zygote,
 * System UI, e.t.c.
 *
 */
public class TimingsLogParser implements IParser {

    private static final String SYSTEM_SERVICES_TIME_PREFIX =
            "^\\d*-\\d*\\s*\\d*:\\d*:\\d*.\\d*\\s*"
                    + "\\d*\\s*\\d*\\s*D\\s*(?<componentname>.*):\\s*(?<subname>\\S*)\\s*";
    private static final String SYSTEM_SERVICES_TIME_SUFFIX = ":\\s*(?<time>.*)ms\\s*$";

    /** Used to parse time info from logcat lines */
    private static final DateFormat DEFAULT_TIME_FORMAT =
            new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

    /**
     * Year is not important in timing info. Always use Unix time starting year for timestamp
     * conversion with simplicity
     */
    private static final String DEFAULT_YEAR = "1970";

    /**
     * Match the line with system services duration info like:
     *
     * <p>03-10 21:43:40.328 1005 1005 D SystemServerTiming:
     * StartKeyAttestationApplicationIdProviderService took to complete: 3474ms
     */
    private static final Pattern SYSTEM_SERVICES_DURATION =
            Pattern.compile(
                    String.format(
                            "%stook to complete%s",
                            SYSTEM_SERVICES_TIME_PREFIX, SYSTEM_SERVICES_TIME_SUFFIX));
    /**
     * Match the line with system services start time info like:
     *
     * <p>01-10 01:25:57.249 989 989 D BootAnimation: BootAnimationStartTiming start time: 8343ms
     */
    private static final Pattern SYSTEM_SERVICES_START_TIME =
            Pattern.compile(
                    String.format(
                            "%sstart time%s",
                            SYSTEM_SERVICES_TIME_PREFIX, SYSTEM_SERVICES_TIME_SUFFIX));

    private List<DurationPattern> durationPatterns = new ArrayList<>();

    private static class DurationPattern {
        String mName;
        Pattern mStartTimePattern;
        Pattern mEndTimePattern;

        DurationPattern(String name, Pattern startTimePattern, Pattern endTimePattern) {
            mName = name;
            mStartTimePattern = startTimePattern;
            mEndTimePattern = endTimePattern;
        }
    }

    @Override
    public IItem parse(List<String> lines) {
        throw new UnsupportedOperationException(
                "Method has not been implemented in lieu of others");
    }

    /**
     * Add a pair of patterns matching the start and end signals in logcat for a duration metric
     *
     * @param name the name of the duration metric, it should be unique
     * @param startTimePattern the pattern matches the logcat line indicating start of duration
     * @param endTimePattern the pattern matches the logcat line indicating end of duration
     */
    public void addDurationPatternPair(
            String name, Pattern startTimePattern, Pattern endTimePattern) {
        DurationPattern durationPattern =
                new DurationPattern(name, startTimePattern, endTimePattern);
        durationPatterns.add(durationPattern);
    }

    /** Cleanup added duration patterns */
    public void clearDurationPatterns() {
        durationPatterns.clear();
    }

    /**
     * Parse logcat lines with the added duration patterns and generate a list of {@link
     * GenericTimingItem} representing the duration items. Note, a duration pattern could have zero
     * or multiple matches
     *
     * @param input logcat lines
     * @return list of {@link GenericTimingItem}
     * @throws IOException
     */
    public List<GenericTimingItem> parseGenericTimingItems(BufferedReader input)
            throws IOException {
        List<GenericTimingItem> items = new ArrayList<>();
        String line;
        //Timing items that are partially matched after captured start time
        Map<String, Double> pendingItems = new HashMap<>();
        while ((line = input.readLine()) != null) {
            items.addAll(parseGenericTimingItem(line, pendingItems));
        }
        return items;
    }

    /**
     * Parse a particular logcat line to get duration items. One line could have be matched by
     * multiple patterns
     *
     * @param line logcat line
     * @param pendingItems timing items that are half-matched with only start time.
     * @return list of {@link GenericTimingItem}
     */
    private List<GenericTimingItem> parseGenericTimingItem(
            String line, Map<String, Double> pendingItems) {
        List<GenericTimingItem> items = new ArrayList<>();
        for (DurationPattern durationPattern : durationPatterns) {
            Matcher matcher = durationPattern.mStartTimePattern.matcher(line);
            if (matcher.find()) {
                double startTimeMillis = parseTime(line);
                pendingItems.put(durationPattern.mName, startTimeMillis);
                continue;
            }
            matcher = durationPattern.mEndTimePattern.matcher(line);
            if (matcher.find()) {
                double endTimeMillis = parseTime(line);
                Double startTimeMillis = pendingItems.get(durationPattern.mName);
                if (startTimeMillis != null) {
                    GenericTimingItem newItem = new GenericTimingItem();
                    newItem.setName(durationPattern.mName);
                    newItem.setStartAndEnd(startTimeMillis, endTimeMillis);
                    items.add(newItem);
                    pendingItems.remove(durationPattern.mName);
                }
            }
        }
        return items;
    }

    /**
     * Get timestamp of current logcat line based on DEFAULT_YEAR
     *
     * @param line logcat line
     * @return timestamp
     */
    private double parseTime(String line) {
        String timeStr = String.format("%s-%s", DEFAULT_YEAR, line);
        try {
            return DEFAULT_TIME_FORMAT.parse(timeStr).getTime();
        } catch (ParseException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * A method that parses the logcat input for system services timing information. It will ignore
     * duplicated log lines and will keep multiple values for the same timing metric generated at
     * different time in the log
     *
     * @param input Logcat input as a {@link BufferedReader}
     * @return a list of {@link SystemServicesTimingItem}
     * @throws IOException
     */
    public List<SystemServicesTimingItem> parseSystemServicesTimingItems(BufferedReader input)
            throws IOException {
        Set<String> matchedLines = new HashSet<>();
        List<SystemServicesTimingItem> items = new ArrayList<>();
        String line;
        while ((line = input.readLine()) != null) {
            if (matchedLines.contains(line)) {
                continue;
            }
            SystemServicesTimingItem item = parseSystemServicesTimingItem(line);
            if (item == null) {
                continue;
            }
            items.add(item);
            matchedLines.add(line);
        }
        return items;
    }

    /**
     * Parse a particular log line to see if it matches the system service timing pattern and return
     * a {@link SystemServicesTimingItem} if matches, otherwise return null.
     *
     * @param line a single log line
     * @return a {@link SystemServicesTimingItem}
     */
    private SystemServicesTimingItem parseSystemServicesTimingItem(String line) {
        Matcher matcher = SYSTEM_SERVICES_DURATION.matcher(line);
        boolean durationMatched = matcher.matches();
        if (!durationMatched) {
            matcher = SYSTEM_SERVICES_START_TIME.matcher(line);
        }
        if (!matcher.matches()) {
            return null;
        }
        SystemServicesTimingItem item = new SystemServicesTimingItem();
        item.setComponent(matcher.group("componentname").trim());
        item.setSubcomponent(matcher.group("subname").trim());
        if (durationMatched) {
            item.setDuration(Double.parseDouble(matcher.group("time")));
        } else {
            item.setStartTime(Double.parseDouble(matcher.group("time")));
        }
        return item;
    }
}
