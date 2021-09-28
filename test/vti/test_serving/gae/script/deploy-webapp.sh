#!/bin/bash
#
# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [ "$#" -lt 1 ]; then
  echo "usage: deploy-webapp.sh prod|test|public|local [deploy options]"
  exit 1
fi

NPM_PATH=$(which npm)
NG_PATH=$(which ng)
if [ ! -f "${NPM_PATH}" ]; then
  echo "Cannot find npm in your PATH."
  echo "Please install node.js and npm to deploy frontend."
  exit 0
fi
if [ ! -f "${NG_PATH}" ]; then
  echo "Cannot find Angular CLI in your PATH."
  echo "Please install Angular CLI to deploy frontend."
  exit 0
fi

pushd frontend
echo "Installing frontend dependencies..."
npm install

echo "Removing files in dist directory..."
rm -r dist/*

echo "Building frontend codes..."
if [ $1 = "local" ]; then
  ng build
else
  ng build --prod
fi
popd

echo "Copying frontend files to webapp/static directory..."
rm -rf webapp/static/
mkdir webapp/static
cp -r frontend/dist/* webapp/static/

if [ $1 = "public" ]; then
  SERVICE="vtslab-schedule"
elif [ $1 = "local" ]; then
  dev_appserver.py ./
  exit 0
else
  SERVICE="vtslab-schedule-$1"
fi

echo "Fetching endpoints service version of $SERVICE ..."
ENDPOINTS=$(gcloud endpoints configs list --service=$SERVICE.appspot.com)
arr=($ENDPOINTS)

if [ ${#arr[@]} -lt 4 ]; then
  echo "You need to deploy endpoints first."
  exit 0
else
  VERSION=${arr[2]}
  NAME=${arr[3]}
  echo "ENDPOINTS_SERVICE_NAME: $NAME"
  echo "ENDPOINTS_SERVICE_VERSION: $VERSION"
fi

echo "Updating app.yaml ..."
if [ "$(uname)" == "Darwin" ]; then
  sed -i "" "s/ENDPOINTS_SERVICE_NAME:.*/ENDPOINTS_SERVICE_NAME: $NAME/g" app.yaml
  sed -i "" "s/ENDPOINTS_SERVICE_VERSION:.*/ENDPOINTS_SERVICE_VERSION: $VERSION/g" app.yaml
else
  sed -i "s/ENDPOINTS_SERVICE_NAME:.*/ENDPOINTS_SERVICE_NAME: $NAME/g" app.yaml
  sed -i "s/ENDPOINTS_SERVICE_VERSION:.*/ENDPOINTS_SERVICE_VERSION: $VERSION/g" app.yaml
fi

echo "Deploying the web app to $SERVICE ..."

gcloud app deploy app.yaml cron.yaml index.yaml queue.yaml worker.yaml --project=$SERVICE ${@:2}

echo "Deployment done!"
