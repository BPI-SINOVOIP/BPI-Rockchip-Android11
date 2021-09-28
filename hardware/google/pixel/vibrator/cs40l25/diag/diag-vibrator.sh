#!/vendor/bin/sh

set -e

do_id()
{
    cat /sys/class/leds/vibrator/device/modalias | sed 's/^i2c://g'
}

do_ping()
{
    test "$(do_id)" == "cs40l25a"
}

do_enable()
{
    /system/bin/idlcli vibrator on "${@}"
}

do_disable()
{
    /system/bin/idlcli vibrator off
}

do_state()
{
    local state="$(cat /sys/class/leds/vibrator/device/vibe_state)"
    if [[ "${state}" == "0" ]]
    then
        echo "stopped"
    else
        echo "running"
    fi
}

do_dump()
{
    local loc="$(basename /sys/class/leds/vibrator/device/driver/*-0043)"
    cat /sys/kernel/debug/regmap/${loc}/registers
}

do_help()
{
    local name="$(basename "${0}")"
    echo "Usage:"
    echo "  ${name} id          - Prints controller ID"
    echo "  ${name} ping        - Verifies probe succedded"
    echo "  ${name} enable <ms> - Enables vibrator for <ms> milliseconds"
    echo "  ${name} disable     - Disables vibrator."
    echo "  ${name} state       - Returns 'stopped' or 'running' state."
    echo "  ${name} dump        - Dumps memory mapped registers."
}

if [[ "${#}" -gt "0" ]]
then
    cmd="do_${1}" && shift
fi

if ! typeset -f "${cmd}" >/dev/null
then
    cmd="do_help"
fi

exec "${cmd}" "${@}"
