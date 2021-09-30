set -e


destRepo="$(cd $(dirname $0)/../.. && pwd)"
tempDir="/tmp/import-temp-work"
rm -rf $tempDir
mkdir -p $tempDir
cd $tempDir

function usage() {
  echo "Usage: $0 group:artifact:version[:classifier][@extension] [group:artifact:version[:classifier][@extension]...]

This script downloads the specified artifacts copies them into the appropriate subdirectory of $destRepo/prebuilts/"
  exit 1
}




inputRepo=m2repository
stageRepo=m2staged
destAndroidRepo=$destRepo/prebuilts/gradle-plugin
destThirdPartyRepo=$destRepo/prebuilts/tools/common/m2/repository


# usage: downloadArtifacts "$group:$artifact:$version[:classifier][@extension]..."
function downloadArtifacts() {
  if [ "$1" == "" ]; then
    usage
  fi
  echo downloading dependencies into $inputRepo
  rm -rf $inputRepo
  while [ "$1" != "" ]; do
    echo importing $1
    IFS=@ read -r dependency extension <<< "$1"
    IFS=: read -ra FIELDS <<< "${dependency}"
    groupId="${FIELDS[0]}"
    artifactId="${FIELDS[1]}"
    version="${FIELDS[2]}"
    classifier="${FIELDS[3]}"

    # download the actual artifact
    downloadArtifact "$groupId" "$artifactId" "$version" "$classifier" "$extension"

    # try to download the sources jar
    downloadArtifact "$groupId" "$artifactId" "$version" "sources" "jar" || true

    # go to next artifact
    shift
  done
  echo done downloading dependencies
}

# usage: downloadArtifact "$group" "$artifact" "$version" "$classifier" "$extension"
function downloadArtifact() {
  pomPath="$PWD/pom.xml"
  echo creating $pomPath
  pomPrefix='<?xml version="1.0" encoding="UTF-8"?>
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
'

  pomSuffix='
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
'


  groupId="$1"
  artifactId="$2"
  version="$3"
  classifier="$4"
  extension="$5"
  pomDependencies=""


  dependencyText=$(echo -e "\n    <dependency>\n      <groupId>${groupId}</groupId>\n      <artifactId>${artifactId}</artifactId>\n      <version>${version}</version>")
  [ $classifier ] && dependencyText+=$(echo -e "\n      <classifier>${classifier}</classifier>")
  [ $extension ] && dependencyText+=$(echo -e "\n      <type>${extension}</type>")
  dependencyText+=$(echo -e "\n    </dependency>")


  pomDependencies="${pomDependencies}${dependencyText}"

  echo "${pomPrefix}${pomDependencies}${pomSuffix}" > $pomPath
  echo done creating $pomPath

  echo downloading one dependency into $inputRepo
  mvn -f "$pomPath" dependency:copy-dependencies
  echo done downloading one dependency into $inputRepo
}

# generates an appropriately formatted repository for merging into existing repositories,
# by computing artifact metadata
function stageRepo() {
  echo staging to $stageRepo
  rm -rf $stageRepo
  
  for f in $(find $inputRepo -type f | grep -v '\.sha1$' | grep -v '\.md5'); do
      md5=$(md5sum $f | sed 's/ .*//')
      sha1=$(sha1sum $f | sed 's/ .*//')
      relPath=$(echo $f | sed "s|$inputRepo/||")
      relDir=$(dirname $relPath)
  
      fileName=$(basename $relPath)
      writeChecksums="true"
  
      destDir="$stageRepo/$relDir"
      destFile="$stageRepo/$relPath"
      if [ "$fileName" == "maven-metadata-local.xml" ]; then
        writeChecksums="false"
        destFile="$destDir/maven-metadata.xml"
      fi
  
      mkdir -p $destDir
      if [ "$writeChecksums" == "true" ]; then
        echo -n $md5 > "${destFile}.md5"
        echo -n $sha1 > "${destFile}.sha1"
      fi
      cp $f $destFile
  done
  
  echo done staging to $stageRepo
}

function announceCopy() {
  input=$1
  output=$2
  if stat $input > /dev/null 2>/dev/null; then
    echo copying "$input" to "$output"
    cp -rT $input $output
  fi
}

function exportArtifact() {
  echo exporting
  announceCopy $stageRepo/com/android $destAndroidRepo/com/android
  rm -rf $stageRepo/com/android

  announceCopy $stageRepo/androidx $destAndroidRepo/androidx
  rm -rf $stageRepo/androidx

  announceCopy $stageRepo $destThirdPartyRepo
  echo done exporting
}


function main() {
  downloadArtifacts "$@"
  stageRepo
  exportArtifact
}

main "$@"
