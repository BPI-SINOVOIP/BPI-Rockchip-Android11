A test project that publishes a Binder service. The methods of this service
check if their caller has certain permissions. This service is used by
Context tests to verify that methods like enforceCallingPermission()
work correctly.

This service has to be in a separate package so the permissions of the
caller (the test) and the callee (this service) are different.
