class Bar {
    fun replacementBarMethod(): Bar { return Bar() }

    fun badBarMethod(): DeprecatedBar { return DeprecatedBar() }

    /**
     * @deprecated Use {@link #replacementBarMethod()} instead.
     */
    @Deprecated
    fun goodBarMethod(): DeprecatedBar { return DeprecatedBar() }
}

/**
 * @deprecated Use {@link #Bar} instead.
 */
@Deprecated
class DeprecatedBar