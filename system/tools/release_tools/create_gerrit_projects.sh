#!/bin/bash
# Author: kenjc@rock-chips.com
# Support for create all projects in gerrit.
# Projects list could be generated from the following cmd:
# USE:
# ./create_gerrit_projects.sh ~/AOSP_12/.repo/manifests/default.xml

PORT=29418
HOST=10.10.10.29
DEFAULT_OWNERS=android-owners
DEFAULT_PARENT=Android-Projects

LOCAL_LOG_FILE=/dev/null

PROJECT_LIST_FILE=$1
gerrit_projects_array={}
local_project_array={}

echo "input file: $PROJECT_LIST_FILE"

gerrit_projects_array=($(ssh -p $PORT $HOST gerrit ls-projects))
gerrit_projects_num=${#gerrit_projects_array[@]}
echo "gerrit NUM: $gerrit_projects_num"

local_project_array=($(gitolite_tool -i $PROJECT_LIST_FILE --prefix "android/"))
local_project_num=${#local_project_array[@]}
echo "local NUM: $local_project_num"

create_project() {
    project_to_be_create=$1
    echo "ssh -p $PORT $HOST gerrit create-project $project_to_be_create --owner $DEFAULT_OWNERS --parent $DEFAULT_PARENT" >> $LOCAL_LOG_FILE
    echo "ssh -p $PORT $HOST gerrit create-project $project_to_be_create --owner $DEFAULT_OWNERS --parent $DEFAULT_PARENT"
    ssh -p $PORT $HOST gerrit create-project $project_to_be_create --owner $DEFAULT_OWNERS --parent $DEFAULT_PARENT
}

exist_in_gerrit() {
    project_to_be_check=$1
    for tmp_project0 in "${gerrit_projects_array[@]}"
    do
        if [[ "$tmp_project0" = "$project_to_be_check" ]]; then
            return 1
        fi
    done
    return 0
}

create_all_projects() {
    for tmp_project1 in "${local_project_array[@]}"
    do
        exist_in_gerrit $tmp_project1
        if [[ "1" = $? ]]; then
            echo "found $tmp_project0" >> $LOCAL_LOG_FILE
        else
            create_project $tmp_project1
        fi
    done
}

create_all_projects
