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
package com.android.icu4j.srcgen;

import com.android.icu4j.srcgen.checker.RecordPublicApiRules;
import com.google.common.collect.Lists;
import com.google.currysrc.api.process.Context;
import com.google.currysrc.api.process.JavadocUtils;
import com.google.currysrc.api.process.Processor;
import com.google.currysrc.api.process.ast.BodyDeclarationLocator;
import com.google.currysrc.api.process.ast.BodyDeclarationLocators;
import com.google.currysrc.api.process.ast.StartPositionComparator;

import org.eclipse.jdt.core.dom.ASTVisitor;
import org.eclipse.jdt.core.dom.AbstractTypeDeclaration;
import org.eclipse.jdt.core.dom.BodyDeclaration;
import org.eclipse.jdt.core.dom.CompilationUnit;
import org.eclipse.jdt.core.dom.EnumConstantDeclaration;
import org.eclipse.jdt.core.dom.EnumDeclaration;
import org.eclipse.jdt.core.dom.FieldDeclaration;
import org.eclipse.jdt.core.dom.MethodDeclaration;
import org.eclipse.jdt.core.dom.TypeDeclaration;
import org.eclipse.jdt.core.dom.rewrite.ASTRewrite;

import java.util.Collections;
import java.util.List;

/**
 * Adds a @hide javadoc tag to {@link BodyDeclaration}s that are not whitelisted.
 */
public class HideNonWhitelistedDeclarations implements Processor {
  private final List<BodyDeclarationLocator> whitelist;
  private final String tagComment;

  public HideNonWhitelistedDeclarations(List<BodyDeclarationLocator> whitelist, String tagComment) {
    this.whitelist = whitelist;
    this.tagComment = tagComment;
  }

  @Override
  public void process(Context context, CompilationUnit cu) {
    // Ignore this process if the whitelist is not provided
    if (whitelist == null) {
      return;
    }
    List<BodyDeclaration> matchingNodes = Lists.newArrayList();
    cu.accept(new ASTVisitor() {
      @Override public boolean visit(EnumConstantDeclaration node) {
        return handleMemberDeclarationNode(node);
      }

      @Override public boolean visit(EnumDeclaration node) {
        return handleTypeDeclarationNode(node);
      }

      @Override public boolean visit(FieldDeclaration node) {
        return handleMemberDeclarationNode(node);
      }

      @Override public boolean visit(MethodDeclaration node) {
        return handleMemberDeclarationNode(node);
      }

      @Override public boolean visit(TypeDeclaration node) {
        return handleTypeDeclarationNode(node);
      }

      private boolean handleTypeDeclarationNode(AbstractTypeDeclaration node) {
        matchIfNotWhitelistedAndNotHidden(node);
        // Continue processing for nested types / methods.
        return true;
      }

      private boolean handleMemberDeclarationNode(BodyDeclaration node) {
        matchIfNotWhitelistedAndNotHidden(node);
        // Leaf declaration (i.e. a method, fields, enum constant).
        return false;
      }

      private void matchIfNotWhitelistedAndNotHidden(final BodyDeclaration node) {
        if (node == null) {
          return;
        }

        if (!RecordPublicApiRules.isPublicApiEligible(node)) {
          return;
        }

        if (BodyDeclarationLocators.matchesAny(whitelist, node)) {
          return;
        }
        matchingNodes.add(node);
      }
    });

    // Tackle nodes in reverse order to avoid messing up the ASTNode offsets.
    Collections.sort(matchingNodes, new StartPositionComparator());
    ASTRewrite rewrite = context.rewrite();
    for (BodyDeclaration bodyDeclaration : Lists.reverse(matchingNodes)) {
      JavadocUtils.addJavadocTag(rewrite, bodyDeclaration, tagComment);
    }
  }

  @Override public String toString() {
    return "HideNonWhitelistedDeclarations{" +
        "whitelist=" + whitelist +
        "tagComment=" + tagComment +
        '}';
  }
}
