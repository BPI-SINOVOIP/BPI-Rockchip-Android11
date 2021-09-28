# LLVM Tools

## Overview

These scripts helps automate tasks such as updating the LLVM next hash,
determing whether a new patch applies correctly, and patch management.

In addition, there are scripts that automate the process of retrieving the
git hash of LLVM from either google3, top of trunk, or for a specific SVN
version.

**NOTE: All scripts must must be run outside the chroot**

**NOTE: sudo must be permissive (i.e. **`cros_sdk`** should NOT prompt for a
password)**

## `update_packages_and_run_tryjobs.py`

### Usage

This script is used for updating a package's `LLVM_NEXT_HASH` (sys-devel/llvm,
sys-libs/compiler-rt, sys-libs/libcxx, sys-libs/libcxxabi, and
sys-libs/llvm-libunwind) and then run tryjobs after updating the git hash.

An example when this script should be run is when certain boards would like
to be tested with the updated `LLVM_NEXT_HASH`.

For example:

```
$ ./update_packages_and_run_tryjobs.py \
  --llvm_version tot \
  --options nochromesdk latest-toolchain \
  --builders kevin-release-tryjob nocturne-release-tryjob
```

The above example would update the packages' `LLVM_NEXT_HASH` to the top of
trunk's git hash and would submit tryjobs for kevin and nocturne boards, passing
in 'nochromesdk' and 'latest-toolchain' for each tryjob.

For help with the command line arguments of the script, run:

```
$ ./update_packages_and_run_tryjobs.py --help
```

Similarly as the previous example, but for updating `LLVM_NEXT_HASH` to
google3:

```
$ ./update_packages_and_run_tryjobs.py \
  --llvm_version google3 \
  --options nochromesdk latest-toolchain \
  --builders kevin-release-tryjob nocturne-release-tryjob
```

Similarly as the previous example, but for updating `LLVM_NEXT_HASH` to
the git hash of revision 367622:

```
$ ./update_packages_and_run_tryjobs.py \
  --llvm_version 367622 \
  --options nochromesdk latest-toolchain \
  --builders kevin-release-tryjob nocturne-release-tryjob
```

## `update_chromeos_llvm_next_hash.py`

### Usage

This script is used for updating a package's/packages' `LLVM_NEXT_HASH` and
creating a change list of those changes which will uploaded for review. For
example, some changes that would be included in the change list are
the updated ebuilds, changes made to the patches of the updated packages such
as being removed or an updated patch metadata file. These changes are determined
by the `--failure_mode` option.

An example where this script would be used is when multiple packages need to
have their `LLVM_NEXT_HASH` updated.

For example:

```
$ ./update_chromeos_llvm_next_hash.py \
  --update_packages sys-devel/llvm sys-libs/compiler-rt \
  --llvm_version google3 \
  --failure_mode disable_patches
```

The example above would update sys-devel/llvm and sys-libs/compiler-rt
`LLVM_NEXT_HASH` to the latest google3's git hash of LLVM. And the change list
may include patches that were disabled for either sys-devel/llvm or
sys-libs/compiler-rt.

For help with the command line arguments of the script, run:

```
$ ./update_chromeos_llvm_next.py --help
```

For example, to update `LLVM_NEXT_HASH` to top of trunk of LLVM:

```
$ ./update_chromeos_llvm_next_hash.py \
  --update_packages sys-devel/llvm sys-libs/compiler-rt \
  --llvm_version tot \
  --failure_mode disable_patches
```

For example, to update `LLVM_NEXT_HASH` to the git hash of revision 367622:

```
$ ./update_chromeos_llvm_next_hash.py \
  --update_packages sys-devel/llvm sys-libs/compiler-rt \
  --llvm_version 367622 \
  --failure_mode disable_patches
```

## `llvm_patch_management.py`

### Usage

This script is used to test whether a newly added patch in a package's patch
metadata file would apply successfully. The script is also used to make sure
the patches of a package applied successfully, failed, etc., depending on the
failure mode specified.

An example of using this script is when multiple packages would like to be
tested when a new patch was added to their patch metadata file.

For example:

```
$ ./llvm_patch_management.py \
  --packages sys-devel/llvm sys-libs/compiler-rt \
  --failure_mode continue
```

