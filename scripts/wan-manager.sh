#!/bin/sh

case $1 in
    start|boot)
	amxrt -D /etc/amx/wan-manager/wan-manager.odl
        ;;
    stop)
        if [ -f /var/run/wan-manager.pid ]; then
            kill `cat /var/run/wan-manager.pid`
        fi
        ;;
    debuginfo)
	echo "TODO debuginfo"
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
