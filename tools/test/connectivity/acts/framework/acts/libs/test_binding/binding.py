from acts import signals


class Binding(object):
    """Creates a binding for a test method with a decorator.

    Python stores all functions as a variable bound to an object. When that
    object is called it will execute the function logic. It is possible to
    create a wrapper object around the real function object to perform custom
    logic and store additional meta-data.

    This object acts as a wrapper for test functions. It allows binding
    additional test logic to a test.
    """

    def __init__(self, inner, arg_modifier=None, before=None, after=None,
                 signal_modifier=None, instance_args=None):
        """
        Args:
            inner: The inner method or other binding being bound to.
            arg_modifier: A function of
                (*args, **kwargs) => args kwargs that will modify the
                arguments to pass to the bound target
            before: A function of (*args, **kwargs) => None that will
                be called before the bound target.
            after: A function of (result, *args, **kwargs) => None
                that will be called after the bound target.
            signal_modifier:  A function of
                (signal, *args, **kwargs) => signal that will be
                called before the signal is sent to modify the signal to send.
        """
        self.instance_args = instance_args or []
        self.arg_modifier = arg_modifier
        self.signal_modifier = signal_modifier
        self.after = after
        self.before = before
        self.inner = inner
        self.__name__ = inner.__name__

    def __get__(self, instance, owner):
        """Called when a new isntance of the test class is created.

        When a new instance of a class is created all method bindings must
        be bound as instance bindings. This transforms the function call
        signature to be func(self, *args, **kwargs) to func(*args, **kwargs).
        The newly created binding handles inserting the self variable so the
        caller does not have to.

        This binding needs to do similar logic by creating a new binding for
        the instance that memorizes the instance as a passed in arg.
        """
        return Binding(self.inner,
                       arg_modifier=self.arg_modifier,
                       before=self.before,
                       after=self.after,
                       signal_modifier=self.signal_modifier,
                       instance_args=[instance] + self.instance_args)

    def __call__(self, *args, **kwargs):
        """Called when the test is executed."""
        full_args = self.instance_args + list(args)

        try:
            if self.arg_modifier:
                full_args, kwargs = self.arg_modifier(self.inner, *full_args,
                                                      **kwargs)

            if self.before:
                self.before(self.inner, *full_args, **kwargs)

            result = 'UNKNOWN ERROR'
            try:
                result = self.inner(*full_args, **kwargs)
            finally:
                if self.after:
                    self.after(self.inner, result, *full_args, **kwargs)

            if result or result is None:
                new_signal = signals.TestPass('')
            else:
                new_signal = signals.TestFailure('')
        except signals.TestSignal as signal:
            new_signal = signal

        if self.signal_modifier:
            new_signal = self.signal_modifier(self.inner, new_signal,
                                              *full_args,
                                              **kwargs)

        raise new_signal

    def __getattr__(self, item):
        """A simple pass through for any variable we do not known about."""
        return getattr(self.inner, item)
