#!/system/bin/sh

smb_processid=$(ps | busybox awk '$9=="/sbin/rksmbd" {print $2;exit;}')
echo "get smb process id = ${smb_processid}"
kill $smb_processid

