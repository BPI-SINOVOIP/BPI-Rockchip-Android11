#! /vendor/bin/sh
cd /
pkill rkaiq_tool_server
rm -f /dev/socket/camera_tool
stop cameraserver
start cameraserver
pkill provider* 
pkill camera*
