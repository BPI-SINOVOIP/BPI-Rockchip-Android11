When a test hangs before it even enters a subtest, we don't have a
journal entry for that subtest. This test checks that the result still
gets attributed to a subtest, since we know from the job_list that a
subtest should be executed.
