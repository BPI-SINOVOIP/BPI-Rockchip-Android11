#!/system/bin/sh

configDstPath=/data/local/chrome-command-line
configSrcPath=/etc/chrome-command-line

# check screen shot folder
if [ ! -f $configDstPath ]; then
  busybox cp $configSrcPath $configDstPath
  chmod 777 $configDstPath
fi