The above example tests sys-devel/llvm and sys-libs/compiler-rt patch metadata
file with the failure mode `continue`.

For help with the command line arguments of the script, run:

```
$ ./llvm_patch_management.py --help
```

## `patch_manager.py`

### Usage

This script is used when when all the command line arguments are known such as
testing a specific metadata file or a specific source tree.

For help with the command line arguments of the script, run:

```
$ ./patch_manager.py --help
```

For example, to see all the failed (if any) patches:

```
$ ./patch_manager.py \
  --svn_version 367622 \
  --patch_metadata_file /abs/path/to/patch/file \
  --filesdir_path /abs/path/to/$FILESDIR \
  --src_path /abs/path/to/src/tree \
  --failure_mode continue
```

For example, to disable all patches that failed to apply:

```
$ ./patch_manager.py \
  --svn_version 367622 \
  --patch_metadata_file /abs/path/to/patch/file \
  --filesdir_path /abs/path/to/$FILESDIR \
  --src_path /abs/path/to/src/tree \
  --failure_mode disable_patches
```

For example, to remove all patches that no longer apply:

```
$ ./patch_manager.py \
  --svn_version 367622 \
  --patch_metadata_file /abs/path/to/patch/file \
  --filesdir_path /abs/path/to/$FILESDIR \
  --src_path /abs/path/to/src/tree \
  --failure_mode remove_patches
```

For example, to bisect a failing patch and stop at the first bisected patch:

```
$ ./patch_manager.py \
  --svn_version 367622 \
  --patch_metadata_file /abs/path/to/patch/file \
  --filesdir_path /abs/path/to/$FILESDIR \
  --src_path /abs/path/to/src/tree \
  --failure_mode bisect_patches \
  --good_svn_version 365631
```

For example, to bisect a failing patch and then continue bisecting the rest of
the failed patches:

```
$ ./patch_manager.py \
  --svn_version 367622 \
  --patch_metadata_file /abs/path/to/patch/file \
  --filesdir_path /abs/path/to/$FILESDIR \
  --src_path /abs/path/to/src/tree \
  --failure_mode bisect_patches \
  --good_svn_version 365631 \
  --continue_bisection True
```

## LLVM Bisection

### `llvm_bisection.py`

#### Usage

This script is used to bisect a bad revision of LLVM. After the script finishes,
the user requires to run the script again to continue the bisection. Intially,
the script creates a JSON file that does not exist which then continues
bisection (after invoking the script again) based off of the JSON file.

For example, assuming the revision 369420 is the bad revision:

```
$ ./llvm_bisection.py \
  --parallel 3 \
  --start_rev 369410 \
  --end_rev 369420 \
  --last_tested /abs/path/to/tryjob/file/ \
  --extra_change_lists 513590 \
  --builder eve-release-tryjob \
  --options latest-toolchain
```

The above example bisects the bad revision (369420), starting from the good
revision 369410 and launching 3 tryjobs in between if possible (`--parallel`).
Here, the `--last_tested` path is a path to a JSON file that does not exist. The
tryjobs are tested on the eve board. `--extra_change_lists` and `--options`
indicate what parameters to pass into launching a tryjob.

For help with the command line arguments of the script, run:

```
$ ./llvm_bisection.py --help
```

### `auto_llvm_bisection.py`

#### Usage

This script automates the LLVM bisection process by using `cros buildresult` to
update the status of each tryjob.

An example when this script would be used to do LLVM bisection overnight
because tryjobs take very long to finish.

For example, assuming the good revision is 369410 and the bad revision is
369420, then:

```
$ ./auto_llvm_bisection.py --start_rev 369410 --end_rev 369420 \
  --last_tested /abs/path/to/last_tested_file.json \
  --extra_change_lists 513590 1394249 \
  --options latest-toolchain nochromesdk \
  --builder eve-release-tryjob
```

The example above bisects LLVM between revision 369410 and 369420. If the file
exists, the script resumes bisection. Otherwise, the script creates the file
provided by `--last_tested`. `--extra_change_lists` and `--options` are used for
each tryjob when being submitted. Lastly, the tryjobs are launched for the board
provided by `--builder` (in this example, for the eve board).

