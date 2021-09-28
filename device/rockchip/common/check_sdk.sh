#!/bin/bash

handleString() {
	if [[ $1 == *"name=\""* ]];then
		PROJECT_NEME=${1##*name=\"}
		PROJECT_NEME=${PROJECT_NEME%%\"}
	elif [[ $1 == *"revision=\""* ]];then
		PROJECT_COMMIT_ID=${1##*revision=\"}
		PROJECT_COMMIT_ID=${PROJECT_COMMIT_ID%%\"}
	elif [[ $1 == *"path=\""* ]];then
		PROJECT_PATH=${1##*path=\"}
		PROJECT_PATH=${PROJECT_PATH%%\"}
	fi
}

clearVar() {
	PROJECT_NEME=""
	PROJECT_PATH=""
	PROJECT_COMMIT_ID=""
}

checkCode() {
	if [[ ! -z $PROJECT_PATH ]]; then
		if [[ ! -d $PROJECT_PATH ]]; then
			echo "WARN:"$PROJECT_PATH" doesn't exist."
			return 1
		fi
		cd $PROJECT_PATH
		RESULT=`git log`
	elif [[ ! -z $PROJECT_NEME ]];then
		if [[ ! -d $PROJECT_NEME ]]; then
			echo "WARN:"$PROJECT_NEME" doesn't exist."
			return 1
		fi
		cd $PROJECT_NEME
		RESULT=`git log |grep "commit" |grep -v "^ "`
	fi
	if [[ ! $RESULT == *"commit "$PROJECT_COMMIT_ID* ]];then
		echo "ERROR: please sync Project "$PROJECT_NEME
		ERROR_PROJECT=$PROJECT_NEME","$ERROR_PROJECT
	fi
	cd - >> /dev/null
}

if [[ $PWD == *\/device\/rockchip\/common ]]
then
	cd ../../..
	echo "Enter $PWD"
fi

if [[ ! -f .repo && ! -f .repo/manifest.xml ]]
then
	echo "Dir .repo and File .repo/manifest.xml doesn't exist."
	exit 1
fi

cd .repo/manifests/

echo "Comparing with the remote server."
#目前仅支持211服务器上的.repo/manifest
BRANCH=$(git branch -vv |sed -n -e 's/^\* \(.*\)/\1/p' |awk -F '[' '{print $2}' |awk -F ']' '{print $1}' |awk -F ':' '{print $1}')
#截取"/"号之后的内容
BRANCH_RESULT=${BRANCH#*\/}
# 获取远程仓库
ORIGIN=`git remote`
# fetch仓库的远程分支
git fetch $ORIGIN refs/heads/$BRANCH_RESULT:refs/remotes/$ORIGIN/$BRANCH_RESULT --no-tags
# 获取当前manifest.xml
MANIFEST=`ls -l ../manifest.xml`
MANIFEST=${MANIFEST##*-> manifests/}
# 获取远程分支的最新一个提交
RESULT=`git log $BRANCH $MANIFEST |head -1`
if [[ ! `git log |grep "commit" |grep -v "^ "` == *$RESULT* ]]; then
	echo "WARN:A new version found! Please check .repo/manifests and update it."
	exit 0
fi
echo "The SDK is already up to date and no update is required."

RESULT=`git remote -v`
#TODO 镜像服务器未处理
if [[ ! $RESULT == *www\.rockchip\.com\.cn* ]]
then
	echo "The current SDK isn't a release SDK. Don't check code."
	exit 0
fi

cd - >> /dev/null


echo "Generates the current manifest.xml."
RESULT=$(.repo/repo/repo manifest -r -o current_manifest.xml 2>&1)

if [[ $RESULT == "Saved manifest to current_manifest.xml" ]]; then
	echo "Saved manifest to current_manifest.xml"
else
	echo $RESULT
	echo "ERROR: An exception occurred in the SDK, please fix it according to the error message."
	rm current_manifest.xml
	exit 1
fi

if [ ! -f current_manifest.xml ]
then
	echo "ERROR: Failed to generates the current manifest.xml."
	exit 1
fi

if [[ `cat current_manifest.xml` == "" ]]
then
	echo "ERROR: Failed to generates the current manifest.xml."
	rm current_manifest.xml
	exit 1
fi

diff .repo/manifest.xml current_manifest.xml > temp.txt

while read line
do
	# 去除"<"、">"
	line=${line##<}
	line=${line%%\/>}
	# 排除不带有project的行
	if [[ ! $line == *"project"* ]];then
		continue
	fi
	# 处理字符串
	array=(${line//\ \ })
	for index in "${!array[@]}"; do
		handleString ${array[index]}
	done
	IS_REPEAT=0
	error_array=(${ERROR_PROJECT//,/ })
	# 判断project是否有重复
	for error_index in "${!error_array[@]}"; do
		if [[ ${error_array[error_index]} == $PROJECT_NEME ]]; then
			IS_REPEAT=1
		fi
	done
	# project重复则跳过第二个
	if [ $IS_REPEAT -eq 1 ];then
		IS_REPEAT=0
		continue
	fi
	checkCode
	clearVar
done < temp.txt

if [[ $ERROR_PROJECT == "" ]]
then
	echo "SDK already contains all commits."
fi

if [ -f current_manifest.xml ]
then
	rm current_manifest.xml
fi

if [ -f temp.txt ]; then
	rm temp.txt
fi
