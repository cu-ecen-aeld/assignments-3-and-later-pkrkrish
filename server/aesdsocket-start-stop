#!/bin/sh

# Path to your executable and the data file
DAEMON=/usr/bin/aesdsocket
DAEMON_ARGS="-d"
NAME=aesdsocket
DESC="AESD Socket Daemon"

case "$1" in
  start)
	echo "Starting $DESC: $NAME"
	# --start: Start the daemon
	# --exec: Path to the compiled binary
	# --args: Passes the -d flag to your C program
	start-stop-daemon --start --oknodo --exec $DAEMON -- $DAEMON_ARGS
	;;
  stop)
	echo "Stopping $DESC: $NAME"
	# --stop: Sends SIGTERM by default
	# --retry: Wait for process to die, then force if necessary
	start-stop-daemon --stop --oknodo --retry TERM/5/KILL/1 --name $NAME
	;;
  restart)
	$0 stop
	$0 start
	;;
  *)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

exit 0
