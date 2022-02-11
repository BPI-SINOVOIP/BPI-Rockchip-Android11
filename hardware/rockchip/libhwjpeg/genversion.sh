#!/bin/sh

cur_dir=`dirname $0`
cd $cur_dir

# check for git short hash
if ! test "$revision"; then
    revision=$(git describe --tags --match N 2> /dev/null)
fi

# Shallow Git clones (--depth) do not have the N tag:
# use 'git-YYYY-MM-DD-hhhhhhh'.
test "$revision" || revision=$(git log -1 --date=short --pretty=format:"git-%h author: %an %cd %s")

# no revision number found
test "$revision" || revision=$(cat RELEASE 2> /dev/null)

# Append the Git hash if we have one
test "$revision" && test "$git_hash" && revision="$revision-$git_hash"

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)
test "$version" || version=$revision

test -n "$3" && version=$version-$3

if [ -z "$2" ]; then
    echo "$version"
fi

# releases extract the version number from the VERSION file
version=$(cat VERSION 2> /dev/null)
test "$version" || version=$revision

# query compiling date
build_date=`date +"%Y-%m-%d %H:%M:%S"`

# query compiling author
build_name=`cat ~/.gitconfig | sed -n "s/^\(.*\)name = \(.*\)/\2/p" | cut -c 1-3`

DST_VERSION_FILE=`pwd`/src/version.h

sed -i "/^#define *HWJPEG_VERSION_INFO   */s/\(#define *HWJPEG_VERSION_INFO   *\)\(\".*\"\)/\1\"${version}\"/" ${DST_VERSION_FILE}
sed -i "/^#define *HWJPEG_BUILD_INFO  */s/\(#define *HWJPEG_BUILD_INFO  *\)\(\".*\"\)/\1\"built-${build_name} ${build_date}\"/" ${DST_VERSION_FILE}

