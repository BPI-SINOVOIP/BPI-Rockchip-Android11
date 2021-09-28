public class Foo {
    public Foo() { }

    public static Foo replacementFooMethod() { return Foo() }

    public static DeprecatedFoo badFooMethod() { return new DeprecatedFoo() }

    /**
     * @deprecated Use {@link #replacementFooMethod()} instead.
     */
    @Deprecated
    public static DeprecatedFoo goodFooMethod() { return new DeprecatedFoo() }
}

/**
 * @deprecated Use {@link #Foo} instead.
 */
@Deprecated
public class DeprecatedFoo {
    public DeprecatedFoo() { }
}
