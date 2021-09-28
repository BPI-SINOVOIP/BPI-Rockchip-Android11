/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.icu4j.srcgen.checker;

import com.google.common.collect.Lists;
import com.google.currysrc.api.RuleSet;
import com.google.currysrc.api.input.InputFileGenerator;
import com.google.currysrc.api.output.NullOutputSourceFileGenerator;
import com.google.currysrc.api.output.OutputSourceFileGenerator;
import com.google.currysrc.api.process.Context;
import com.google.currysrc.api.process.Processor;
import com.google.currysrc.api.process.Rule;
import com.google.currysrc.api.process.ast.BodyDeclarationLocators;
import com.google.currysrc.api.process.ast.TypeLocator;

import org.eclipse.jdt.core.dom.ASTNode;
import org.eclipse.jdt.core.dom.ASTVisitor;
import org.eclipse.jdt.core.dom.AbstractTypeDeclaration;
import org.eclipse.jdt.core.dom.AnnotationTypeDeclaration;
import org.eclipse.jdt.core.dom.AnnotationTypeMemberDeclaration;
import org.eclipse.jdt.core.dom.BodyDeclaration;
import org.eclipse.jdt.core.dom.CompilationUnit;
import org.eclipse.jdt.core.dom.EnumConstantDeclaration;
import org.eclipse.jdt.core.dom.EnumDeclaration;
import org.eclipse.jdt.core.dom.FieldDeclaration;
import org.eclipse.jdt.core.dom.Javadoc;
import org.eclipse.jdt.core.dom.MethodDeclaration;
import org.eclipse.jdt.core.dom.Modifier;
import org.eclipse.jdt.core.dom.TagElement;
import org.eclipse.jdt.core.dom.TypeDeclaration;

import java.io.File;
import java.util.Collections;
import java.util.List;

import static com.google.currysrc.api.process.Rules.createOptionalRule;

/**
 * Rules that operate over a set of files and record the public API (according to Android's rules
 * for @hide).
 */
public class RecordPublicApiRules implements RuleSet {

  private final InputFileGenerator inputFileGenerator;

  private final RecordPublicApi recordPublicApi;

  public RecordPublicApiRules(InputFileGenerator inputFileGenerator) {
    this.inputFileGenerator = inputFileGenerator;
    recordPublicApi = new RecordPublicApi();
  }

  @Override public InputFileGenerator getInputFileGenerator() {
    return inputFileGenerator;
  }

  @Override public List<Rule> getRuleList(File file) {
    return Collections.<Rule>singletonList(createOptionalRule(recordPublicApi));
  }

  @Override public OutputSourceFileGenerator getOutputSourceFileGenerator() {
    return NullOutputSourceFileGenerator.INSTANCE;
  }

  public List<String> publicMembers() {
    return recordPublicApi.publicMembers();
  }

  private static class RecordPublicApi implements Processor {
    private final List<String> publicMembers = Lists.newArrayList();

    @Override public void process(Context context, CompilationUnit cu) {
      cu.accept(new ASTVisitor() {
        @Override public boolean visit(AnnotationTypeDeclaration node) {
          throw new AssertionError("Not supported");
        }

        @Override public boolean visit(AnnotationTypeMemberDeclaration node) {
          throw new AssertionError("Not supported");
        }

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
      });
    }

    @Override
    public String toString() {
      return "RecordPublicApi{}";
    }

    private boolean handleTypeDeclarationNode(AbstractTypeDeclaration node) {
      handleDeclarationNode(node);
      // Continue processing for nested types / methods.
      return true;
    }

    private boolean handleMemberDeclarationNode(BodyDeclaration node) {
      handleDeclarationNode(node);
      // Leaf declaration (i.e. a method, fields, enum constant).
      return false;
    }

    private void handleDeclarationNode(BodyDeclaration node) {
      if (!isPublicApiEligible(node)) {
        return;
      }
      // The node is appropriately public and is not hidden.
      publicMembers.addAll(BodyDeclarationLocators.toLocatorStringForms(node));
    }

    public List<String> publicMembers() {
      return publicMembers;
    }
  }

  /**
   * Determine if a declaration can be public API by looking at its and ancestor type's modifier and
   * {@code @hide} javadoc tag.
   */
  public static boolean isPublicApiEligible(BodyDeclaration node) {
    if (isExplicitlyHidden(node)) {
      return false;
    }

    AbstractTypeDeclaration typeDeclaration = TypeLocator.findTypeDeclarationNode(node);
    if (typeDeclaration == null) {
      // Not unusual: methods / fields defined on anonymous types are like this. The parent
      // is a constructor expression, not a declaration.
      return false;
    }

    boolean isNonTypeDeclaration = typeDeclaration != node;
    if (isNonTypeDeclaration) {
      if (isExplicitlyHidden(node) || !isMemberPublicApiEligible(typeDeclaration, node)) {
        return false;
      }
    }
    while (typeDeclaration != null) {
      if (isExplicitlyHidden(typeDeclaration) ||
              !RecordPublicApiRules.isTypePublicApiEligible(typeDeclaration)) {
        return false;
      }
      typeDeclaration = TypeLocator.findEnclosingTypeDeclaration(typeDeclaration);
    }
    return true;
  }

  private static boolean isExplicitlyHidden(BodyDeclaration node) {
    // Don't visit AST or use ASTVisitor here as the caller is visiting the node.
    Javadoc javadoc = node.getJavadoc();
    if (javadoc == null) {
      return false;
    }
    for (TagElement tagElement : (List<TagElement>) javadoc.tags()) {
      String tagName = tagElement.getTagName();
      if (tagName == null) {
        continue;
      }
      if ("@hide".equals(tagName.toLowerCase())) {
        return true;
      }
    }
    return false;
  }

  private static boolean isTypePublicApiEligible(AbstractTypeDeclaration typeDeclaration) {
    int typeModifiers = typeDeclaration.getModifiers();
    return ((typeModifiers & Modifier.PUBLIC) != 0);
  }

  private static boolean isMemberPublicApiEligible(AbstractTypeDeclaration type, BodyDeclaration node) {
    if (node.getNodeType() == ASTNode.ENUM_DECLARATION
            || node.getNodeType() == ASTNode.TYPE_DECLARATION) {
      throw new AssertionError("Unsupported node type: " + node);
    }

    if (type instanceof TypeDeclaration) {
      TypeDeclaration typeDeclaration = (TypeDeclaration) type;
      // All methods are public on interfaces. Not sure if true on Java 8.
      if (typeDeclaration.isInterface()) {
        return true;
      }
    }
    // Enum constant doesn't have visiblity
    if (node.getNodeType() == ASTNode.ENUM_CONSTANT_DECLARATION) {
      return true;
    }
    int memberModifiers = node.getModifiers();
    return ((memberModifiers & (Modifier.PUBLIC | Modifier.PROTECTED)) != 0);
  }
}
