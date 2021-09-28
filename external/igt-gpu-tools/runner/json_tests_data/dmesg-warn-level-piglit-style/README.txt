When --dmesg-warn-level is set to 6 also KERN_INFO level messages should be
treated as warnings triggering a result change to dmesg-warn/dmesg-fail.

This makes sure that the piglit-style-dmesg does not clash with
dmesg-warn-level. All the messages are prefixed with drm_ to fit the filters.

IGT messages were artificially bumped to KERN_DEBUG to not pollute the warnings.
