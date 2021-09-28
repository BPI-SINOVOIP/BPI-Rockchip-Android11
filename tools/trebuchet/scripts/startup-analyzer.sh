#!/bin/bash

./gradlew :trebuchet:startup-analyzer:run --args="$(echo $@)"