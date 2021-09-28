#!/bin/bash

./gradlew :trebuchet:startup-summarizer:run --args="$(echo $@)"