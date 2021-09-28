/*
 * Copyright (C) 2016 The Dagger Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package dagger.internal.codegen;

import static com.google.testing.compile.CompilationSubject.assertThat;
import static dagger.internal.codegen.Compilers.daggerCompiler;
import static dagger.internal.codegen.DaggerModuleMethodSubject.Factory.assertThatMethodInUnannotatedClass;
import static dagger.internal.codegen.DaggerModuleMethodSubject.Factory.assertThatModuleMethod;

import com.google.common.collect.ImmutableList;
import com.google.testing.compile.Compilation;
import com.google.testing.compile.JavaFileObjects;
import dagger.Module;
import dagger.producers.ProducerModule;
import java.lang.annotation.Annotation;
import java.util.Collection;
import javax.inject.Inject;
import javax.inject.Qualifier;
import javax.inject.Singleton;
import javax.tools.JavaFileObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

/** Tests {@link BindsOptionalOfMethodValidator}. */
@RunWith(Parameterized.class)
public class BindsOptionalOfMethodValidationTest {
  @Parameters(name = "{0}")
  public static Collection<Object[]> data() {
    return ImmutableList.copyOf(new Object[][] {{Module.class}, {ProducerModule.class}});
  }

  private final String moduleDeclaration;

  public BindsOptionalOfMethodValidationTest(Class<? extends Annotation> moduleAnnotation) {
    moduleDeclaration = "@" + moduleAnnotation.getCanonicalName() + " abstract class %s { %s }";
  }

  @Test
  public void nonAbstract() {
    assertThatMethod("@BindsOptionalOf Object concrete() { return null; }")
        .hasError("must be abstract");
  }

  @Test
  public void hasParameters() {
    assertThatMethod("@BindsOptionalOf abstract Object hasParameters(String s1);")
        .hasError("parameters");
  }

  @Test
  public void typeParameters() {
    assertThatMethod("@BindsOptionalOf abstract <S> S generic();").hasError("type parameters");
  }

  @Test
  public void notInModule() {
    assertThatMethodInUnannotatedClass("@BindsOptionalOf abstract Object notInModule();")
        .hasError("within a @Module or @ProducerModule");
  }

  @Test
  public void throwsException() {
    assertThatMethod("@BindsOptionalOf abstract Object throwsException() throws RuntimeException;")
        .hasError("may not throw");
  }

  @Test
  public void returnsVoid() {
    assertThatMethod("@BindsOptionalOf abstract void returnsVoid();").hasError("void");
  }

  @Test
  public void returnsMembersInjector() {
    assertThatMethod("@BindsOptionalOf abstract MembersInjector<Object> returnsMembersInjector();")
        .hasError("framework");
  }

  @Test
  public void tooManyQualifiers() {
    assertThatMethod(
            "@BindsOptionalOf @Qualifier1 @Qualifier2 abstract String tooManyQualifiers();")
        .importing(Qualifier1.class, Qualifier2.class)
        .hasError("more than one @Qualifier");
  }

  @Test
  public void intoSet() {
    assertThatMethod("@BindsOptionalOf @IntoSet abstract String intoSet();")
        .hasError("cannot have multibinding annotations");
  }

  @Test
  public void elementsIntoSet() {
    assertThatMethod("@BindsOptionalOf @ElementsIntoSet abstract Set<String> elementsIntoSet();")
        .hasError("cannot have multibinding annotations");
  }

  @Test
  public void intoMap() {
    assertThatMethod("@BindsOptionalOf @IntoMap abstract String intoMap();")
        .hasError("cannot have multibinding annotations");
  }

  /**
   * Tests that @BindsOptionalOf @IntoMap actually causes module validation to fail.
   *
   * @see <a href="http://b/118434447">bug 118434447</a>
   */
  @Test
  public void intoMapWithComponent() {
    JavaFileObject module =
        JavaFileObjects.forSourceLines(
            "test.TestModule",
            "package test;",
            "",
            "import dagger.BindsOptionalOf;",
            "import dagger.Module;",
            "import dagger.multibindings.IntoMap;",
            "",
            "@Module",
            "interface TestModule {",
            "  @BindsOptionalOf @IntoMap Object object();",
            "}");
    JavaFileObject component =
        JavaFileObjects.forSourceLines(
            "test.TestComponent",
            "package test;",
            "",
            "import dagger.Component;",
            "",
            "@Component(modules = TestModule.class)",
            "interface TestComponent {}");

    Compilation compilation = daggerCompiler().compile(module, component);
    assertThat(compilation).failed();
    assertThat(compilation)
        .hadErrorContaining("cannot have multibinding annotations")
        .inFile(module)
        .onLineContaining("object();");
  }

  /** An injectable value object. */
  public static final class Thing {
    @Inject
    Thing() {}
  }

  @Test
  public void implicitlyProvidedType() {
    assertThatMethod("@BindsOptionalOf abstract Thing thing();")
        .importing(Thing.class)
        .hasError("return unqualified types that have an @Inject-annotated constructor");
  }

  @Test
  public void hasScope() {
    assertThatMethod("@BindsOptionalOf @Singleton abstract String scoped();")
        .importing(Singleton.class)
        .hasError("cannot be scoped");
  }

  private DaggerModuleMethodSubject assertThatMethod(String method) {
    return assertThatModuleMethod(method).withDeclaration(moduleDeclaration);
  }

  /** A qualifier. */
  @Qualifier
  public @interface Qualifier1 {}

  /** A qualifier. */
  @Qualifier
  public @interface Qualifier2 {}
}
