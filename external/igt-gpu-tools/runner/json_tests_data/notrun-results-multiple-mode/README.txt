Tests that are not executed are expected to be present in results.json
when possible. If run with --multiple-mode, we can't distinguish
tests without subtests from tests where we attempt to execute all
subtests.
