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

package com.android.cts.releaseparser;

import java.util.HashMap;
import java.util.Map;
import java.util.ArrayList;
import java.util.List;

public class ArgumentParser {
    private HashMap<String, List<String>> mArgumentMap;
    private String[] mArguments;

    public ArgumentParser(String args[]) {
        mArguments = args;
        mArgumentMap = null;
    }

    public Map<String, List<String>> getArgumentMap() {
        if (mArgumentMap == null) {
            parse();
        }
        return mArgumentMap;
    }

    /**
     * Gets parameter list of an option
     *
     * @return List<String> of parameters or null if no such option
     */
    public List<String> getParameterList(String option) {
        return getArgumentMap().get(option);
    }

    /**
     * Gets parameter element by index of an option
     *
     * @return String of the parameter or null
     */
    public String getParameterElement(String option, int index) {
        String ele = null;
        try {
            ele = getArgumentMap().get(option).get(index);
        } catch (Exception ex) {
        }
        return ele;
    }

    /**
     * Checks if arguments contin an option
     *
     * @return true or false
     */
    public boolean containsOption(String option) {
        return getArgumentMap().containsKey(option);
    }

    /**
     * Parses arguments for a map of <options, parameterList> format: -[option1] <parameter1.1>
     * <parameter1.2>...
     */
    private void parse() {
        mArgumentMap = new HashMap<String, List<String>>();

        for (int i = 0; i < mArguments.length; i++) {
            // an option starts with "-"
            if (mArguments[i].startsWith("-")) {
                String option = mArguments[i].substring(1);

                // gets parameters till the next option, starts with -
                ArrayList parameterList = new ArrayList<String>();
                while ((i + 1 < mArguments.length) && (!mArguments[i + 1].startsWith("-"))) {
                    ++i;
                    parameterList.add(mArguments[i]);
                }
                mArgumentMap.put(option, parameterList);
            }
        }
    }
}
