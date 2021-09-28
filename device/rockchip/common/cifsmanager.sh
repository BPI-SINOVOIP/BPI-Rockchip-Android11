#!/bin/sh
user=`getprop sys.cifsmanager.user`
password=`getprop sys.cifsmanager.pwd`
mountpoint=`getprop sys.cifsmanager.mountpoint`
path=`getprop sys.cifsmanager.path`

echo $user
echo $password
echo $mountpoint
echo $path

#if [ ! -d ${mountpoint} ];then
#    mkdir -p  ${mountpoint};
#    echo 'create mountpoint'
#else
#    echo 'mountpoint already exists.'
#fi

if [ "${user}" = "guest" ];then
    echo "is guest"
    mount -t cifs ${path} -o username=${user},uid=1000,iocharset=utf8,gid=1015,file_mode=0775,dir_mode=0775,rw ${mountpoint}
else
    echo "is not guest"
    mount -t cifs ${path} -o username=${user},password=${password},iocharset=utf8,uid=1000,gid=1015,file_mode=0775,dir_mode=0775,rw ${mountpoint}
fi
