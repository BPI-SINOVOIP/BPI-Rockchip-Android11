/*
 * Copyright (C) 2016 The Android Open Source Project
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
#pragma version(1)
#pragma rs java_package_name(android.view.cts.surfacevalidator)
#pragma rs reduce(countBlackishPixels) accumulator(countBlackishPixelsAccum) combiner(countBlackishPixelsCombiner)

uchar MIN_ALPHA;
uchar MAX_ALPHA;
uchar MIN_RED;
uchar MAX_RED;
uchar MIN_GREEN;
uchar MAX_GREEN;
uchar MIN_BLUE;
uchar MAX_BLUE;
int BOUNDS[4];

static void countBlackishPixelsAccum(int *accum, uchar4 pixel, uint32_t x, uint32_t y) {

    if (pixel.a <= MAX_ALPHA
            && pixel.a >= MIN_ALPHA
            && pixel.r <= MAX_RED
            && pixel.r >= MIN_RED
            && pixel.g <= MAX_GREEN
            && pixel.g >= MIN_GREEN
            && pixel.b <= MAX_BLUE
            && pixel.b >= MIN_BLUE
            && x >= BOUNDS[0]
            && x < BOUNDS[2]
            && y >= BOUNDS[1]
            && y < BOUNDS[3]) {
        *accum += 1;
    }
}

static void countBlackishPixelsCombiner(int *accum, const int *other){
    *accum += *other;
}
