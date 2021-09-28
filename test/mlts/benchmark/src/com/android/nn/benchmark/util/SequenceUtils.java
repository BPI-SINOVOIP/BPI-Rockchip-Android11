package com.android.nn.benchmark.util;

/** Utilities for operations on sequences. */
public final class SequenceUtils {
    private SequenceUtils() {}

    /**
     * Calculates Levenshtein distance between 2 sequences.
     *
     * This is the minimum number of single-element edits (insertions, deletions or substitutions)
     * required to change one sequence to another.
     * See: https://en.wikipedia.org/wiki/Levenshtein_distance
     */
    public static int calculateEditDistance(int[] seqA, int[] seqB) {
        int m = seqA.length;
        int n = seqB.length;
        int[][] d = new int[m + 1][n + 1];

        for (int i = 0; i <= m; ++i) {
            d[i][0] = i;
        }

        for (int j = 1; j <= n; ++j) {
            d[0][j] = j;
        }

        for (int i = 1; i <= m; ++i) {
            for (int j = 1; j <= n; ++j) {
                int substitutionCost = (seqA[i - 1] == seqB[j - 1]) ? 0 : 1;
                d[i][j] = Math.min(
                        Math.min(d[i - 1][j] + 1, d[i][j - 1] + 1),
                        d[i - 1][j - 1] + substitutionCost);
            }
        }
        return d[m][n];
    }

    public static int indexOfLargest(float[] items) {
        int ret = -1;
        float largest = -Float.MAX_VALUE;
        for (int i = 0; i < items.length; ++i) {
            if (items[i] > largest) {
                ret = i;
                largest = items[i];
            }
        }
        return ret;
    }
}
