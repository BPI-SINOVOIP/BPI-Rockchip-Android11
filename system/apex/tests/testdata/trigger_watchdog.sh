# To trigger native watchdog on install of version 300000000 of
# the com.android.tzdata apex:
# $ adb shell setprop persist.debug.trigger_watchdog.apex com.android.tzdata@300000000
apex=/apex/`/system/bin/getprop persist.debug.trigger_watchdog.apex`
/system/bin/log -t TriggerWatchdog "Checking for presence of $apex"
/system/bin/setprop debug.trigger_watchdog.status check
if [ -a $apex ]
then
    /system/bin/log -t TriggerWatchdog "Detected presence of $apex"
    /system/bin/log -t TriggerWatchdog "KILLING SYSTEM SERVER"
    /system/bin/setprop debug.trigger_watchdog.status kill
    while :
    do
        pid=`pidof system_server`
        if [ ! -z "$pid" ]
        then
            /system/bin/log -t TriggerWatchdog "Killing system_server pid=$pid ..."
            kill $pid
        fi
        sleep 1
    done
fi
