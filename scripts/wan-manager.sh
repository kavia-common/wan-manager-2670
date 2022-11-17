#!/bin/sh
ulimit -c unlimited
name="wan-manager"
name_pid="`pgrep ${name}`"

case $1 in
    start|boot)
        if [ -z "${name_pid}" ]; then
            ${name} -D
        fi
        ;;
    stop)
        if [ -n "${name_pid}" ]; then
            kill ${name_pid}
        fi
        ;;
    debuginfo)
	ubus-cli "X_PRPL-COM_WANManager.?"
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    log)
	echo "TODO log wan-manager client"
	;;
    *)
        echo "Usage : $0 [start|boot|stop|debuginfo|log]"
        ;;
esac
