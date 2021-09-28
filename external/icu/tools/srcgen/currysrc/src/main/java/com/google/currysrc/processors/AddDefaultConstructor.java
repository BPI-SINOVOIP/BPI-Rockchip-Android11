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
package com.google.currysrc.processors;

import com.google.currysrc.api.process.Context;
import com.google.currysrc.api.process.Processor;
import com.google.currysrc.api.process.ast.TypeLocator;
import java.util.List;
import org.eclipse.jdt.core.dom.AST;
import org.eclipse.jdt.core.dom.AbstractTypeDeclaration;
import org.eclipse.jdt.core.dom.CompilationUnit;
import org.eclipse.jdt.core.dom.FieldDeclaration;
import org.eclipse.jdt.core.dom.MethodDeclaration;
import org.eclipse.jdt.core.dom.Modifier;
import org.eclipse.jdt.core.dom.Modifier.ModifierKeyword;
import org.eclipse.jdt.core.dom.TypeDeclaration;
import org.eclipse.jdt.core.dom.rewrite.ASTRewrite;
import org.eclipse.jdt.core.dom.rewrite.ListRewrite;

/**
 * Add a default constructor to a white list of classes.
 */
public class AddDefaultConstructor implements Processor {

  /**
   * The visibility modifier keywords.
   */
  private static final ModifierKeyword[] VISIBILITY_MODIFIERS = new ModifierKeyword[]{
      ModifierKeyword.PUBLIC_KEYWORD,
      ModifierKeyword.PRIVATE_KEYWORD, ModifierKeyword.PROTECTED_KEYWORD};

  private final List<TypeLocator> whitelist;
  private Listener listener;

  public interface Listener {

    /**
     * Called when a default constructor is added to a class.
     *
     * @param locator the locator for the modified class.
     * @param typeDeclaration the class that was modified.
     */
    void onAddDefaultConstructor(TypeLocator locator, TypeDeclaration typeDeclaration);
  }

  public AddDefaultConstructor(List<TypeLocator> whitelist) {
    this.whitelist = whitelist;
    this.listener = (l, typeDeclaration) -> {};
  }

  public void setListener(Listener listener) {
    this.listener = listener;
  }

  @Override
  public void process(Context context, CompilationUnit cu) {
    final ASTRewrite rewrite = context.rewrite();
    for (TypeLocator typeLocator : whitelist) {
      AbstractTypeDeclaration abstractTypeDeclaration = typeLocator.find(cu);
      if (abstractTypeDeclaration instanceof TypeDeclaration) {
        TypeDeclaration typeDeclaration = (TypeDeclaration) abstractTypeDeclaration;
        addDefaultConstructor(rewrite, typeDeclaration);
        listener.onAddDefaultConstructor(typeLocator, typeDeclaration);
      }
    }
  }

  private void addDefaultConstructor(ASTRewrite rewrite, TypeDeclaration node) {
    AST ast = rewrite.getAST();
    MethodDeclaration method = ast.newMethodDeclaration();
    method.setConstructor(true);
    // Copy the class name as a constructor must have the same name as the class.
    method.setName(ast.newSimpleName(node.getName().getIdentifier()));

    // The default constructor has the same visibility as its containing class.
    int modifiers = node.getModifiers();
    @SuppressWarnings("unchecked") List<Modifier> methodModifiers = method.modifiers();
    for (ModifierKeyword keyword : VISIBILITY_MODIFIERS) {
      if ((modifiers & keyword.toFlagValue()) != 0) {
        methodModifiers.add(ast.newModifier(keyword));
      }
    }

    // Create an empty body.
    method.setBody(ast.newBlock());

    ListRewrite listRewrite = rewrite.getListRewrite(node, node.getBodyDeclarationsProperty());

    MethodDeclaration[] methods = node.getMethods();
    if (methods.length > 0) {
      // Insert before the first method.
      listRewrite.insertBefore(method, methods[0], null);
    } else {
      FieldDeclaration[] fields = node.getFields();
      if (fields.length > 0) {
        // Insert after the last field.
        listRewrite.insertAfter(method, fields[fields.length - 1], null);
      } else {
        // Insert at the beginning of the class.
        listRewrite.insertFirst(method, null);
      }
    }
  }
}
