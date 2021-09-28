# Autotest Documentation For Enterprise
To provide all the information needed about the current state of Enterprise
autotest automation. Current coverage, location of tests, how to execute
the tests, what machine to run the tests on, test breakdown, etc.

[TOC]

## Current coverage

Calculating coverage could be tricky as there are many different ways
it could be done. We were using two ways to do it:

*   By policy:
    *   Look at this recently updated [spreadsheet](http://go/ent-pol-auto):
        There are 265 policies available for ChromeOS via C/D Panel. We have
        96 policies automated, 75 of those are in C/D Panel. So that’s
        75/264 = %28 coverage + 21 more tests covering various other policies.
*   By section:
    *   Refer to this recently updated [spreadsheet](http://go/ent-sec-auto)
        in which we list out current coverage.

## Test Location

*	Tests that automate user policies are located [here](http://go/usr-pol-loc).
*	Tests that automate device policies are located [here](http://go/dev-pol-loc).
*	Most of Enterprise tests start with *policy_* but there are some
	that begin with *enterprise_*.

## Test Results

*   The best way to view test results is by using stainless:
*   Go to https://stainless.corp.google.com/
*   Click on Test History Matrix
*   In the Test dropdown, select “policy_*”
*   Hit Search and you should see the results like so:
![Results](https://screenshot.googleplex.com/UxMiYrVMDdF.png)

## Running a test

A test can be executed using this command from chroot:
```sh
test_that --board=$BOARD_NAME $IP_ADDRESS FULL_TEST_NAME*
```
Example:
```sh
/trunk/src/scripts $ test_that --board=hana 100.107.106.138
policy_DeviceServer.AllowBluetooth_true
```

**--board** - should be the board that you have setup locally. You only need to
setup the board ones and you shouldn’t have to touch it again for a long time.
The board that you setup on your workstation doesn’t have to match the
DUT(device under test) board that you’re executing the test on. To set up the
board please follow instructions [here](http://go/run-autotest). You will also
need to run the build_packages command.

**IP_ADDRESS** - IP of the DUT. If you have a device locally, it needs to be
plugged into the test network and not corp network. You can also use a device
in the lab. To reserve a device from the lab please follow these steps:

*	Setup skylab using go/skylab-tools-guide (Advanced users: Manual
	installation)
*	"Lease" a dut go/skylab-dut-locking
*   Grab the host name, for example: chromeos15-row3-rack13-host2. Do not
	include the prefix (e.g. "crossk")
*	Use this as the IP: chromeos15-row3-rack13-host2**.cros**.

Full test name - test name can be grabbed from the control file.
[Example](http://go/control-file-name).

You can check other options for test_that by running: *test_that --help*.

## Setting up a local DUT

To run a test on a local DUT you need to make sure the DUT has been
properly setup with a test build. You can use this helpful
[tool](http://go/crosdl-usage). Execute from [here](https://cs.corp.google.com/chromeos_public/src/platform/crostestutils/provingground/crosdl.py)
Run this command to put the build on a USB stick:
```sh
*./crosdl.py -c dev -t -b 12503.0.0 -p sarien --to_stick /dev/sda*
```
Or this command to update the DUT directly(flaky):
```sh
*./crosdl.py -c dev -t -b 12105.54.0 -p sarien --to_ip $IP_ADDRESS*
```
Note: The DUT must be reachable via SSH for this to work.


To find out the right build number, please use [goldeneye](http://go/goldeneye)
and search for the right build for your board.

## Test Breakdown

See the [Autotest Best Practices](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/refs/heads/master/docs/best-practices.md#control-files) for general autotest information.
This section will provide details on how Enterprise autotests are written.
Each test will require the following:
*	A control file
*	Control files for each test configuration
*	A .py defining the test, which inherits test.test

### Control files

[Control files](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/refs/heads/master/docs/best-practices.md#control-files) are used as the entry point to a test.
A typical dir for a user policy (client) test will consist of control file(s)
and, along with .py test file(s). A control file will contain basic description of the
test as well as options such as these:
``` python
	AUTHOR = 'name'
	NAME = 'full_test_name'
	ATTRIBUTES = 'suite:ent-nightly, suite:policy'
	TIME = 'SHORT'
	TEST_CATEGORY = 'General'
	TEST_CLASS = 'enterprise'
	TEST_TYPE = 'client'
```

On a user policy (client) test, there will be a base control file, plus an
additional file for each test configuration. [Example](https://cs.corp.google.com/aosp-android10/external/autotest/client/site_tests/policy_AllowDinosaurEasterEgg/)
In this example there is the "base" control file, with no args specified, which
is simply named "control". Additionally there is a control file for each
configuration of the test (.allow, .disallow, .not_set). The args to be
passed to the test (.py) are specified in the final line of each of those
control files. Example:
``` python
job.run_test('policy_AllowDinosaurEasterEgg',
             case=True)
````

### Test file

Example of a basic [test](http://go/basic-ent-test).
The class name of the test, ```policy_ShowHomeButton``` has to match the name
of the .py file, and should ideally match the directory name as well.

**run_once** - The function that gets called first. Parameters from the
	control passed into this function.

**setup_case** - sets up DMS, logs in, verifies policies values and various
other login arguments. Defined: [enterprise_policy_base](http://go/ent-pol-base). Explained in detail below.

**start_ui_root** - needed if you’re planning on interacting with UI objects
during your test. Defined:[ui_utils](http://go/ent-ui-utils).
This [CL](http://crrev.com/c/1531141) describes what ui_utils is based off
and the usefulness of it.

**check_home_button** - Function that verifies the presence of the Home button
in this test. Depending on the policy setting, the test is using
*ui.item_present* to verify the status of the Home button.

Every enterprise test will require a run_once function and will most likely
require setup_case. You will need to pass in a dictionary with the policy
name and value into setup_case.

### Useful utility

This [utils.py](http://go/ent_util) file which contains many useful functions
that you’ll come across in tests.

**Some examples:**

*	**poll_for_condition** - keeps checking for condition to be true until a
	timeout is reached at which point an error is raised.
*	**run** - runs a shell command on the DUT.

### Difference between device policy test and user policy test

To run test device policies the DUT will need to be fully enrolled, starting
with a cleared TPM (thus a reboot). Client tests do not support rebooting the
device before/during/after a test.

In order to support clearing the TPM & rebooting, all device policies must be
written as a ["server"](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/refs/heads/master/docs/best-practices.md#when_why-to-write-a-server_side-test) test.
Server tests (for Enterprise) will need a "server" control & test, in addition
to having a client control file and a .py test file. The server test will do
any server operations (reboot, servo control, wifi cell control, etc)

Below is an example of testing a device
[Example](http://go/ent-cont-example) of the server control file. This will
run the server test [policy_DeviceServer](http://go/ent-test-example) and pass the parameters specified.
The server test will clear the tpm, create an autotest client of the DUT, then
run the autotest specified in the control file policy_DeviceAllowBluetooth.

**Note** The parameterization control files are all of the server control
files. The Client side [control file](http://go/ent-device-client-example) is only a
pass through for the parameters from the control file, and does not set any new
behavior.

### Debugging an autotest

Unfortunately there's no good debugging tool in autotest and you can't use pdb
so you're left with using time.sleep and logging. With time.sleep you can pause
the test and see what's going on in the actual device. When using logging you
can run 'logging.info("what you want to log")' and then when the test is done
running you can check the results here:
/tmp/test_that_latest/results-1-TESTNAME/TESTNAME/debug/TESTNAME.INFO

If a test is failing remotely, on stainless, you can view the logs there by
clicking on the Logs link. You can also see the screenshot of the screen
when a test errored/failed.

### Using Servo board with Autotests

Some tests require the use of the [Servo Board](http://go/servo-ent).
If you want to get ahold of a servo board you need to reach out to crosdistros@
and request one. You can either get a Servo type A or Servo type C, in case
your test involves controlling the power to the DUT.

Setting up the servo, hopefully you'll find this
[screenshot](https://screenshot.googleplex.com/PcZGhW5eqk3) useful. You can see
that two cables on the left go to the DUT and the cable on the right goes into
the host machine. If you're going to be feeding the power to the DUT you will
also need to connect a Type-C charger to the Servo by plugging it into the
slot marked "Dut Power". Note: if you grabbed the micro usb -> USB A cables
in the tech stop make sure that the light on the switch glows orange and not
green. If it's green the tests will not work.

Starting the servo, from chroot run: "sudo servo_updater" make sure everything
is up to date. Then run "sudo servod -b BOARD_NAME" BOARD_NAME being the board
you have built on your server. While this is running, in another terminal tab
you can now execute dut-control commands such as
"dut-control servo_v4_role:scr".

With the servod running you can now execute local tests using the servo board.
[Example test using servo](http://go/servo-ent-example-test).

## Enterprise Autotest Infra

This section will focus on a basic explination of the [Enterprise base class](http://go/ent-pol-base)
used for autotest, along with commonly used calls, APIs, etc.

### Base class overview:

The enterprise base class currently supports the following:
*	Enrolling with a fake account & DMS through the full OOBE flow. Commonly
		used for device policy testing)
*	Kiosk enrollment with fake account
*	Enrolling for user policies (not requiring OOBE flow).
*	Enterprise ARC tests
*	Logging in with a real account/DMS
*	Enrolling with a real account- currently broken see http://crbug.com/1019320
*	Configuring User/Device/Extension policies with a fake DMS
*	Obtaining policies through an API
*	Verifying policies
*	UI interaction

In addition to the features above, the base class will setup chrome for
testing. This includes passing in username/password, browser flags, ARC
settings, etc.


### Policy Management

Policy Managing with a fake DMS is mostly handled via the [policy_manager](http://go/ent-pol-manager).

The Enterprise base class uses the policy manager to configure policies,
set the policies with the fake DMS server, obtain policies from a DUT, and
verify they are properly set (ie match the configured). In addition the policy
manager handles features such as adding/updating/removing policies once after
the initial setup, and make complex testing, such as extension of obfuscated
policies easier to test.

If a test is to fail with "Policy <POLICY_NAME> value was not set correctly.",
the verification within the policy_manager is failing. This means the policy
that was configured via the policy_manager does not match the value obtained
from the DUT.

When using the fake DMS (see [enterprise_fake_dmserver](http://go/fake-ent-dms)and [policy_testserver](http://go/fake-policy-server),
policies are provided to the fDMS via a json blob which is created by the
policy_manager.

Policies from the DUT are obtained via an autotestprivate API, called via
the [enterprise_policy_utils](http://go/ent-pol-utils) ```get_all_policies```
and policies are refreshed (ie force a refetch from the DMS) via
```refresh_policies```.

### Enrollment and Kiosk Mode

Enterprise autotest uses the autotest [enrollment](http://go/ent-at-enrollment) to support
device enrollment.

This class has the ability to enroll both real and fake accounts, including
walking through the enrollment OOBE flow. The actual interaction with the
UI/APIs for login is acomplished by calling telemetry.

Additionally Kiosk mode is also supported.


### Chrome

Tests interact with chrome (ie launch, define plugins, ARC settings, etc) via
[chrome.py](http://go/autotest-chrome). chrome.py is built upon telemetry
for browser interactions. The base class will handle chrome
interaction for you, however there are specific examples such as the
enrollment retainment test, that will interact with chrome.py directly.


### Common Issues and possible solutions

*	Historically there have been issues with DUT enrollment via APIs. As of
	R80-x, this should be resolved. Typically enrollment issues have an error
	message along the lines of:
	```test did not pass (reason: Unhandled TimeoutException: Timed out while waiting 60s for _EnterpriseWebviewVisible.).```
	If this error is seen, it is typically related to something during the OOBE
	flow, when waiting for the enterprise enrollment screen.
*	Some of the Enterprise Autotests use UI interaction/reading for the tests.
	These UI elements change somewhat often, and will occasionally cause these
	tests to break. UI based errors usually have a traceback leading to
	ui.utils, and can often be fixed by simply update the UI element the test
	is looking for.
*	Errors from chrome.py can also lead to Enterprise tests failing. This
	package is not directly owned by Enterprise, or anyone other group, but
	is a shared resource. If a test fails due to this package, it is likely
	up to the test owner to fix, but they should be cognisant of other teams
	using the package.
*	inspector_backend timeouts occasionally occur (<0.5% of all tests.)
	The problem is traces backto a inspector backend crash/disconnect between
	telemetry and the DUT.This error is well outside the scope of Enterprise
	autotest. Rerunning the	test is likely the easiest solution


