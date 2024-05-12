#!/bin/sh

# Start and stop aesdsocket in daemon mode.
case "$1" in 
	start)
		echo  -n "Starting aesdsocket daemon "
		start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d && echo " -  Sucessfully started" || echo " -  Failed to start"
		;;
	stop)
		echo -n "Stopping aesdsocket daemon "
		start-stop-daemon -K -n aesdsocket -a /usr/bin/aesdsocket && echo " -  Sucessfully stopped" || echo " -  Failed to stop"
		;;
	*)
		echo "Usage $0 {start|stop}"
	exit 1
esac
