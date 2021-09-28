/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.framebufferizer.utils;

public class DeviceInfo {
    final private int widthPx;
    final private int heightPx;
    final private double dp2px;
    final private double mm2px;
    final private double powerButtonTopMm;
    final private double powerButtonBottomMm;
    final private double volUpButtonTopMm;
    final private double volUpButtonBottomMm;

    DeviceInfo(int widthPx, int heightPx, double dp2px, double mm2px, double powerButtonTopMm,
            double powerButtonBottomMm, double volUpButtonTopMm, double volUpButtonBottomMm) {
        this.widthPx = widthPx;
        this.heightPx = heightPx;
        this.dp2px = dp2px;
        this.mm2px = mm2px;
        this.powerButtonTopMm = powerButtonTopMm;
        this.powerButtonBottomMm = powerButtonBottomMm;
        this.volUpButtonTopMm = volUpButtonTopMm;
        this.volUpButtonBottomMm = volUpButtonBottomMm;
    }

    public int getWidthPx() {
        return widthPx;
    }

    public int getHeightPx() {
        return heightPx;
    }

    public double getDp2px() {
        return dp2px;
    }

    public double getMm2px() {
        return mm2px;
    }

    public double getPowerButtonTopMm() {
        return powerButtonTopMm;
    }

    public double getPowerButtonBottomMm() {
        return powerButtonBottomMm;
    }

    public double getVolUpButtonTopMm() {
        return volUpButtonTopMm;
    }

    public double getVolUpButtonBottomMm() {
        return volUpButtonBottomMm;
    }
}
