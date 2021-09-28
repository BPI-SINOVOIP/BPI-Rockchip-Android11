#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
import bisect
from collections import namedtuple
import inspect
import numbers


def _fully_qualified_name(func):
    """Returns the fully-qualified name of a function.

    Note: __qualname__ is not the fully qualified name. It is the the fully
          qualified name without the module name.

    See: https://www.python.org/dev/peps/pep-3155/#naming-choice
    """
    return '%s:%s' % (func.__module__, func.__qualname__)


_FrameInfo = namedtuple('_FrameInfo', ['frame', 'filename', 'lineno',
                                       'function', 'code_context', 'index'])


def _inspect_stack():
    """Returns named tuple for each tuple returned by inspect.stack().

    For Python3.4 and earlier, which returns unnamed tuples for inspect.stack().

    Returns:
        list of _FrameInfo named tuples representing stack frame info.
    """
    return [_FrameInfo(*info) for info in inspect.stack()]


def set_version(get_version_func, min_version, max_version):
    """Returns a decorator returning a VersionSelector containing all versions
    of the decorated func.

    Args:
        get_version_func: The lambda that returns the version level based on the
                          arguments sent to versioned_func
        min_version: The minimum API level for calling versioned_func.
        max_version: The maximum API level for calling versioned_func.

    Raises:
        SyntaxError if get_version_func is different between versioned funcs.

    Returns:
        A VersionSelector containing all versioned calls to the decorated func.
    """
    func_owner_variables = None
    for frame_info in _inspect_stack():
        if frame_info.function == '<module>':
            # We've reached the end of the most recently imported module in our
            # stack without finding a class first. This indicates that the
            # decorator is on a module-level function.
            func_owner_variables = frame_info.frame.f_locals
            break
        elif '__qualname__' in frame_info.frame.f_locals:
            # __qualname__ appears in stack frames of objects that have
            # yet to be interpreted. Here we can guarantee that the object in
            # question is the innermost class that contains the function.
            func_owner_variables = frame_info.frame.f_locals
            break

    def decorator(func):
        if isinstance(func, (staticmethod, classmethod)):
            raise SyntaxError('@staticmethod and @classmethod decorators must '
                              'be placed before the versioning decorator.')
        func_name = func.__name__

        if func_name in func_owner_variables:
            # If the function already exists within the class/module, get it.
            version_selector = func_owner_variables[func_name]
            if isinstance(version_selector, (staticmethod, classmethod)):
                # If the function was also decorated with @staticmethod or
                # @classmethod, the version_selector will be stored in __func__.
                version_selector = version_selector.__func__
            if not isinstance(version_selector, _VersionSelector):
                raise SyntaxError('The previously defined function "%s" is not '
                                  'decorated with a versioning decorator.' %
                                  version_selector.__qualname__)
            if (version_selector.comparison_func_name !=
                    _fully_qualified_name(get_version_func)):
                raise SyntaxError('Functions of the same name must be decorated'
                                  ' with the same versioning decorator.')
        else:
            version_selector = _VersionSelector(get_version_func)

        version_selector.add_fn(func, min_version, max_version)
        return version_selector

    return decorator


