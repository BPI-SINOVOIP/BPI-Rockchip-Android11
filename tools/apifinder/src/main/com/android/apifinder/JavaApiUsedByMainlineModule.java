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

package com.android.apifinder;

import static com.google.errorprone.BugPattern.SeverityLevel.SUGGESTION;

import com.google.auto.service.AutoService;
import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.MethodInvocationTreeMatcher;
import com.google.errorprone.bugpatterns.BugChecker.NewClassTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.source.tree.NewClassTree;
import com.sun.tools.javac.code.Symbol;
import com.sun.tools.javac.code.Symbol.MethodSymbol;
import com.sun.tools.javac.code.Symbol.VarSymbol;
import java.util.ArrayList;
import java.util.List;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.Modifier;

/** Bug checker to detect method or field used by a mainline module */
@AutoService(BugChecker.class)
@BugPattern(
    name = "JavaApiUsedByMainlineModule",
    summary = "Any public method used by a mainline module.",
    severity = SUGGESTION)
public final class JavaApiUsedByMainlineModule extends BugChecker
    implements MethodInvocationTreeMatcher, NewClassTreeMatcher {

    /*
     * Checks if a method or class is private.
     * A method is considered as private method when any of the following condition met.
     *    1. Method is defined as private.
     *    2. Method's class is defined as private.
     *    3. Method's ancestor classes is defined as private.
     */
    private boolean isPrivate(Symbol sym) {
        Symbol tmpSym = sym;
        while (tmpSym != null) {
            if (!tmpSym.getModifiers().contains(Modifier.PUBLIC)) {
                return true;
            }
            tmpSym = ASTHelpers.enclosingClass(tmpSym);
        }
        return false;
    }

    /*
     * Constructs parameters. Only return parameter type.
     * For example
     *     (int, boolean, java.lang.String)
     */
    private String constructParameters(Symbol sym) {
        List<VarSymbol> paramsList = ((MethodSymbol) sym).getParameters();
        List<StringBuilder> stringParamsList = new ArrayList();
        for (VarSymbol paramSymbol : paramsList) {
            StringBuilder tmpParam = new StringBuilder(paramSymbol.asType().toString());

            // Removes "<*>" in parameter type.
            if (tmpParam.indexOf("<") != -1) {
                tmpParam = tmpParam.replace(
                    tmpParam.indexOf("<"), tmpParam.lastIndexOf(">") + 1, "");
            }

            stringParamsList.add(tmpParam);
        }
        return "(" + String.join(", ", stringParamsList) + ")";
    }

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState state) {
        Symbol sym = ASTHelpers.getSymbol(tree);

        // Exclude private function.
        if (isPrivate(sym)) {
            return Description.NO_MATCH;
        }

        // Constructs method name formatted as superClassName.className.methodName,
        // using supperClassName.className.className for constructor
        String methodName = sym.name.toString();
        List<String> nameList = new ArrayList();
        if (sym.getKind() == ElementKind.CONSTRUCTOR) {
            Symbol classSymbol = ASTHelpers.enclosingClass(sym);
            while (classSymbol != null) {
                nameList.add(0, classSymbol.name.toString());
                classSymbol = ASTHelpers.enclosingClass(classSymbol);
            }
            methodName = String.join(".", nameList);
        }

        String params = constructParameters(sym);

        return buildDescription(tree)
            .setMessage(
                String.format("%s.%s%s", ASTHelpers.enclosingClass(sym), methodName, params))
            .build();
    }

    @Override
    public Description matchNewClass(NewClassTree tree, VisitorState state) {
        Symbol sym = ASTHelpers.getSymbol(tree);

        // Excludes private class.
        if (isPrivate(sym)) {
            return Description.NO_MATCH;
        }

        String params = constructParameters(sym);

        // Constructs constructor name.
        Symbol tmpSymbol = ASTHelpers.enclosingClass(sym);
        List<String> nameList = new ArrayList();
        while (tmpSymbol != null) {
            nameList.add(0, tmpSymbol.name.toString());
            tmpSymbol = ASTHelpers.enclosingClass(tmpSymbol);
        }
        String constructorName = String.join(".", nameList);

        return buildDescription(tree)
            .setMessage(
                String.format(
                    "%s.%s%s", ASTHelpers.enclosingClass(sym), constructorName, params))
            .build();
    }
}

