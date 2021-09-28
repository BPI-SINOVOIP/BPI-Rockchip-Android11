# Overview
This directory contains JSON files describing metadata of AFDO profiles
used to compile packages (Chrome and kernel) in Chrome OS.

# Description of each JSON Files
kernel_afdo.json contains the name of the latest AFDO profiles for each
kernel revision.

chrome_afdo.json contains the name of the latest AFDO profiles used in
latest Chrome revision, including both benchmark and CWP profiles.

# Usage
## Updates
When a new profile (kernel or Chrome) is successfully uploaded to the
production GS bucket, a bot submits to modify the corresponding JSON
file to reflect the updates.

## Roll to Chrome OS
There will be scheduler jobs listening to the changes made to these
JSON files. When changes detected, buildbot will roll these changes into
corresponding Chrome OS packages.
