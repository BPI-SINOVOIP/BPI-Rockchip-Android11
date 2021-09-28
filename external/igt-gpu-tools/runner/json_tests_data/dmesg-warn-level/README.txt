When --dmesg-warn-level is set to 6 also KERN_INFO level messages should be
treated as warnings triggering a result change to dmesg-warn/dmesg-fail.

IGT messages were artifically bumped to KERN_DEBUG to not pollute the warnings.
