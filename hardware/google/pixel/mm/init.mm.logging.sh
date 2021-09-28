#!/vendor/bin/sh

if [ $# -eq 1 ]; then
    interval=$1
else
    exit 1
fi

kcompactd_pid=`pidof -x kcompactd0`
kswapd_pid=`pidof -x kswapd0`

while true
do
    log_time=`date '+%m-%d-%H-%M-%S'`

    log_vmstat=`cat /proc/vmstat`
    log_kcompactd=`cat /proc/$kcompactd_pid/stat`
    log_kswapd=`cat /proc/$kswapd_pid/stat`
    log_stat=`cat /proc/stat`

    log_line="$log_time $log_vmstat"
    echo $log_line >> /data/vendor/mm/vmstat/log

    log_line="$log_time $log_kcompactd"
    echo $log_line >> /data/vendor/mm/kcompactd/log

    log_line="$log_time $log_kswapd"
    echo $log_line >> /data/vendor/mm/kswapd/log

    log_line="$log_time $log_stat"
    echo $log_line >> /data/vendor/mm/stat/log

    sleep $interval
done
