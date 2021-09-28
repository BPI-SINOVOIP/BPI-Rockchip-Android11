@echo off
@setlocal enabledelayedexpansion

REM It is to be used with BYOB setup to run tests on cloud VMs.
REM REM It will run ADB tests.
REM It takes 1 command line argument.
REM DIST_DIR => Absolute path for the distribution directory.

REM It will return 0 if it is able to execute tests, otherwise
REM it will return 1.

REM Owner: akagrawal@google.com

set DIST_DIR=%1

echo "Checkout adt-infra repo"
REM $ADT_INFRA has to be set on the build machine. It should have absolute path
REM where adt-infra needs to be checked out.
rmdir /s /q %ADT_INFRA%
git clone https://android.googlesource.com/platform/external/adt-infra -b emu-master-dev %ADT_INFRA%

set BUILD_DIR=C:\buildbot\prebuilt\%BUILD_NUMBER%

setx ANDROID_HOME %SDK_PLAT_TOOLS% /M
setx ANDROID_SDK_ROOT %SDK_PLAT_TOOLS% /M

call refreshenv

echo "Setup new ADB"
rmdir /s /q %ANDROID_SDK_ROOT%\platform-tools
7z x -aoa %BUILD_DIR%\sdk_x86-sdk\sdk-repo* -o%ANDROID_SDK_ROOT%\

echo "Extract tests from general-tests.zip"
7z l %BUILD_DIR%\test_suites_x86_64\general-tests.zip | findstr "adb_integration_test"
if errorlevel 1 goto StartTest

mkdir %DIST_DIR%\general-tests
7z x -aoa %BUILD_DIR%\test_suites_x86_64\general-tests.zip -o%DIST_DIR%\general-tests\ host\testcases\adb_integration_test_*

:StartTest
echo "Run ADB tests from $ADT_INFRA"
set count=0
start %ADT_INFRA%\emu_test\utils\run_test_android_bridge.cmd %DIST_DIR%

:loop
set /a count+=1
sleep 60
tasklist /v | find "run_test_android_bridge"
if errorlevel 1 goto cmdDone

if %count% equ 90 goto cmdKill
goto loop

:cmdKill
echo "ADB test timed out"
taskkill /fi "windowtitle eq run_test_android_bridge*"

:cmdDone
cmd.exe /c %ANDROID_HOME%\platform-tools\adb.exe kill-server

exit 0
