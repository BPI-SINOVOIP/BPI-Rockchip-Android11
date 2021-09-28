#!/bin/bash

set -e

readonly DEST_REPO="$(dirname $(readlink -f "$0"))/../.."
readonly INPUT_REPO="m2repository"
readonly STAGE_REPO="m2staged"
readonly DEST_ANDROID_REPO="${DEST_REPO}/prebuilts/manifest-merger"

if ! TEMP_DIR=$(mktemp -d); then
    echo "ERROR: failed to create dir"
    exit 1
fi
readonly TEMP_DIR

function cleanup() {
    rm -rf "${TEMP_DIR}"
}

trap cleanup EXIT

cd "${TEMP_DIR}"

function usage() {
  cat <<EOF
Usage: $(basename $0) group:artifact:version[:classifier][@extension] [group:artifact:version[:classifier][@extension]...]

Downloads the specified artifacts into the appropriate subdirectory of ${DEST_REPO}/prebuilts

EOF
  exit 1
}

# usage: downloadArtifacts "$group:$artifact:$version[:classifier][@extension]..."
function downloadArtifacts() {
  [ -z "$1" ] && usage

  echo "downloading dependencies into ${INPUT_REPO}"
  rm -rf "${INPUT_REPO}"
  for arg in $*; do
    echo "importing ${arg}"
    IFS=@ read -r dependency extension <<< "${arg}"
    IFS=: read -ra FIELDS <<< "${dependency}"
    local -r groupId="${FIELDS[0]}"
    local -r artifactId="${FIELDS[1]}"
    local -r version="${FIELDS[2]}"
    local -r classifier="${FIELDS[3]}"

    # download the actual artifact
    downloadArtifact "${groupId}" "${artifactId}" "${version}" "${classifier}" "${extension}"

    # try to download the sources jar
    downloadArtifact "${groupId}" "${artifactId}" "${version}" "sources" "jar" || true

    # go to next artifact
  done
  echo "done downloading dependencies"
}

# usage: generatePomFile "$pomFile" "$group" "$artifact" "$version" "$classifier" "$extension"
function generatePomFile() {
  local -r pomFile="$1"
  local -r groupId="$2"
  local -r artifactId="$3"
  local -r version="$4"
  local -r classifier="$5"
  local -r extension="$6"

  local pomDependencies=""

  echo "creating ${pomFile}"
  cat <<EOF > "${pomFile}"
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0" xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 http://maven.apache.org/xsd/maven-4.0.0.xsd" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>com.google.android.build</groupId>
  <artifactId>m2repository</artifactId>
  <version>1.0</version>
  <repositories>
    <repository>
      <id>google</id>
      <name>Google</name>
      <url>https://maven.google.com</url>
    </repository>
    <repository>
      <id>jcenter</id>
      <name>JCenter</name>
      <url>https://jcenter.bintray.com</url>
    </repository>
  </repositories>
  <dependencies>
    <dependency>
       <groupId>${groupId}</groupId>
       <artifactId>${artifactId}</artifactId>
       <version>${version}</version>
EOF

if [ -n "${classifier}" ]; then
   cat <<EOF >> "${pomFile}"
       <classifier>${classifier}</classifier>
EOF
fi

if [ -n "${extension}" ]; then
  cat <<EOF >> "${pomFile}"
       <type>${extension}</type>
EOF
fi

cat <<EOF >> "${pomFile}"
    </dependency>
  </dependencies>
  <build>
        <plugins>
            <plugin>
                <groupId>org.apache.maven.plugins</groupId>
                <artifactId>maven-dependency-plugin</artifactId>
                <version>2.8</version>
                <executions>
                    <execution>
                        <id>default-cli</id>
                        <configuration>
                            <includeScope>runtime</includeScope>
                            <addParentPoms>true</addParentPoms>
                            <copyPom>true</copyPom>
                            <useRepositoryLayout>true</useRepositoryLayout>
                            <outputDirectory>m2repository</outputDirectory>
                        </configuration>
                    </execution>
                </executions>
            </plugin>
        </plugins>
    </build>
</project>
EOF

  echo "done creating ${pomFile}"
}

# usage: downloadArtifact "$group" "$artifact" "$version" "$classifier" "$extension"
function downloadArtifact() {
  local -r pomFile="${PWD}/pom.xml"

  generatePomFile "${pomFile}" "$@"

  echo "downloading one dependency into ${INPUT_REPO}"
  mvn -f "${pomFile}" dependency:copy-dependencies
  echo "done downloading one dependency into ${INPUT_REPO}"
}

# generates an appropriately formatted repository for merging into existing repositories,
# by computing artifact metadata
function createStageRepo() (
  echo "staging to ${STAGE_REPO}"
  rm -rf "${STAGE_REPO}"

  for f in $(find "${INPUT_REPO}" -type f ! -name "*.sha1" ! -name "*.md5"); do
      local relPath="${f##${INPUT_REPO}}"
      local relDir=$(dirname "${relPath}")

      local fileName=$(basename "${relPath}")
      local writeChecksums=true

      local destDir="${STAGE_REPO}/${relDir}"
      local destFile="${STAGE_REPO}/${relPath}"
      if [ "${fileName}" == "maven-metadata-local.xml" ]; then
        writeChecksums=false
        destFile="${destDir}/maven-metadata.xml"
      fi

      mkdir -p "${destDir}"
      if ${writeChecksums}; then
        local md5=$(md5sum "${f}" | sed 's/ .*//')
        local sha1=$(sha1sum "${f}" | sed 's/ .*//')
        echo -n ${md5} > "${destFile}.md5"
        echo -n ${sha1} > "${destFile}.sha1"
      fi
      cp "${f}" "${destFile}"
  done

  echo "done staging to ${STAGE_REPO}"
)

function announceCopy() {
  local -r input="$1"
  local -r output="$2"
  if [ -e "${input}" ]; then
    echo "copying '${input}' to '${output}'"
    cp -rT "${input}" "${output}"
  fi
}

function exportArtifact() {
  echo "exporting"

  mkdir -p "${DEST_ANDROID_REPO}/com/android"
  announceCopy "${STAGE_REPO}/com/android" "${DEST_ANDROID_REPO}/com/android"
  rm -rf "${STAGE_REPO}/com/android"

  echo "done exporting"
}


function main() {
  downloadArtifacts "$@"
  createStageRepo
  exportArtifact
}

main "$@"
