package com.android.apifinder;

public class JavaApiUsedByMainlineModuleCases {

  public class PublicSubClass {
    public void publicMethod() {}

    private void privateMethod() {}
  }

  private class PrivateSubClass {
    public void publicMethod() {}
  }

  public void testMethod() {
    // BUG: Diagnostic contains: JavaApiUsedByMainlineModuleCases.PublicSubClass
    // .JavaApiUsedByMainlineModuleCases.PublicSubClass()
    PublicSubClass publicTestClass = new PublicSubClass();

    // BUG: Diagnostic contains: JavaApiUsedByMainlineModuleCases.PublicSubClass.publicMethod()
    publicTestClass.publicMethod();

    /** Should not be reported since PrivateSubClass is a private class. */
    PrivateSubClass privateTestClass = new PrivateSubClass();
    privateTestClass.publicMethod();
  }
}
