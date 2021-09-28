#!/bin/bash
#
# Build the CDD HTML and PDF from the source files.
# From the root directory run:
#   ./cdd_gen.sh --version xx --branch xx
#
# where version is the version number and branch is the name of the AOSP branch.
#
# To run this script, you must install these packages as shown:
#   sudo apt-get install wkhtmltopdf
#   pip install Jinja2
#   pip install markdown
#   pip install pytidylib
#

positional=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -v|--version)
    version="$2"
    shift # past argument
    shift # past value
    ;;
    -b|--branch)
    branch="$2"
    shift # past argument
    shift # past value
    ;;
    --default)
    default=YES
    shift # past argument
    ;;
    *)    # unknown option
    positional+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${positional[@]}" # restore positional parameters

if  [ -z "${version}" ];
then
  read -p "Version number: " version
fi

if  [ -z "${branch+x}" ];
then
  read -p "AOSP branch name for revision history: " branch
fi

echo "version = ${version}"
echo "branch = ${branch}"

current_date=$(date "+%m-%d")
echo "Current Date : $current_date"

filename="android-${version}-cdd-${current_date}"
echo "$filename"


python make_cdd.py --version $version --branch $branch --output $filename;

mkdir -p /tmp/$filename

wkhtmltopdf -B 1in -T 1in -L .75in -R .75in page $filename.html --footer-html source/android-cdd-footer.html /tmp/$filename/$filename-body.pdf
wkhtmltopdf -s letter -B 0in -T 0in -L 0in -R 0in cover source/android-cdd-cover.html /tmp/$filename/$filename-cover.pdf


mv $filename.html /tmp/$filename
mv $filename-devsite.html /tmp/$filename

echo ""
echo "The generated files have been placed in the /tmp/$filename directory."
echo "Please copy them to your Google Drive or another more permanent location."
echo ""