class _VersionSelector(object):
    """A class that maps API levels to versioned functions for that API level.

    Attributes:
        entry_list: A sorted list of Entries that define which functions to call
                    for a given API level.
    """

    class ListWrap(object):
        """This class wraps a list of VersionSelector.Entry objects.

        This is required to make the bisect functions work, since the underlying
        implementation of those functions do not use __cmp__, __lt__, __gt__,
        etc. because they are not implemented in Python.

        See: https://docs.python.org/3/library/bisect.html#other-examples
        """

        def __init__(self, entry_list):
            self.list = entry_list

        def __len__(self):
            return len(self.list)

        def __getitem__(self, index):
            return self.list[index].level

    class Entry(object):
        def __init__(self, level, func, direction):
            """Creates an Entry object.

            Args:
                level: The API level for this point.
                func: The function to call.
                direction: (-1, 0 or 1) the  direction the ray from this level
                           points towards.
            """
            self.level = level
            self.func = func
            self.direction = direction

    def __init__(self, version_func):
        """Creates a VersionSelector object.

        Args:
            version_func: The function that converts the arguments into an
                          integer that represents the API level.
        """
        self.entry_list = list()
        self.get_version = version_func
        self.instance = None
        self.comparison_func_name = _fully_qualified_name(version_func)

    def __name__(self):
        if len(self.entry_list) > 0:
            return self.entry_list[0].func.__name__
        return '%s<%s>' % (self.__class__.__name__, self.get_version.__name__)

    def print_ranges(self):
        """Returns all ranges as a string.

        The string is formatted as '[min_a, max_a], [min_b, max_b], ...'
        """
        ranges = []
        min_boundary = None
        for entry in self.entry_list:
            if entry.direction == 1:
                min_boundary = entry.level
            elif entry.direction == 0:
                ranges.append(str([entry.level, entry.level]))
            else:
                ranges.append(str([min_boundary, entry.level]))
        return ', '.join(ranges)

    def add_fn(self, fn, min_version, max_version):
        """Adds a function to the VersionSelector for the given API range.

        Args:
            fn: The function to call when the API level is met.
            min_version: The minimum version level for calling this function.
            max_version: The maximum version level for calling this function.

        Raises:
            ValueError if min_version > max_version or another versioned
                       function overlaps this new range.
        """
        if min_version > max_version:
            raise ValueError('The minimum API level must be greater than the'
                             'maximum API level.')
        insertion_index = bisect.bisect_left(
            _VersionSelector.ListWrap(self.entry_list), min_version)
        if insertion_index != len(self.entry_list):
            right_neighbor = self.entry_list[insertion_index]
            if not (min_version <= max_version < right_neighbor.level and
                    right_neighbor.direction != -1):
                raise ValueError('New range overlaps another API level. '
                                 'New range: %s, Existing ranges: %s' %
                                 ([min_version, max_version],
                                  self.print_ranges()))
        if min_version == max_version:
            new_entry = _VersionSelector.Entry(min_version, fn, direction=0)
            self.entry_list.insert(insertion_index, new_entry)
        else:
            # Inserts the 2 entries into the entry list at insertion_index.
            self.entry_list[insertion_index:insertion_index] = [
                _VersionSelector.Entry(min_version, fn, direction=1),
                _VersionSelector.Entry(max_version, fn, direction=-1)]

    def __call__(self, *args, **kwargs):
        """Calls the proper versioned function for the given API level.

        This is a magic python function that gets called whenever parentheses
        immediately follow the attribute access (e.g. obj.version_selector()).

        Args:
            *args, **kwargs: The arguments passed into this call. These
                             arguments are intended for the decorated function.

        Returns:
            The result of the called function.
        """
        if self.instance is not None:
            # When the versioned function is a classmethod, the class is passed
            # into __call__ as the first argument.
            level = self.get_version(self.instance, *args, **kwargs)
        else:
            level = self.get_version(*args, **kwargs)
        if not isinstance(level, numbers.Number):
            kwargs_out = []
            for key, value in kwargs.items():
                kwargs_out.append('%s=%s' % (key, str(value)))
            args_out = str(list(args))[1:-1]
            kwargs_out = ', '.join(kwargs_out)
            raise ValueError(
                'The API level the function %s returned %s for the arguments '
                '(%s). This function must return a number.' %
                (self.get_version.__qualname__, repr(level),
                 ', '.join(i for i in [args_out, kwargs_out] if i)))

        index = bisect.bisect_left(_VersionSelector.ListWrap(self.entry_list),
                                   level)

        # Check to make sure the function being called is within the API range
        if index == len(self.entry_list):
            raise NotImplementedError('No function %s exists for API level %s'
                                      % (self.entry_list[0].func.__qualname__,
                                         level))
        closest_entry = self.entry_list[index]
        if (closest_entry.direction == 0 and closest_entry.level != level or
                closest_entry.direction == 1 and closest_entry.level > level or
                closest_entry.direction == -1 and closest_entry.level < level):
            raise NotImplementedError('No function %s exists for API level %s'
                                      % (self.entry_list[0].func.__qualname__,
                                         level))

        func = self.entry_list[index].func
        if self.instance is None:
            # __get__ was not called, so the function is module-level.
            return func(*args, **kwargs)

        return func(self.instance, *args, **kwargs)

    def __get__(self, instance, owner):
        """Gets the instance and owner whenever this function is obtained.

        These arguments will be used to pass in the self to instance methods.
        If the function is marked with @staticmethod or @classmethod, those
        decorators will handle removing self or getting the class, respectively.

        Note that this function will NOT be called on module-level functions.

        Args:
            instance: The instance of the object this function is being called
                      from. If this function is static or a classmethod,
                      instance will be None.
            owner: The object that owns this function. This is the class object
                   that defines the function.

        Returns:
            self, this VersionSelector instance.
        """
        self.instance = instance
        return self