For help with the command line arguments of the script, run:

```
$ ./auto_llvm_bisection.py --help
```

### `update_tryjob_status.py`

#### Usage

This script updates a tryjob's 'status' value when bisecting LLVM. This script
can use the file that was created by `llvm_bisection.py`.

An example when this script would be used is when the result of tryjob that was
launched was 'fail' (due to flaky infra) but it should really have been
'success'.

For example, to update a tryjob's 'status' to 'good':

```
$ ./update_tryjob_status.py \
  --set_status good \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file
```

The above example uses the file in `--status_file` to update a tryjob in that
file that tested the revision 369412 and sets its 'status' value to 'good'.

For help with the command line arguments of the script, run:

```
$ ./update_tryjob_status.py --help
```

For example, to update a tryjob's 'status' to 'bad':

```
$ ./update_tryjob_status.py \
  --set_status bad \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file
```

For example, to update a tryjob's 'status' to 'pending':

```
$ ./update_tryjob_status.py \
  --set_status pending \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file
```

For example, to update a tryjob's 'status' to 'skip':

```
$ ./update_tryjob_status.py \
  --set_status skip \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file
```

For example, to update a tryjob's 'status' based off a custom script exit code:

```
$ ./update_tryjob_status.py \
  --set_status custom_script \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file \
  --custom_script /abs/path/to/script.py
```

### `update_all_tryjobs_with_auto.py`

#### Usage

This script updates all tryjobs that are 'pending' to the result provided by
`cros buildresult`.

For example:

```
$ ./update_all_tryjobs_with_auto.py \
  --last_tested /abs/path/to/last_tested_file.json \
  --chroot_path /abs/path/to/chroot
```

The above example will update all tryjobs whose 'status' is 'pending' in the
file provided by `--last_tested`.

For help with the command line arguments of the script, run:

```
$ ./update_all_tryjobs_with_auto.py --help
```

### `modify_a_tryjob.py`

#### Usage

This script modifies a tryjob directly given an already created tryjob file when
bisecting LLVM. The file created by `llvm_bisection.py` can be used in this
script.

An example when this script would be used is when a tryjob needs to be manually
added.

For example:

```
$ ./modify_a_tryjob.py \
  --modify_a_tryjob add \
  --revision 369416 \
  --extra_change_lists 513590 \
  --options latest-toolchain \
  --builder eve-release-tryjob \
  --status_file /abs/path/to/tryjob/file
```

The above example creates a tryjob that tests revision 369416 on the eve board,
passing in the extra arguments (`--extra_change_lists` and `--options`). The
tryjob is then inserted into the file passed in via `--status_file`.

For help with the command line arguments of the script, run:

```
$ ./modify_a_tryjob.py --help
```

For example, to remove a tryjob that tested revision 369412:

```
$ ./modify_a_tryjob.py \
  --modify_a_tryjob remove \
  --revision 369412 \
  --status_file /abs/path/to/tryjob/file
```

For example, to relaunch a tryjob that tested revision 369418:

```
$ ./modify_a_tryjob.py \
  --modify_a_tryjob relaunch \
  --revision 369418 \
  --status_file /abs/path/to/tryjob/file
```

## Other Helpful Scripts

### `get_llvm_hash.py`

#### Usage

The script has a class that deals with retrieving either the top of trunk git
hash of LLVM, the git hash of google3, or a specific git hash of a SVN version.
It also has other functions when dealing with a git hash of LLVM.

In addition, it has a function to retrieve the latest google3 LLVM version.

For example, to retrieve the top of trunk git hash of LLVM:

```
from get_llvm_hash import LLVMHash

LLVMHash().GetTopOfTrunkGitHash()
```

For example, to retrieve the git hash of google3:

```
from get_llvm_hash import LLVMHash

LLVMHash().GetGoogle3LLVMHash()
```

For example, to retrieve the git hash of a specific SVN version:

```
from get_llvm_hash import LLVMHash

LLVMHash().GetLLVMHash(<svn_version>)
```

For example, to retrieve the latest google3 LLVM version:

```
from get_llvm_hash import GetGoogle3LLVMVersion

GetGoogle3LLVMVersion(stable=True)
```
