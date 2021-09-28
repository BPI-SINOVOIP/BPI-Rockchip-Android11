# To trigger a reboot on install of version 300000000 of
# the com.android.tzdata apex:
# $ adb shell setprop persist.debug.trigger_reboot_after_activation com.android.tzdata@300000000.apex
active_apex=/data/apex/active/`/system/bin/getprop persist.debug.trigger_reboot_after_activation`
if [[ $active_apex == *.apex ]]
then
    while :
    do
        if [ -a $active_apex ]
        then
            /system/bin/log -t TriggerWatchdog "Detected presence of $active_apex"
            /system/bin/setprop sys.powerctl reboot
        fi
    done
fi