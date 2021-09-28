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

package com.android.phone.testapps.telephonymanagertestapp;

import android.content.Context;
import android.telephony.NumberVerificationCallback;
import android.telephony.PhoneNumberRange;
import android.telephony.RadioAccessSpecifier;
import android.text.TextUtils;
import android.widget.Toast;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.stream.Collectors;

class ParameterParser {
    private static ParameterParser sInstance;

    static ParameterParser get(Context context) {
        if (sInstance == null) {
            sInstance = new ParameterParser(context);
        }
        return sInstance;
    }

    private final Context mContext;
    private final Map<Class, Function<String, Object>> mParsers =
            new HashMap<Class, Function<String, Object>>() {{
                put(PhoneNumberRange.class, ParameterParser::parsePhoneNumberRange);
                put(Executor.class, s -> parseExecutor(s));
                put(NumberVerificationCallback.class, s -> parseNumberVerificationCallback(s));
                put(Consumer.class, s -> parseConsumer(s));
                put(List.class, s -> parseList(s));
                put(RadioAccessSpecifier.class, s -> parseRadioAccessSpecifier(s));
            }};

    private ParameterParser(Context context) {
        mContext = context;
    }

    Object executeParser(Class type, String input) {
        if ("null".equals(input)) return null;
        return mParsers.getOrDefault(type, s -> null).apply(input);
    }

    private static PhoneNumberRange parsePhoneNumberRange(String input) {
        String[] parts = input.split(" ");
        if (parts.length != 4) {
            return null;
        }
        return new PhoneNumberRange(parts[0], parts[1], parts[2], parts[3]);
    }

    private Executor parseExecutor(String input) {
        return mContext.getMainExecutor();
    }

    private Consumer parseConsumer(String input) {
        return o -> Toast.makeText(mContext, "Consumer: " + String.valueOf(o), Toast.LENGTH_SHORT)
                .show();
    }

    /**
     * input format: (ran)/(band1)+(band2)+.../(chan1)+(chan2)+...
     * @return
     */
    private RadioAccessSpecifier parseRadioAccessSpecifier(String input) {
        String[] parts = input.split("/");
        int ran = Integer.parseInt(parts[0]);
        String[] bandStrings = parts[1].split("\\+");
        int[] bands = new int[bandStrings.length];
        String[] chanStrings = parts[2].split("\\+");
        int[] chans = new int[chanStrings.length];

        for (int i = 0; i < bands.length; i++) {
            bands[i] = Integer.parseInt(bandStrings[i]);
        }

        for (int i = 0; i < chans.length; i++) {
            chans[i] = Integer.parseInt(chanStrings[i]);
        }
        return new RadioAccessSpecifier(ran, bands, chans);
    }

    private List parseList(String input) {
        if (TextUtils.isEmpty(input)) {
            return Collections.emptyList();
        }
        String[] components = input.split(" ");
        String className = components[0];
        Class c;
        try {
            c = mContext.getClassLoader().loadClass(className);
        } catch (ClassNotFoundException e) {
            Toast.makeText(mContext, "Invalid class " + className,
                    Toast.LENGTH_SHORT).show();
            return null;
        }
        if (!mParsers.containsKey(c)) {
            Toast.makeText(mContext, "Cannot parse " + className,
                    Toast.LENGTH_SHORT).show();
            return null;
        }
        return Arrays.stream(components).skip(1)
                .map(mParsers.get(c))
                .collect(Collectors.toList());
    }

    private NumberVerificationCallback parseNumberVerificationCallback(String input) {
        return new NumberVerificationCallback() {
            @Override
            public void onCallReceived(String phoneNumber) {
                Toast.makeText(mContext, "Received verification " + phoneNumber,
                        Toast.LENGTH_SHORT).show();
            }

            @Override
            public void onVerificationFailed(int reason) {
                Toast.makeText(mContext, "Verification failed " + reason,
                        Toast.LENGTH_SHORT).show();
            }
        };
    }
}
