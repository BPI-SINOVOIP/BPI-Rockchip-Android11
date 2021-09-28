# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
ATest decorator.
"""

def static_var(varname, value):
    """Decorator to cache static variable.

    Args:
        varname: Variable name you want to use.
        value: Variable value.

    Returns: decorator function.
    """

    def fun_var_decorate(func):
        """Set the static variable in a function."""
        setattr(func, varname, value)
        return func
    return fun_var_decorate
