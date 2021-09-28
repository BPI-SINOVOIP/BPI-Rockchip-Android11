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
 * limitations under the License.
 */

package com.google.android.car.diagnostictools.utils;

/**
 * This class allows for custom formulas to translate input live/freeze frame data into the
 * appropriate values. Ideally a `conversion` object should be used in the JSON but this allows for
 * more flexibility if needed.
 */
class MathEval {

    /**
     * This is a sanity check for user generated strings to catch errors once instead of everytime
     * the data is translated
     *
     * @param str Translation string to test
     * @return True if the string doesn't or won't fail when processing simple inputs
     */
    static boolean testTranslation(final String str) {
        if (str == null || str.length() == 0) {
            return true;
        } else if (str.length() > 50) {
            return false;
        }

        try {
            eval(str, (float) 100.0);
            eval(str, (float) 10.0);
            eval(str, (float) 1);
            eval(str, (float) .1);
            eval(str, (float) 0);
            eval(str, (float) -100.0);
            eval(str, (float) -10.0);
            eval(str, (float) -1);
            eval(str, (float) -.1);
        } catch (Exception e) {
            return false;
        }
        return true;
    }

    /**
     * Uses a translation string and applies the formula to a variable From
     * https://stackoverflow.com/a/26227947 with modifications. String must only use +,-,*,/,^,(,)
     * or "sqrt", "sin", "cos", "tan" (as a function ie sqrt(4)) and the variable x
     *
     * @param translationString Translation string which uses x as the variable
     * @param variableIn Float that the translation is operating on
     * @return New Float that has gone through operations defined in "translationString". If
     *     translationString is non-operable then the variableIn will be returned
     * @throws TranslationTooLongException Thrown if the translation string is longer than 50 chars
     *     to prevent long execution times
     */
    static double eval(final String translationString, Float variableIn)
            throws TranslationTooLongException {

        if (translationString == null || translationString.length() == 0) {
            return variableIn;
        } else if (translationString.length() > 50) {
            throw new TranslationTooLongException(
                    "Translation function " + translationString + " is too long");
        }

        return new Object() {
            int mPos = -1, mCh;

            void nextChar() {
                mCh = (++mPos < translationString.length()) ? translationString.charAt(mPos) : -1;
            }

            boolean eat(int charToEat) {
                while (mCh == ' ') {
                    nextChar();
                }
                if (mCh == charToEat) {
                    nextChar();
                    return true;
                }
                return false;
            }

            double parse() {
                nextChar();
                double x = parseExpression();
                if (mPos < translationString.length()) {
                    throw new RuntimeException("Unexpected: " + (char) mCh);
                }
                return x;
            }

            // Grammar:
            // expression = term | expression `+` term | expression `-` term
            // term = factor | term `*` factor | term `/` factor
            // factor = `+` factor | `-` factor | `(` expression `)`
            //        | number | functionName factor | factor `^` factor

            double parseExpression() {
                double x = parseTerm();
                for (;; ) {
                    if (eat('+')) {
                        x += parseTerm(); // addition
                    } else if (eat('-')) {
                        x -= parseTerm(); // subtraction
                    } else {
                        return x;
                    }
                }
            }

            double parseTerm() {
                double x = parseFactor();
                for (;; ) {
                    if (eat('*')) {
                        x *= parseFactor(); // multiplication
                    } else if (eat('/')) {
                        x /= parseFactor(); // division
                    } else {
                        return x;
                    }
                }
            }

            double parseFactor() {
                if (eat('+')) {
                    return parseFactor(); // unary plus
                }
                if (eat('-')) {
                    return -parseFactor(); // unary minus
                }

                double x;
                int startPos = this.mPos;
                if (eat('(')) { // parentheses
                    x = parseExpression();
                    eat(')');
                } else if ((mCh >= '0' && mCh <= '9') || mCh == '.') { // numbers
                    while ((mCh >= '0' && mCh <= '9') || mCh == '.') {
                        nextChar();
                    }
                    x = Double.parseDouble(translationString.substring(startPos, this.mPos));
                } else if (mCh == 'x') {
                    x = variableIn;
                    nextChar();
                    // System.out.println(x);
                } else if (mCh >= 'a' && mCh <= 'z') { // functions
                    while (mCh >= 'a' && mCh <= 'z') {
                        nextChar();
                    }
                    String func = translationString.substring(startPos, this.mPos);
                    x = parseFactor();
                    switch (func) {
                        case "sqrt":
                            x = Math.sqrt(x);
                            break;
                        case "sin":
                            x = Math.sin(Math.toRadians(x));
                            break;
                        case "cos":
                            x = Math.cos(Math.toRadians(x));
                            break;
                        case "tan":
                            x = Math.tan(Math.toRadians(x));
                            break;
                        default:
                            throw new RuntimeException("Unknown function: " + func);
                    }
                } else {
                    throw new RuntimeException("Unexpected: " + (char) mCh);
                }

                if (eat('^')) {
                    x = Math.pow(x, parseFactor()); // exponentiation
                }

                return x;
            }
        }.parse();
    }

    /** Exception thrown if the translation string is too long */
    static class TranslationTooLongException extends Exception {

        TranslationTooLongException(String errorMsg) {
            super(errorMsg);
        }
    }
}
