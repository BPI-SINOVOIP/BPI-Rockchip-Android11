#!/usr/bin/env python3
#
# Copyright 2018 - The Android Open Source Project
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
"""Define errors that are raised by AIDEgen."""


class AIDEgenError(Exception):
    """Base AIDEgen exception."""


class BuildFailureError(AIDEgenError):
    """Raised when a build failed."""


class GenerateIDEProjectFileError(AIDEgenError):
    """Raised when IDE project files are not generated."""


class JsonFileNotExistError(AIDEgenError):
    """Raised when a json file does not exist."""


class EmptyModuleDependencyError(AIDEgenError):
    """Raised when the module dependency is empty. Note that even
    a standalone module without jar dependency shall have its src path as
    dependency.
    """

class ProjectOutsideAndroidRootError(AIDEgenError):
    """Raised when a project to be generated IDE project file is not under
    source tree's root directory."""


class ProjectPathNotExistError(AIDEgenError):
    """Raised when a project path does not exist."""


class NoModuleDefinedInModuleInfoError(AIDEgenError):
    """Raised when a module is not defined in module-info.json."""


class IDENotExistError(AIDEgenError):
    """Raised if no IDE exists in a specific path."""


class FakeModuleError(AIDEgenError):
    """Raised if the module is a fake module."""


class InvalidXMLError(AIDEgenError):
    """Raised if parsing xml file failed."""


class InstanceNotExistError(AIDEgenError):
    """Raised if instance does not exist."""


class ModuleInfoEmptyError(AIDEgenError):
    """Raised if module's info dictionary is empty."""


class NoModuleNameDefinedInModuleInfoError(AIDEgenError):
    """Raised if 'module_name' key isn't defined in module's info dictionary."""


class NoPathDefinedInModuleInfoError(AIDEgenError):
    """Raised if 'path' key isn't defined in module's info dictionary."""


# The following error is used by aidegen_functional_test module.
class CommitIDNotExistError(AIDEgenError):
    """Raised if the commit id doesn't exist."""
