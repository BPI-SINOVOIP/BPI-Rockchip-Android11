#!/bin/bash
# Author: kenjc@rock-chips.com
# Support for create all projects in gerrit.
# Projects list could be generated from the following cmd:
# USE:
# ./genernate_remove.sh /PATH/to/default.xml out_put.xml

PORT=29418
HOST=10.10.10.29
DEFAULT_OWNERS=android-owners
DEFAULT_PARENT=Android-Projects

LOCAL_LOG_FILE=/dev/null

PROJECT_LIST_FILE=$1
REMOVE_FILE=$2

gerrit_projects_array={}
local_project_array={}

echo "input file: $PROJECT_LIST_FILE"

gerrit_projects_array=($(ssh -p $PORT $HOST gerrit ls-projects))
gerrit_projects_num=${#gerrit_projects_array[@]}
echo "gerrit NUM: $gerrit_projects_num"

local_project_array=($(gitolite_tool -i $PROJECT_LIST_FILE))
local_project_num=${#local_project_array[@]}
echo "local NUM: $local_project_num"

echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $REMOVE_FILE
echo "<manifest>" >> $REMOVE_FILE
echo "    <!-- AUTO Genernated by genernate_remove.sh -->" >> $REMOVE_FILE

generated_project() {
    project_to_be_create=$1
    echo "    <remove-project name=\"$project_to_be_create\"/>" >> $REMOVE_FILE
}

exist_in_gerrit() {
    project_to_be_check=$1
    for tmp_project0 in "${gerrit_projects_array[@]}"
    do
        short_name=${tmp_project0##*android/}
        if [[ "$short_name" = "$project_to_be_check" ]]; then
            return 1
        fi
    done
    return 0
}

scan_all_projects() {
    for tmp_project1 in "${local_project_array[@]}"
    do
        exist_in_gerrit $tmp_project1
        if [[ "1" = $? ]]; then
            echo "found $tmp_project0" >> $LOCAL_LOG_FILE
        else
            generated_project $tmp_project1
        fi
    done
}

scan_all_projects
echo "    <!-- AUTO Genernated done -->" >> $REMOVE_FILE
echo "</manifest>" >> $REMOVE_FILE
