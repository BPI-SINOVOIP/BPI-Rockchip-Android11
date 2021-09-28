When --dmesg-warn-level is set to 1 to make sure that changing the setting works
also in this direction.

This makes sure that the piglit-style-dmesg does not clash with
dmesg-warn-level. All the messages are prefixed with drm_ to fit the filters.
