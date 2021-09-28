# PythonVirtualenvPreparerTest

This directory tests the functionality of VtsPythonVirtualenvPreparer.


Two modules are included in this project:

* VtsSelfTestPythonVirtualenvPreparerTestPart0: to verify the python module (numpy) that's going to be tested has not been installed by default.
* VtsSelfTestPythonVirtualenvPreparerTestPart1: test duplicated module preparer to install a new module and empty module preparer.
* VtsSelfTestPythonVirtualenvPreparerTestPart2: test whether a python module installed in previous tests is still available through plan level virtual environment

The naming of `Part0`, `Part1` and `Part2` is to ensure the order of execution.
