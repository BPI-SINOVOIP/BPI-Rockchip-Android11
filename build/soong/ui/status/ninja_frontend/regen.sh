#!/bin/bash

aprotoc --go_out=paths=source_relative:. frontend.proto
