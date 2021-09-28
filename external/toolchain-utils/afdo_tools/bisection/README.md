# `afdo_prof_analysis.py`

`afdo_prof_analysis.py` is the main script and entrypoint for this AFDO profile
analysis tool. This tool attempts to determine which part of a "bad" profile is
bad. It does this using several analysis techniques which iterate over provided
good and bad profiles to isolate the problematic portion of the bad profile.
Goodness and badness are determined by the user, by passing a user-provided
bash script. If the program runs successfully to completion, results will be
output to the path specified by `analysis_output_file` as a JSON with the
following keys:

* `seed`: Float, the seed to randomness for this analysis
* `bisect_results`: a sub-JSON with the following keys:
    + `ranges`: 2d list, where each element is a list of functions that are
    problematic in conjunction with one another.
    + `individuals`: individual functions with a bad profile
* `good_only_functions`: Boolean: is the bad profile just missing some
  function profiles (that only the good profile has?)
* `bad_only_functions`: Boolean: does the bad profile have extra function
  profiles (i.e. the good profile doesn't have these functions) causing
  bad-ness?

## Resuming

`afdo_prof_analysis.py` offers the ability to resume profile analysis in case
it was interrupted and the user does not want to restart analysis from the
beginning. On every iteration of the analysis, it saves state to disk (as
specified by the `state_file` flag). By default the tool will resume from this
state file, and this behavior can be disabled by providing the `no_resume` flag
when running the script.

## Usage

### Example Invocation
  `python afdo_prof_analysis.py --good_prof good.txt --bad_prof bad.txt
  --external_decider profile_test.sh --analysis_output_file afdo_results.json`

### Required flags:

  * `good_prof`: A "good" text-based AFDO profile as outputted by
  bin/llvm-profdata (within an LLVM build).
  * `bad_prof`: A "bad" text-based AFDO profile as outputted by
  bin/llvm-profdata (within an LLVM build).
  * `external_decider`: A user-provided bash script that, given a text-based
    AFDO profile as above, has one of the following exit codes:
    + 0: The given profile is GOOD.
    + 1: The given profile is BAD.
    + 125: The goodness of the given profile cannot be accurately determined by
    the benchmarking script.
    + 127: Something went wrong while running the benchmarking script, no
    information about the profile (and this result will cause analysis to abort).
  * `analysis_output_file`: The path of a file to which to write the output.
      analysis results.

### Optional flags:

Note that these are all related to the state-saving feature which is
described above in "Resuming", so feel free to return to this later.
  * `state_file`: An explicit path for saving/restoring intermediate state.
      Defaults to `$(pwd)/afdo_analysis_state.json`.
  * `no_resume`: If enabled, the analysis will not attempt to resume from
    previous state; instead, it will start from the beginning. Defaults to
    False, i.e. by default will always try to resume from previous state if
    possible.
  * `remove_state_on_completion`: If enabled, the state file will be removed
    upon the completion of profile analysis. If disabled, the state file will
    be renamed to `<state_file_name>.completed.<date>` to prevent reusing this
    as intermediate state. Defaults to False.
  * `seed`: A float specifying the seed for randomness. Defaults to seconds
    since epoch. Note that this can only be passed when --no_resume is True,
    since otherwise there is ambiguity in which seed to use.
