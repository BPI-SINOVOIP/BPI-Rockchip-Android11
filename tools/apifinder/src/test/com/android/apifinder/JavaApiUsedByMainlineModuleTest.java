package com.android.apifinder;

import com.android.apifinder.JavaApiUsedByMainlineModule;
import com.google.errorprone.CompilationTestHelper;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link JavaApiUsedByMainlineModule}. */
@RunWith(JUnit4.class)
public final class JavaApiUsedByMainlineModuleTest {

  private CompilationTestHelper compilationHelper;

  @Before
  public void setUp() {
    compilationHelper = CompilationTestHelper.newInstance(
        JavaApiUsedByMainlineModule.class, getClass());
  }

  /*
   * The error prone testing library will run the plugin on the resource file.
   * The error prone testing library will compare the comment of each method in the
   * resource file to determine if the return value is expected.
   */
  @Test
  public void positiveFindPublicMethod() {
    compilationHelper
        .addSourceFile("JavaApiUsedByMainlineModuleCases.java").doTest();
  }
}
