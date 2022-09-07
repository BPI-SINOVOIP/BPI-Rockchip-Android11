#!/bin/bash

ACLOUD_PREBUILT_PROJECT_DIR=$ANDROID_BUILD_TOP/prebuilts/asuite
ACLOUD_VENDOR_FOLDER=$ANDROID_BUILD_TOP/vendor/google/tools/acloud
ACLOUD_PRESETUP_FOLDER=$ANDROID_BUILD_TOP/tools/acloud/setup/pre_setup_sh
GOOGLE_CONFIG=$ACLOUD_PRESETUP_FOLDER/google-acloud.config
GOOGLE_SETUP=$ACLOUD_PRESETUP_FOLDER/google_acloud_setup.sh
GREEN='\033[0;32m'
NC='\033[0m'
RED='\033[0;31m'

# Go to a project we know exists and grab user git config info from there.
if [ ! -d $ACLOUD_PREBUILT_PROJECT_DIR ]; then
    # If this doesn't exist, either it's not a lunch'd env or something weird is
    # going on. Either way, let's stop now.
    exit
fi

# Get the user eMail.
pushd $ACLOUD_PREBUILT_PROJECT_DIR &> /dev/null
USER=$(git config --get user.email)
popd &> /dev/null

if [[ $USER == *@google.com ]]; then
    echo "It looks like you're a googler running acloud setup."
    echo -e "Take a look at ${GREEN}go/acloud-googler-setup${NC} first"
    echo "before continuing to enable a streamlined setup experience."
    echo -n "Press [enter] to continue"
    # Display disclaimer to googlers if it doesn't look like their env will
    # enable streamlined setup.
    if [ ! -d $ACLOUD_VENDOR_FOLDER ] && [ ! -f $GOOGLE_CONFIG ] &&
       [ ! -f $GOOGLE_SETUP ]; then
        echo -e " with the ${RED}manual setup flow${NC}."
    fi
    read
fi
