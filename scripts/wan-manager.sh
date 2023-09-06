#!/bin/sh
[ -f /etc/environment ] && source /etc/environment
ulimit -c ${ULIMIT_CONFIGURATION:-0}
name="wan-manager"

case $1 in
    start|boot)
        ${name} -D
        ;;
    stop|shutdown)
        if [ -f /var/run/${name}.pid ]; then
            kill `cat /var/run/${name}.pid`
        else
            killall ${name}
        fi
        ;;
    debuginfo)
        ubus-cli "WANManager.?"
        ;;
    restart)
        $0 stop
        sleep 1s
        $0 start
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|shutdown|debuginfo|restart]"
        ;;
esac
