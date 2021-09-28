import inspect


def for_all_tests(decorator):
    """Applies a decorator to all tests within a test class.

    Args:
        decorator: The decorator to apply.

    Returns:
        The class decorator function.
    """

    def _decorate(decorated):
        test_names = []
        for name, value in inspect.getmembers(decorated,
                                              predicate=inspect.isfunction):
            if name.startswith("test_"):
                test_names.append(name)

        for test_name in test_names:
            setattr(decorated, test_name,
                    decorator(getattr(decorated, test_name)))

        return decorated

    return _decorate
