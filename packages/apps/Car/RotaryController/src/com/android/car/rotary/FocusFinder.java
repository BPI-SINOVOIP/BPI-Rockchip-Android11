/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.rotary;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.VisibleForTesting;

/**
 * The algorithm used for finding the next focusable view in a given direction from a view that
 * currently has focus. Most of the methods are copied from {@link android.view.FocusFinder}.
 */
class FocusFinder {

    /**
     * How much to bias the major axis over the minor axis in {@link #getWeightedDistanceFor}.
     * Warning: this fudge factor is finely tuned. Be sure to run all focus tests if you dare
     * tweak it.
     */
    private static final long MAJOR_AXIS_BIAS = 13;

    private static final int[] DIRECTIONS = new int[]{
            View.FOCUS_LEFT, View.FOCUS_RIGHT, View.FOCUS_UP, View.FOCUS_DOWN
    };

    /**
     * Returns whether part of {@code destRect} is in {@code direction} of part of {@code srcRect}.
     *
     * @param srcRect   the source rectangle
     * @param destRect  the destination rectangle
     * @param direction must be {@link View#FOCUS_UP}, {@link View#FOCUS_DOWN},
     *                  {@link View#FOCUS_LEFT}, or {@link View#FOCUS_RIGHT}
     */
    static boolean isPartiallyInDirection(Rect srcRect, Rect destRect, int direction) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return destRect.left < srcRect.right;
            case View.FOCUS_RIGHT:
                return destRect.right > srcRect.left;
            case View.FOCUS_UP:
                return destRect.top < srcRect.bottom;
            case View.FOCUS_DOWN:
                return destRect.bottom > srcRect.top;
        }
        throw new IllegalArgumentException("direction must be "
                + "FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, or FOCUS_RIGHT.");
    }

    /**
     * Returns whether part of {@code destRect} is in {@code direction} of {@code srcRect} and
     * {@code destRect} is not strictly in any of other 3 directions of {@code srcRect}.
     *
     * @param srcRect   the source rectangle
     * @param destRect  the destination rectangle
     * @param direction must be {@link View#FOCUS_UP}, {@link View#FOCUS_DOWN},
     *                  {@link View#FOCUS_LEFT}, or {@link View#FOCUS_RIGHT}
     */
    static boolean isInDirection(Rect srcRect, Rect destRect, int direction) {
        // If destRect is strictly in the given direction of srcRect, destRect is in the given
        // direction of srcRect.
        if (isStrictlyInDirection(srcRect, destRect, direction)) {
            return true;
        }

        // If destRect is strictly in any of the other directions of srcRect, destRect is not in
        // the given direction of srcRect.
        for (int i = 0; i < DIRECTIONS.length; i++) {
            if (direction != DIRECTIONS[i]
                    && isStrictlyInDirection(srcRect, destRect, DIRECTIONS[i])) {
                return false;
            }
        }

        // Otherwise check whether part of destRect is in the given direction of srcRect.
        switch (direction) {
            case View.FOCUS_LEFT:
                return destRect.left < srcRect.left;
            case View.FOCUS_RIGHT:
                return destRect.right > srcRect.right;
            case View.FOCUS_UP:
                return destRect.top < srcRect.top;
            case View.FOCUS_DOWN:
                return destRect.bottom > srcRect.bottom;
        }
        throw new IllegalArgumentException("direction must be "
                + "FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, or FOCUS_RIGHT.");
    }

    /**
     * Returns whether {@code destRect} is a candidate for the next focus given the {@code
     * direction}.
     *
     * For example, iff {@code destRect} is a candidate for {@link View#FOCUS_LEFT}, the following
     * conditions must be true:
     * <ul>
     *  <li> {@code destRect.left} is on the left of {@code srcRect.left}
     *  <li> and one of the following conditions must be true:
     *  <ul>
     *   <li> {@code destRect.right} is on the left of {@code srcRect.right}
     *   <li> {@code destRect.right} equals or is on the left of {@code srcRect.left} (an edge case
     *        for an empty {@code srcRect}, which is used in some cases when searching from a point
     *        on the screen)
     *  </ul>
     * </ul>
     *
     * @param srcRect   the source rectangle we are searching from
     * @param destRect  the candidate rectangle
     * @param direction must be {@link View#FOCUS_UP},{@link View#FOCUS_DOWN},
     *                  {@link View#FOCUS_LEFT},or {@link View#FOCUS_RIGHT}
     */
    static boolean isCandidate(Rect srcRect, Rect destRect, int direction) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return (srcRect.right > destRect.right || srcRect.left >= destRect.right)
                        && srcRect.left > destRect.left;
            case View.FOCUS_RIGHT:
                return (srcRect.left < destRect.left || srcRect.right <= destRect.left)
                        && srcRect.right < destRect.right;
            case View.FOCUS_UP:
                return (srcRect.bottom > destRect.bottom || srcRect.top >= destRect.bottom)
                        && srcRect.top > destRect.top;
            case View.FOCUS_DOWN:
                return (srcRect.top < destRect.top || srcRect.bottom <= destRect.top)
                        && srcRect.bottom < destRect.bottom;
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns whether {@code rect1} is a better candidate than {@code rect2} for a focus search in
     * a particular {@code direction} from a {@code source} rect.  This is the core routine that
     * determines the order of focus searching.
     * <p>
     * Note: this method doesn't check whether {@code rect1} and {@code rect2} are candidates in the
     * first place, because the strategy to determine a candidate varies: geometry is used for
     * focusable views, while view hierarchy and geometry are used for focus areas. The caller is
     * responsible for using a proper strategy to exclude the non-candidates before calling this
     * method.
     *
     * @param direction must be {@link View#FOCUS_UP},{@link View#FOCUS_DOWN},
     *                  {@link View#FOCUS_LEFT},or {@link View#FOCUS_RIGHT}
     * @param source    the source rectangle we are searching from
     * @param rect1     the candidate rectangle
     * @param rect2     the current best candidate
     */
    static boolean isBetterCandidate(int direction, Rect source, Rect rect1, Rect rect2) {
        // If rect1 is better by beam, it wins.
        if (beamBeats(direction, source, rect1, rect2)) {
            return true;
        }

        // If rect2 is better by beam, then rect1 can't be.
        if (beamBeats(direction, source, rect2, rect1)) {
            return false;
        }

        // Otherwise, do fudge-tastic comparison of the major and minor axis.
        return getWeightedDistanceFor(
                majorAxisDistance(direction, source, rect1),
                minorAxisDistance(direction, source, rect1))
                < getWeightedDistanceFor(
                majorAxisDistance(direction, source, rect2),
                minorAxisDistance(direction, source, rect2));
    }

    private static long getWeightedDistanceFor(long majorAxisDistance, long minorAxisDistance) {
        return MAJOR_AXIS_BIAS * majorAxisDistance * majorAxisDistance
                + minorAxisDistance * minorAxisDistance;
    }

    /**
     * Finds the distance on the minor axis (w.r.t the direction to the nearest edge of the
     * destination rectangle).
     */
    private static int minorAxisDistance(int direction, Rect source, Rect dest) {
        switch (direction) {
            case View.FOCUS_LEFT:
            case View.FOCUS_RIGHT:
                // The distance between the center verticals.
                return Math.abs(
                        ((source.top + source.height() / 2) - ((dest.top + dest.height() / 2))));
            case View.FOCUS_UP:
            case View.FOCUS_DOWN:
                // The distance between the center horizontals.
                return Math.abs(
                        ((source.left + source.width() / 2) - ((dest.left + dest.width() / 2))));
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns whether {@code rect1} is a better candidate than {@code rect2} by virtue of it being
     * in {@code source}'s beam.
     */
    @VisibleForTesting
    static boolean beamBeats(int direction, Rect source, Rect rect1, Rect rect2) {
        final boolean rect1InSrcBeam = beamsOverlap(direction, source, rect1);
        final boolean rect2InSrcBeam = beamsOverlap(direction, source, rect2);

        // If rect1 isn't exclusively in the src beam, it doesn't win.
        if (rect2InSrcBeam || !rect1InSrcBeam) {
            return false;
        }

        // We know rect1 is in the beam, and rect2 is not. If rect1 is to the direction of, and
        // rect2 is not, rect1 wins. For example, for direction left, if rect1 is to the left of
        // the source and rect2 is below, then we always prefer the in beam rect1, since rect2
        // could be reached by going down.
        if (!isToDirectionOf(direction, source, rect2)) {
            return true;
        }

        // For horizontal directions, being exclusively in beam always wins.
        if ((direction == View.FOCUS_LEFT || direction == View.FOCUS_RIGHT)) {
            return true;
        }

        // For vertical directions, beams only beat up to a point: as long as rect2 isn't
        // completely closer, rect1 wins. E.g., for direction down, completely closer means for
        // rect2's top edge to be closer to the source's top edge than rect1's bottom edge.
        return majorAxisDistance(direction, source, rect1)
                < majorAxisDistanceToFarEdge(direction, source, rect2);
    }

    /**
     * Returns whether the "beams" (w.r.t the given {@code direction}'s axis of {@code rect1} and
     * {@code rect2}) overlap.
     */
    @VisibleForTesting
    static boolean beamsOverlap(int direction, Rect rect1, Rect rect2) {
        switch (direction) {
            case View.FOCUS_LEFT:
            case View.FOCUS_RIGHT:
                return (rect2.bottom > rect1.top) && (rect2.top < rect1.bottom);
            case View.FOCUS_UP:
            case View.FOCUS_DOWN:
                return (rect2.right > rect1.left) && (rect2.left < rect1.right);
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns whether {@code dest} is to the {@code direction} of {@code src}.
     */
    private static boolean isToDirectionOf(int direction, Rect src, Rect dest) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return src.left >= dest.right;
            case View.FOCUS_RIGHT:
                return src.right <= dest.left;
            case View.FOCUS_UP:
                return src.top >= dest.bottom;
            case View.FOCUS_DOWN:
                return src.bottom <= dest.top;
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns the distance from the edge furthest in the given {@code direction} of {@code source}
     * to the edge nearest in the given {@code direction} of {@code dest}.  If the {@code dest} is
     * not in the {@code direction} from {@code source}, returns 0.
     */
    @VisibleForTesting
    static int majorAxisDistance(int direction, Rect source, Rect dest) {
        return Math.max(0, majorAxisDistanceRaw(direction, source, dest));
    }

    private static int majorAxisDistanceRaw(int direction, Rect source, Rect dest) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return source.left - dest.right;
            case View.FOCUS_RIGHT:
                return dest.left - source.right;
            case View.FOCUS_UP:
                return source.top - dest.bottom;
            case View.FOCUS_DOWN:
                return dest.top - source.bottom;
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns the distance along the major axis (w.r.t the {@code direction} from the edge of
     * {@code source} to the far edge of {@code dest}). If the {@code dest} is not in the {@code
     * direction} from {@code source}, returns 1 (to break ties with {@link #majorAxisDistance}).
     */
    @VisibleForTesting
    static int majorAxisDistanceToFarEdge(int direction, Rect source, Rect dest) {
        return Math.max(1, majorAxisDistanceToFarEdgeRaw(direction, source, dest));
    }

    private static int majorAxisDistanceToFarEdgeRaw(int direction, Rect source, Rect dest) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return source.left - dest.left;
            case View.FOCUS_RIGHT:
                return dest.right - source.right;
            case View.FOCUS_UP:
                return source.top - dest.top;
            case View.FOCUS_DOWN:
                return dest.bottom - source.bottom;
        }
        throw new IllegalArgumentException("direction must be one of "
                + "{FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, FOCUS_RIGHT}.");
    }

    /**
     * Returns whether {@code destRect} is strictly in {@code direction} of {@code srcRect}.
     * <p>
     * For example, iff {@code destRect} is strictly to the {@link View#FOCUS_LEFT} of {@code
     * srcRect}, the following conditions must be true:
     * <ul>
     *   <li> {@code destRect.left} is on the left of {@code srcRect.left}
     *   <li> {@code destRect.right} overlaps with or is on the left of {@code srcRect.right}
     *   <li> [{@code destRect.top}, {@code destRect.bottom}] contains or is contained by
     *        [{@code srcRect.top}, {@code srcRect.bottom}]
     * </ul>
     *
     * @param srcRect   the source rectangle
     * @param destRect  the destination rectangle
     * @param direction must be {@link View#FOCUS_UP}, {@link View#FOCUS_DOWN},
     *                  {@link View#FOCUS_LEFT}, or {@link View#FOCUS_RIGHT}
     */
    private static boolean isStrictlyInDirection(Rect srcRect, Rect destRect, int direction) {
        switch (direction) {
            case View.FOCUS_LEFT:
                return destRect.left < srcRect.left && destRect.right <= srcRect.right
                        && containsOrIsContainedVertically(srcRect, destRect);
            case View.FOCUS_RIGHT:
                return destRect.right > srcRect.right && destRect.left >= srcRect.left
                        && containsOrIsContainedVertically(srcRect, destRect);
            case View.FOCUS_UP:
                return destRect.top < srcRect.top && destRect.bottom <= srcRect.bottom
                        && containsOrIsContainedHorizontally(srcRect, destRect);
            case View.FOCUS_DOWN:
                return destRect.bottom > srcRect.bottom && destRect.top >= srcRect.top
                        && containsOrIsContainedHorizontally(srcRect, destRect);
        }
        throw new IllegalArgumentException("direction must be "
                + "FOCUS_UP, FOCUS_DOWN, FOCUS_LEFT, or FOCUS_RIGHT.");
    }

    /**
     * Returns true if the projection of {@code rect1} on the Y-axis contains or is contained by the
     * projection of {@code rect2} on the Y-axis.
     */
    private static boolean containsOrIsContainedVertically(Rect rect1, Rect rect2) {
        return (rect1.top <= rect2.top && rect1.bottom >= rect2.bottom)
                || (rect2.top <= rect1.top && rect2.bottom >= rect1.bottom);
    }

    /**
     * Returns true if the projection of {@code rect1} on the X-axis contains or is contained by the
     * projection of {@code rect2} on the X-axis.
     */
    private static boolean containsOrIsContainedHorizontally(Rect rect1, Rect rect2) {
        return (rect1.left <= rect2.left && rect1.right >= rect2.right)
                || (rect2.left <= rect1.left && rect2.right >= rect1.right);
    }
}
