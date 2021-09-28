This directory is for testing igt_runner's results.json creation. Each
subdirectory is a run-result directory, referred to from
runner_json_tests.c, with test run intermediary files without any
results.json. A semi-automatically crafted reference.json file in each
respective directory is compared with igt_runner's generated json and
needs to match semantically; indentation and such doesn't matter, data
has to be equal.

Each directory also contains a README file explaining what the
particular directory aims to test.
